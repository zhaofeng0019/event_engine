#include "perf_sample_record.h"
#include "comm_func.h"
#include <cstring>
namespace event_engine
{
    bool SampleReadFormat::Read(RawStream &raw_stream, uint64_t format)
    {
        uint64_t nr_or_value;

        if (!raw_stream.Read(&nr_or_value, sizeof(nr_or_value)))
        {
            return false;
        }

        if ((format & PERF_FORMAT_TOTAL_TIME_ENABLED) && (!raw_stream.Read(&time_enabled_, sizeof(time_enabled_))))
        {
            return false;
        }

        if ((format & PERF_FORMAT_TOTAL_TIME_RUNNING) && (!raw_stream.Read(&time_running_, sizeof(time_running_))))
        {
            return false;
        }

        if (format & PERF_FORMAT_GROUP)
        {
            for (int i = 0; i < nr_or_value; i++)
            {
                value v;
                if (!raw_stream.Read(&v.value_, sizeof(v.value_)))
                {
                    return false;
                }
                if ((format & PERF_FORMAT_ID) && !raw_stream.Read(&v.id_, sizeof(v.id_)))
                {
                    return false;
                }
                values_.push_back(v);
            }
        }
        else
        {
            value v;
            v.value_ = nr_or_value;
            if ((format & PERF_FORMAT_ID) && !raw_stream.Read(&v.id_, sizeof(v.id_)))
            {
                return false;
            }
            values_.push_back(v);
        }
        return true;
    }

    void Decoder::DecoderFromEvent(std::string group, std::string name, std::function<void(void *)> fn, std::string &err)
    {
        fields_ = GetTraceEventFormat(group, name, err);
        handle_ = fn;
    }

    bool PerfSampleRecord::Read(RawStream &raw_stream, std::unordered_map<uint64_t, perf_event_attr> attr_map, perf_event_attr *default_attr, std::unordered_map<uint64_t, Decoder> decoder_map)
    {
        int sample_end;
        std::unordered_map<uint64_t, perf_event_attr>::iterator it;
        perf_event_attr attr;
        if (!raw_stream.Read(&header_, sizeof(header_)))
        {
            return false;
        }
        sample_end = raw_stream.GetOffset() + header_.size - sizeof(header_);

        if (default_attr != nullptr)
        {
            attr = *default_attr;
            if ((attr.sample_type & PERF_SAMPLE_IDENTIFIER) && !raw_stream.Read(&sample_id_, sizeof(sample_id_)))
            {
                goto fail_end;
            }
        }
        else
        {
            if (!raw_stream.Read(&sample_id_, sizeof(sample_id_)))
            {
                goto fail_end;
            }
            it = attr_map.find(sample_id_);

            if (it == attr_map.end())
            {
                goto fail_end;
            }
            attr = it->second;
            if (!(attr.sample_type & PERF_SAMPLE_IDENTIFIER))
            {
                goto fail_end;
            }
        }

        if ((attr.sample_type & PERF_SAMPLE_IP) && (!raw_stream.Read(&ip_, sizeof(ip_))))
        {
            goto fail_end;
        }

        if ((attr.sample_type & PERF_SAMPLE_TID) && ((!raw_stream.Read(&pid_, sizeof(pid_))) || (!raw_stream.Read(&tid_, sizeof(tid_)))))
        {
            goto fail_end;
        }

        if ((attr.sample_type & PERF_SAMPLE_TIME) && (!raw_stream.Read(&time_, sizeof(time_))))
        {
            goto fail_end;
        }

        if ((attr.sample_type & PERF_SAMPLE_ADDR) && (!raw_stream.Read(&addr_, sizeof(addr_))))
        {
            goto fail_end;
        }

        if ((attr.sample_type & PERF_SAMPLE_ID) && (!raw_stream.Read(&id_, sizeof(id_))))
        {
            goto fail_end;
        }

        if ((attr.sample_type & PERF_SAMPLE_STREAM_ID) && (!raw_stream.Read(&stream_id_, sizeof(stream_id_))))
        {
            goto fail_end;
        }

        if ((attr.sample_type & PERF_SAMPLE_CPU) && (!raw_stream.Read(&cpu_, sizeof(cpu_))))
        {
            goto fail_end;
            uint32_t res{0};
            if (!raw_stream.Read(&res, sizeof(res)))
            {
                goto fail_end;
            }
        }

        if ((attr.sample_type & PERF_SAMPLE_PERIOD) && (!raw_stream.Read(&period_, sizeof(period_))))
        {
            goto fail_end;
        }

        if ((attr.sample_type & PERF_SAMPLE_READ) && read_format_.Read(raw_stream, attr.read_format))
        {
            goto fail_end;
        }

        if (attr.sample_type & PERF_SAMPLE_CALLCHAIN)
        {
            uint64_t nr{0};
            if (!raw_stream.Read(&nr, sizeof(nr)))
            {
                goto fail_end;
            }
            for (uint64_t i = 0; i < nr; i++)
            {
                uint64_t ip{0};
                if (!raw_stream.Read(&ip, sizeof(ip)))
                {
                    goto fail_end;
                }
                ips_.push_back(ip);
            }
        }

        if ((attr.sample_type & PERF_SAMPLE_RAW))
        {
            if (!raw_stream.Read(&raw_data_size_, sizeof(raw_data_size_)))
            {
                goto fail_end;
            }
            if (raw_stream.GetOffset() - sizeof(raw_data_size_) + raw_data_size_ > sample_end)
            {
                goto fail_end;
            }
            raw_data_ptr_ = raw_stream.GetOffset() + raw_stream.GetPtr();
            raw_stream.SetOffset(raw_stream.GetOffset() + raw_data_size_);
            auto it = decoder_map.find(sample_id_);
            if (it != decoder_map.end())
            {
                raw_data_.Parse(it->second.fields_, raw_data_ptr_, raw_data_size_);
                handle_ = it->second.handle_;
            }
            else
            {
                goto fail_end;
            }
        }

        if (attr.sample_type & PERF_SAMPLE_BRANCH_STACK)
        {
            uint64_t nr{0};
            if (!raw_stream.Read(&nr, sizeof(nr)))
            {
                goto fail_end;
            }

            for (uint64_t i = 0; i < nr; i++)
            {
                std::tuple<uint64_t, uint64_t, uint64_t> pbe;
                if (!raw_stream.Read(&pbe, sizeof(pbe)))
                {
                    goto fail_end;
                }
                BranchEntry entry;
                entry.from_ = std::get<0>(pbe);
                entry.to_ = std::get<1>(pbe);
                uint64_t flag = std::get<2>(pbe);
                entry.mispred_ = ((flag & (1 << 0)) != 0);
                entry.predicted_ = ((flag & (1 << 1)) != 0);
                entry.intx_ = ((flag & (1 << 3)) != 0);
                entry.abort_ = ((flag & (1 << 4)) != 0);
                entry.cycles_ = uint16_t((flag & 0xff0) >> 4);
                branches_.push_back(entry);
            }
        }

        raw_stream.SetOffset(sample_end);
        return true;

    fail_end:
        raw_stream.SetOffset(sample_end);
        return false;
    }

    bool PerfSampleRecord::operator<(const PerfSampleRecord &other) const
    {
        return time_ < other.time_;
    }
}