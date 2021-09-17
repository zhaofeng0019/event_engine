#include "monitor.h"
#include "comm_func.h"
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <fcntl.h>
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
                option.perf_event_dir_ = TracingDir();
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
        if (err != "")
        {
            err = strerror(errno);
        }
        ClearCgroupAndGroupFd();
        return;
    }

    std::unordered_map<uint64_t, Decoder> Monitor::GetDecoderMap()
    {
        return decoder_map_;
    }

    std::unordered_map<uint64_t, perf_event_attr> Monitor::GetAttrMap()
    {
        return attr_map_;
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

    bool Monitor::InitGroupLeaders(int pid_or_fd, bool is_fd, unsigned long flag, int ring_buffer_page, std::string &err)
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
            group_fd_.push_back(group_fd);
            RingBuffer ring_buffer(group_fd, ring_buffer_page);
            if (!ring_buffer.Mmap(err))
            {
                return false;
            }
            ring_buffer_map_[group_fd] = ring_buffer;
        }
    }

    void Monitor::ClearCgroupAndGroupFd()
    {
        for (auto fd : group_fd_)
        {
            close(fd);
        }
        for (auto fd : cgroup_fd_)
        {
            close(fd);
        }
        for (auto it : ring_buffer_map_)
        {
            it.second.Unmap();
        }
    }
    uint64_t Monitor::CpuTimeOffset(int cpu, int group_fd, RingBuffer &ring_buffer, std::string &err)
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
            if (!record.Read(buffs[0].second, buffs[0].first, offset, attr_map_, &refer_attr, decoder_map_))
            {
                err = "read refer sample err";
                goto fail;
            }

            for (auto buff : buffs)
            {
                delete[] buff.second;
            }
        }

    fail:
        close(fd);
        return 0;
    }

}