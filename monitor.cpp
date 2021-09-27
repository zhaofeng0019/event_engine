#include "monitor.h"
#include "comm_func.h"
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <fcntl.h>
#include <chrono>
#include <iostream>
#include <cstdio>
#include <algorithm>
namespace event_engine
{
    const int ATTR_SIZE{64};

    bool Monitor::Init(MonitorOption option, std::string &err)
    {
        loop_err_handle_ = option.loop_err_handle_;
        cpus_ = sysconf(_SC_NPROCESSORS_ONLN);
        if (option.cgroup_.size() != 0)
        {
            /* cgroup */
            if (option.perf_event_dir_ == "")
            {
                option.perf_event_dir_ = PerfEventDir();
            }

            if (option.perf_event_dir_ == "")
            {
                err = "no perf event dir";
                goto fail;
            }

            for (auto cgroup : option.cgroup_)
            {
                std::string path = option.perf_event_dir_ + "/" + cgroup;
                int fd = open(path.c_str(), O_RDONLY, 0);
                if (fd == -1)
                {
                    goto fail;
                }
                cgroup_fd_.push_back(fd);
                if (!InitGroupLeaders(fd, true, option.flag_, option.ring_buff_page_, err))
                {
                    goto fail;
                }
            }
            return true;
        }

        if (option.all_events_)
        {
            option.pids_.clear();
            option.pids_.push_back(-1);
        }
        /* pid */
        for (auto pid : option.pids_)
        {
            if (!InitGroupLeaders(pid, false, option.flag_, option.ring_buff_page_, err))
            {
                goto fail;
            }
        }
        return true;
    fail:
        if (err == "")
        {
            err = errno == 0 ? "monitor init fail" : strerror(errno);
        }
        ClearCgroupAndGroupFd();
        return false;
    }

    bool Monitor::Start(std::string &err)
    {
        stop_fd_ = eventfd(0, 0);
        done_fd_ = eventfd(0, 0);

        poll_fd_ = new pollfd[group_.size() + 1];

        for (int i = 0; i < group_.size(); i++)
        {
            poll_fd_[i].fd = group_[i].group_fd_;
            poll_fd_[i].events = POLLIN;
        }
        poll_fd_[group_.size()].fd = stop_fd_;
        poll_fd_[group_.size()].events = POLLIN;
        is_running_ = true;
        if (!EnableAll(err))
        {
            return false;
        }
        std::thread t(&Monitor::RunLoop, this);
        t.detach();
        return true;
    }

    void Monitor::RunLoop(void)
    {
        while (true)
        {
            int n = poll(poll_fd_, group_.size() + 1, -1);
            if (n == 0)
            {
                continue;
            }
            if (n < 0 && errno != EINTR)
            {
                loop_err_ = strerror(errno);
                goto out_loop;
            }

            if (poll_fd_[group_.size()].revents & POLLIN)
            {
                /*stop eventfd read for stop */
                goto out_loop;
            }

            {
                std::lock_guard<std::mutex> lock(lock_);
                std::vector<PerfSampleRecord> sample_records;
                std::vector<char *> ptrs;
                for (int i = 0; i < group_.size(); i++)
                {
                    if ((poll_fd_[i].revents & (!POLLIN)))
                    {
                        char err_buff[128]{0};
                        std::sprintf(err_buff, "fd %d cpu %d err revents type %d", group_[i].group_fd_, group_[i].cpu, poll_fd_[i].revents);
                        loop_err_ = err_buff;
                        goto out_loop;
                    }

                    if (!(poll_fd_[i].revents & POLLIN))
                    {
                        continue;
                    }
                    auto record_ptrs = group_[i].RingBuffer_.Read();
                    for (auto record_ptr : record_ptrs)
                    {
                        ptrs.push_back(record_ptr.second);
                        RawStream raw_stream(record_ptr.second, record_ptr.first);
                        while (true)
                        {
                            PerfSampleRecord sample_record;
                            if (!sample_record.Read(raw_stream, stream_id_attr_map_, nullptr, stream_id_decoder_map_))
                            {
                                break;
                            }
                            sample_record.time_ += group_[i].RingBuffer_.GetTimeOffset();
                            sample_records.push_back(sample_record);
                        }
                    }
                }
                std::sort(sample_records.begin(), sample_records.end());
                for (auto &sample_record : sample_records)
                {
                    if (sample_record.handle_ != nullptr)
                    {
                        sample_record.handle_(&sample_record); /*ptr are safe  only in handle */
                    }
                }

                for (auto ptr : ptrs)
                {
                    /* release all the buff malloced before in ringbuffer read */
                    delete[] ptr;
                }
            }
        }
    out_loop:
        if (loop_err_ != "" && loop_err_handle_ != nullptr)
        {
            loop_err_handle_(loop_err_);
        }
        delete[] poll_fd_;
        std::string void_err;
        DisableAll(void_err);
        std::vector<uint64_t> probe_ids;
        {
            std::lock_guard<std::mutex> lock(lock_);
            for (auto it : probe_id_fd_map_)
            {
                probe_ids.push_back(it.first);
            }
        }
        for (auto id : probe_ids)
        {
            RemoveProbeEvent(id, void_err);
        }
        ClearCgroupAndGroupFd();
        eventfd_write(done_fd_, 1);
        is_running_ = false;
    }

    void Monitor::Stop()
    {
        if (!is_running_)
        {
            return;
        }
        eventfd_write(stop_fd_, 1);

        eventfd_t value;
        eventfd_read(done_fd_, &value);

        close(stop_fd_);
        close(done_fd_);
    }

    bool Monitor::IsRunning()
    {
        return is_running_;
    }

    bool Monitor::InitGroupLeaders(int pid_or_fd, bool is_fd, unsigned long flag, uint32_t ring_buffer_page, std::string &err)
    {
        if (is_fd)
        {
            flag |= PERF_FLAG_PID_CGROUP;
        }

        perf_event_attr group_attr{0};
        group_attr.type = PERF_TYPE_SOFTWARE;
        group_attr.size = ATTR_SIZE;
        group_attr.config = PERF_COUNT_SW_DUMMY;
        group_attr.disabled = true;
        group_attr.watermark = true;
        group_attr.wakeup_watermark = 1;
        for (int cpu = 0; cpu < cpus_; cpu++)
        {
            int group_fd = OpenPerfEvent(&group_attr, pid_or_fd, cpu, -1, flag, err);
            if (group_fd < 0)
            {
                return false;
            }

            PerfEventGroup perf_group;
            perf_group.pid_or_fd_ = pid_or_fd;
            perf_group.group_fd_ = group_fd;
            perf_group.cpu = cpu;
            perf_group.flag_ = flag;
            RingBuffer ring_buffer(group_fd, ring_buffer_page);
            if (!ring_buffer.Mmap(err))
            {
                close(group_fd);
                return false;
            }
            perf_group.RingBuffer_ = ring_buffer;

            group_.push_back(perf_group);

            if (!CpuTimeOffset(cpu, group_fd, group_[group_.size() - 1].RingBuffer_, err))
            {
                return false;
            }
        }
        return true;
    }

    void Monitor::ClearCgroupAndGroupFd()
    {
        for (auto &group : group_)
        {
            close(group.group_fd_);
            group.RingBuffer_.Unmap();
        }

        group_.clear();

        for (auto fd : cgroup_fd_)
        {
            close(fd);
        }
        cgroup_fd_.clear();
    }

    bool Monitor::CpuTimeOffset(int cpu, int group_fd, RingBuffer &ring_buffer, std::string &err)
    {
        perf_event_attr refer_attr{0};
        refer_attr.type = PERF_TYPE_SOFTWARE;
        refer_attr.size = ATTR_SIZE;
        refer_attr.config = PERF_COUNT_SW_CONTEXT_SWITCHES;
        refer_attr.sample_freq = 1;
        refer_attr.sample_type = PERF_SAMPLE_TIME;
        refer_attr.freq = true;
        refer_attr.wakeup_events = 1;

        int fd = OpenPerfEvent(&refer_attr, -1, cpu, group_fd, (PERF_FLAG_FD_OUTPUT | PERF_FLAG_FD_NO_GROUP), err);
        if (fd < 0)
        {
            return 0;
        }
        pollfd poll_fd[1]{0};
        poll_fd[0].fd = group_fd;
        poll_fd[0].events = POLLIN;
        while (true)
        {
            int n = poll(poll_fd, 1, -1);
            if (n < 0 && errno != EINTR)
            {
                err = strerror(errno);
                goto fail;
            }
            if (n == 0)
            {
                continue;
            }
            auto buffs = ring_buffer.Read();
            if (buffs.size() == 0)
            {
                err = "ringbuffer read err";
                goto fail;
            }
            PerfSampleRecord record;
            RawStream raw_stream(buffs[0].second, buffs[0].first);
            if (!record.Read(raw_stream, stream_id_attr_map_, &refer_attr, stream_id_decoder_map_))
            {
                err = "read refer sample err";
                goto fail;
            }
            uint64_t time_offset = std::chrono::system_clock::now().time_since_epoch().count() - ring_buffer.TimeRunning() - record.time_;
            ring_buffer.SetTimeOffset(time_offset);
            for (auto buff : buffs)
            {
                delete[] buff.second;
            }
            break;
        }
        close(fd);
        return true;
    fail:
        close(fd);
        return false;
    }

    bool Monitor::EnableProbe(uint64_t probe_id, std::string &err)
    {
        std::lock_guard<std::mutex> lock(lock_);
        auto it = probe_id_fd_map_.find(probe_id);
        if (it == probe_id_fd_map_.end())
        {
            char buff[64]{0};
            std::sprintf(buff, "probe_id %ld no found", probe_id);
            err = std::string(buff);
            return false;
        }
        for (auto fd : it->second)
        {
            if (EnablePerfEvent(fd, err) < 0)
            {
                return false;
            }
        }
        return true;
    }

    bool Monitor::DisableProbe(uint64_t probe_id, std::string &err)
    {
        std::lock_guard<std::mutex> lock(lock_);
        auto it = probe_id_fd_map_.find(probe_id);
        if (it == probe_id_fd_map_.end())
        {
            char buff[64]{0};
            std::sprintf(buff, "probe_id %ld no found", probe_id);
            err = std::string(buff);
            return false;
        }
        for (auto fd : it->second)
        {
            if (DisablePerfEvent(fd, err) < 0)
            {
                return false;
            }
        }
        return true;
    }

    bool Monitor::EnableAll(std::string &err)
    {
        for (auto it : probe_id_fd_map_)
        {
            if (!EnableProbe(it.first, err))
            {
                return false;
            }
        }
        return true;
    }

    bool Monitor::DisableAll(std::string &err)
    {
        for (auto it : probe_id_fd_map_)
        {
            if (!DisableProbe(it.first, err))
            {
                return false;
            }
        }
        return true;
    }

    uint64_t Monitor::RegisterProbeEvent(ProbeOption option, std::string &err)
    {
        std::lock_guard<std::mutex> lock(lock_);
        if (option.is_dynamic_)
        {
            std::string void_err;
            RemoveProbe(option.group_, option.name_, option.is_kprobe_, void_err);
            if (AddProbe(option.group_, option.name_, option.address_, option.output_, option.on_return_, option.is_kprobe_, err) < 0)
            {
                return 0;
            }
        }

        auto fields = GetTraceEventFormat(option.group_, option.name_, err);
        if (err != "")
        {
            return 0;
        }

        int event_id = GetTraceEventID(option.group_, option.name_, err);
        if (event_id < 0)
        {
            return 0;
        }

        perf_event_attr attr{0};
        if (option.attr_ != nullptr)
        {
            attr = *option.attr_;
        }

        attr.config = event_id;
        attr.disabled = option.disabled_;
        attr.type = PERF_TYPE_TRACEPOINT;
        attr.size = ATTR_SIZE;
        attr.sample_period = 1;
        attr.sample_type |= (PERF_SAMPLE_STREAM_ID | PERF_SAMPLE_IDENTIFIER | PERF_SAMPLE_TIME | PERF_SAMPLE_RAW | PERF_SAMPLE_TID);
        attr.pinned = false;
        attr.freq = false;
        attr.watermark = true;
        attr.use_clockid = false;
        attr.wakeup_watermark = 1;

        std::vector<int> event_fds;
        std::vector<uint64_t> stream_ids;

        for (auto group : group_)
        {
            int fd = OpenPerfEvent(&attr, group.pid_or_fd_, group.cpu, group.group_fd_, group.flag_ | (PERF_FLAG_FD_OUTPUT | PERF_FLAG_FD_NO_GROUP), err);
            if (fd < 0)
            {
                goto fail;
            }
            event_fds.push_back(fd);
            uint64_t stream_id = GetStreamId(fd, err);
            if (err != "")
            {
                goto fail;
            }
            stream_ids.push_back(stream_id);
            if (option.filter_ != "")
            {
                if (SetPerfFilter(fd, option.filter_, err) < 0)
                {
                    goto fail;
                }
            }
        }

        probe_id_fd_map_[next_probe_id_] = event_fds;
        probe_id_stream_id_map_[next_probe_id_] = stream_ids;
        if (option.is_dynamic_)
        {
            probe_id_dynamic_probe_map_[next_probe_id_] = std::tuple<std::string, std::string, bool>{option.group_, option.name_, option.is_kprobe_};
        }

        for (auto stream_id : stream_ids)
        {
            stream_id_attr_map_[stream_id] = attr;
            stream_id_decoder_map_[stream_id] = Decoder{
                .fields_ = fields,
                .handle_ = option.decoder_handle_,
            };
        }
        return next_probe_id_++;

    fail:
        for (auto fd : event_fds)
        {
            close(fd);
        }
        if (option.is_dynamic_)
        {
            std::string void_err;
            RemoveProbe(option.group_, option.name_, option.is_kprobe_, void_err);
        }
        return 0;
    }

    bool Monitor::RemoveProbeEvent(uint64_t probe_id, std::string &err)
    {
        std::lock_guard<std::mutex> lock(lock_);
        char buff[64]{0};
        auto it = probe_id_fd_map_.find(probe_id);
        if (it == probe_id_fd_map_.end())
        {
            std::sprintf(buff, "probe_id %ld no found", probe_id);
            err = std::string(buff);
            return false;
        }
        for (auto fd : it->second)
        {
            close(fd);
        }

        probe_id_fd_map_.erase(it);

        for (auto stream_id : probe_id_stream_id_map_[probe_id])
        {
            stream_id_attr_map_.erase(stream_id);
            stream_id_decoder_map_.erase(stream_id);
        }

        probe_id_stream_id_map_.erase(probe_id);

        auto dynamic_it = probe_id_dynamic_probe_map_.find(probe_id);
        if (dynamic_it == probe_id_dynamic_probe_map_.end())
        {
            return true;
        }

        auto dynamic_tuple = dynamic_it->second;
        int res = RemoveProbe(std::get<0>(dynamic_tuple), std::get<1>(dynamic_tuple), std::get<2>(dynamic_tuple), err);
        if (res < 0)
        {
            return false;
        }

        return true;
    }
}