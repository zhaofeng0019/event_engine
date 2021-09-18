#include "monitor.h"
#include "comm_func.h"
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <fcntl.h>
#include <chrono>
#include <iostream>
#include <cstdio>
namespace event_engine
{
    const int ATTR_SIZE{64};

    Monitor::Monitor(MonitorOption &option, std::string &err)
    {
        cpus_ = sysconf(_SC_NPROCESSORS_ONLN);
        if (option.ring_buff_page_ == 0)
        {
            option.ring_buff_page_ = 1024;
        }
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
            return;
        }

        if (option.cgroup_all_)
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
        return;
    fail:
        if (err == "")
        {
            err = errno == 0 ? "monitor init fail" : strerror(errno);
        }
        ClearCgroupAndGroupFd();
        return;
    }

    bool Monitor::Start(std::string &err)
    {
        stop_fd_ = eventfd(0, 0);
        done_fd_ = eventfd(0, 0);

        /* do some init */

        std::thread t(&Monitor::RunLoop, this);
        t.detach();
        return true;
    }

    void Monitor::RunLoop(void)
    {
        is_running_ = true;
        while (true)
        {
            int n = poll(poll_fd_, 1, -1);
            if (poll_fd_[0].events & POLLIN)
            {
                /* stop */
                break;
            }
        }
        delete[] poll_fd_;
        /* do some clear */
        eventfd_write(done_fd_, 1);
    }

    void Monitor::Stop()
    {
        if (!is_running_)
        {
            return;
        }
        eventfd_write(stop_fd_, 20);
        eventfd_t value;
        eventfd_read(done_fd_, &value);
        return;
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

            group_.push_back(PerfEventGroup{
                .pid_or_fd_ = pid_or_fd,
                .group_fd_ = group_fd,
                .flag_ = flag,
                .cpu = cpu,
            });

            RingBuffer ring_buffer(group_fd, ring_buffer_page);
            if (!ring_buffer.Mmap(err))
            {
                return false;
            }
            ring_buffer_map_[group_fd] = ring_buffer;
            if (!CpuTimeOffset(cpu, group_fd, ring_buffer, err))
            {
                return false;
            }
        }
        return true;
    }

    void Monitor::ClearCgroupAndGroupFd()
    {
        for (auto group : group_)
        {
            close(group.group_fd_);
        }

        group_.clear();

        for (auto fd : cgroup_fd_)
        {
            close(fd);
        }

        cgroup_fd_.clear();
        for (auto it : ring_buffer_map_)
        {
            it.second.Unmap();
        }
        ring_buffer_map_.clear();
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
            int offset{0};
            if (!record.Read(buffs[0].second, buffs[0].first, offset, stream_id_attr_map_, &refer_attr, stream_id_decoder_map_))
            {
                err = "read refer sample err";
                goto fail;
            }
            uint64_t time_offset = std::chrono::system_clock::now().time_since_epoch().count() - ring_buffer.TimeRunning() - record.time_;
            ring_buffer.SetTimeOffset(offset);
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
            std::sprintf(buff, "probe_id %d no found", probe_id);
            err = std::string(buff);
            return false;
        }
        for (auto fd : it->second)
        {
            if (!EnablePerfEvent(fd, err))
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
            std::sprintf(buff, "probe_id %d no found", probe_id);
            err = std::string(buff);
            return false;
        }
        for (auto fd : it->second)
        {
            if (!DisablePerfEvent(fd, err))
            {
                return false;
            }
        }
        return true;
    }

    bool Monitor::EnableAll(std::string &err)
    {
        std::lock_guard<std::mutex> lock(lock_);
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
        std::lock_guard<std::mutex> lock(lock_);
        for (auto it : probe_id_fd_map_)
        {
            if (!DisableProbe(it.first, err))
            {
                return false;
            }
        }
        return true;
    }

    uint64_t Monitor::RegisterProbeEvent(ProbeOption &option, std::string &err)
    {
        std::lock_guard<std::mutex> lock(lock_);
        if (option.is_dynamic_)
        {
            if (!AddProbe(option.group_, option.name_, option.address_, option.output_, option.on_return_, option.is_kprobe_, err))
            {
                return 0;
            }
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
        attr.disabled = option.diabled_;
        attr.type = PERF_TYPE_TRACEPOINT;
        attr.size = ATTR_SIZE;
        attr.sample_period = 1;
        attr.sample_type |= PERF_SAMPLE_STREAM_ID | PERF_SAMPLE_IDENTIFIER | PERF_SAMPLE_TIME;
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
        }

        probe_id_fd_map_[next_probe_id_] = event_fds;
        probe_id_stream_id_map_[next_probe_id_] = stream_ids;
        probe_id_dyamic_probe_map_[next_probe_id_] = std::tuple<std::string, std::string, bool>{option.group_, option.name_, option.on_return_};
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
            std::sprintf(buff, "probe_id %d no found", probe_id);
            err = std::string(buff);
            return false;
        }
        for (auto fd : it->second)
        {
            close(fd);
        }

        for (auto stream_id : probe_id_stream_id_map_[probe_id])
        {
            stream_id_decoder_map_.erase(stream_id);
            stream_id_decoder_map_.erase(stream_id);
        }

        auto dynamic_it = probe_id_dyamic_probe_map_.find(probe_id);
        if (dynamic_it == probe_id_dyamic_probe_map_.end())
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
