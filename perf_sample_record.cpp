#include "perf_sample_record.h"
#include "util.h"
#include <cstring>
namespace event_engine
{
    bool SampleReadFormat::Read(char *data_ptr, const int total_size, int &offset, uint64_t format)
    {
        uint64_t nr_or_value;

        if (!SafeMemcpy(&nr_or_value, data_ptr, sizeof(nr_or_value), offset, total_size))
        {
            return false;
        }

        if ((format & PERF_FORMAT_TOTAL_TIME_ENABLED) && (!SafeMemcpy(&time_enabled_, data_ptr, sizeof(time_enabled_), offset, total_size)))
        {
            return false;
        }

        if ((format & PERF_FORMAT_TOTAL_TIME_RUNNING) && (!SafeMemcpy(&time_running_, data_ptr, sizeof(time_running_), offset, total_size)))
        {
            return false;
        }

        if (format & PERF_FORMAT_GROUP)
        {
            for (int i = 0; i < nr_or_value; i++)
            {
                value v;
                if (!SafeMemcpy(&v.value_, data_ptr, sizeof(v.value_), offset, total_size))
                {
                    return false;
                }
                if ((format & PERF_FORMAT_ID) && !SafeMemcpy(&v.id_, data_ptr, sizeof(v.id_), offset, total_size))
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
            if ((format & PERF_FORMAT_ID) && !SafeMemcpy(&v.id_, data_ptr, sizeof(v.id_), offset, total_size))
            {
                return false;
            }
            values_.push_back(v);
        }
        return true;
    }

    bool PerfSampleRecord::Read(char *data_ptr, const int total_size, int &offset, std::map<uint64_t, perf_event_attr> attr_map)
    {
        int sample_end;
        std::map<uint64_t, perf_event_attr>::iterator it;
        perf_event_attr attr;

        if (!SafeMemcpy(&header_, data_ptr, sizeof(header_), offset, total_size))
        {
            return false;
        }

        sample_end = offset + header_.size - sizeof(header_);

        if (!SafeMemcpy(&sample_id_, data_ptr, sizeof(sample_id_), offset, total_size))
        {
            goto fail_end;
        }

        it = attr_map.find(sample_id_);

        if (it == attr_map.end())
        {
            goto fail_end;
        }

        attr = it->second;

        if ((attr.sample_type & PERF_SAMPLE_IP) && (!SafeMemcpy(&ip_, data_ptr, sizeof(ip_), offset, total_size)))
        {
            goto fail_end;
        }

        if ((attr.sample_type & PERF_SAMPLE_TID) && ((!SafeMemcpy(&pid_, data_ptr, sizeof(pid_), offset, total_size)) || (!SafeMemcpy(&tid_, data_ptr, sizeof(tid_), offset, total_size))))
        {
            goto fail_end;
        }

        if ((attr.sample_type & PERF_SAMPLE_TIME) && (!SafeMemcpy(&time_, data_ptr, sizeof(time_), offset, total_size)))
        {
            goto fail_end;
        }

        if ((attr.sample_type & PERF_SAMPLE_ADDR) && (!SafeMemcpy(&addr_, data_ptr, sizeof(addr_), offset, total_size)))
        {
            goto fail_end;
        }

        if ((attr.sample_type & PERF_SAMPLE_ID) && (!SafeMemcpy(&id_, data_ptr, sizeof(id_), offset, total_size)))
        {
            goto fail_end;
        }

        if ((attr.sample_type & PERF_SAMPLE_CPU) && (!SafeMemcpy(&cpu_, data_ptr, sizeof(cpu_), offset, total_size)))
        {
            goto fail_end;
            uint32_t res{0};
            if (!SafeMemcpy(&res, data_ptr, sizeof(res), offset, total_size))
            {
                goto fail_end;
            }
        }

        if ((attr.sample_type & PERF_SAMPLE_PERIOD) && (!SafeMemcpy(&period_, data_ptr, sizeof(period_), offset, total_size)))
        {
            goto fail_end;
        }

        if ((attr.sample_type & PERF_SAMPLE_READ) && read_format_.Read(data_ptr, total_size, offset, attr.read_format))
        {
            goto fail_end;
        }

        if (attr.sample_type & PERF_SAMPLE_CALLCHAIN)
        {
            uint64_t nr{0};
            if (!SafeMemcpy(&nr, data_ptr, sizeof(nr), offset, total_size))
            {
                goto fail_end;
            }
            for (uint64_t i = 0; i < nr; i++)
            {
                uint64_t ip{0};
                if (!SafeMemcpy(&ip, data_ptr, sizeof(ip), offset, total_size))
                {
                    goto fail_end;
                }
                ips_.push_back(ip);
            }
        }

        if ((attr.sample_type & PERF_SAMPLE_RAW) && !(SafeMemcpy(&raw_data_size_, data_ptr, sizeof(raw_data_size_), offset, total_size)))
        {
            goto fail_end;
        }

        if (offset + raw_data_size_ > sample_end)
        {
            goto fail_end;
        }

        raw_data_ = data_ptr + offset;

        offset += raw_data_size_;

        if (attr.sample_type & PERF_SAMPLE_BRANCH_STACK)
        {
            uint64_t nr{0};
            if (!SafeMemcpy(&nr, data_ptr, sizeof(nr), offset, total_size))
            {
                goto fail_end;
            }

            for (uint64_t i = 0; i < nr; i++)
            {
                std::tuple<uint64_t, uint64_t, uint64_t> pbe;
                if (!SafeMemcpy(&pbe, data_ptr, sizeof(pbe), offset, total_size))
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

        offset = sample_end;
        return true;

    fail_end:
        offset = sample_end;
        return false;
    };
}