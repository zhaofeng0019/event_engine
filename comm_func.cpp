#include "comm_func.h"
#include <sys/syscall.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <cstring>

namespace event_engine
{

    int OpenPerfEvent(perf_event_attr *attr, int pid_or_fd, int cpu, int group_fd, unsigned long flags, std::string &err)
    {
        int res = syscall(SYS_perf_event_open, attr, pid_or_fd, cpu, group_fd, flags);
        if (res < 0)
        {
            err = strerror(errno);
        }
        return res;
    }

    uint64_t GetStreamId(int fd, std::string &err)
    {
        uint64_t value{0};
        if (ioctl(fd, PERF_EVENT_IOC_ID, &value) < 0)
        {
            err = strerror(errno);
        }
        return value;
    }

    int EnablePerfEvent(int fd, std::string &err)
    {
        int res = ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
        if (res < 0)
        {
            err = strerror(errno);
        }
        return res;
    }

    int DisablePerfEvent(int fd, std::string &err)
    {
        int res = ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        if (res < 0)
        {
            err = strerror(errno);
        }
        return res;
    }

    int SetPerfFilter(int fd, const std::string &filter, std::string &err)
    {
        int res = ioctl(fd, PERF_EVENT_IOC_SET_FILTER, filter.c_str());
        if (res < 0)
        {
            err = strerror(errno);
        }
        return res;
    }

    std::vector<MountInfo> Mounts()
    {
        std::vector<MountInfo> res;
        std::ifstream fin;
        fin.open("/proc/self/mountinfo");
        if (!fin.is_open())
        {
            std::cerr << "fatal: can't open mount file /proc/self/mountinf" << std::endl;
            exit(errno);
        }
        char buff[1024]{0};
        while (fin.getline(buff, sizeof(buff)))
        {
            MountInfo info;
            info.ParseFromLine(StringTrimSpace(std::string(buff)));
            res.push_back(info);
        }
        return res;
    }

    std::string TracingDir()
    {
        auto mounts = Mounts();
        for (auto m : mounts)
        {
            if (m.filesystem_type_ == "tracefs")
            {
                return m.mount_point_;
            }
        }
        for (auto m : mounts)
        {
            if (m.filesystem_type_ == "debugfs")
            {
                std::string path = m.mount_point_ + "/tracing";
                struct stat st_buff;
                if (stat((path + "/events").c_str(), &st_buff) < 0 || !S_ISDIR(st_buff.st_mode))
                {
                    continue;
                }
                return path;
            }
        }
        return "";
    }

    std::string PerfEventDir()
    {
        auto mounts = Mounts();
        for (auto m : mounts)
        {
            if (m.filesystem_type_ == "cgroup")
            {
                for (auto option : m.super_options_)
                {
                    if (option.first == "perf_event")
                    {
                        return m.mount_point_;
                    }
                }
            }
        }
        return "";
    }

    int WriteTraceCommand(const std::string &relative_path, const std::string &cmd, std::string &err)
    {
        std::string path = TracingDir() + "/" + relative_path;
        int fd = open(path.c_str(), O_WRONLY | O_APPEND);
        if (fd == -1)
        {
            err = strerror(errno);
            return -1;
        }
        auto s = write(fd, cmd.c_str(), cmd.size());
        close(fd);
        if (s == -1)
        {
            err = strerror(errno);
            return -1;
        }
        return 0;
    }

    int AddProbe(const std::string &group, const std::string &name, const std::string &address, const std::string &output, bool on_return, bool is_kprobe, std::string &err)
    {
        std::string cmd = on_return ? "r:" : "p:";
        cmd += group + "/" + name + " ";
        cmd += address + " ";
        cmd += output;
        return WriteTraceCommand(is_kprobe ? "kprobe_events" : "uprobe_events", cmd, err);
    }

    int RemoveProbe(const std::string &group, const std::string &name, bool is_kprobe, std::string &err)
    {
        return WriteTraceCommand(is_kprobe ? "kprobe_events" : "uprobe_events", "-:" + group + "/" + name, err);
    }

    int GetTraceEventID(const std::string &group, const std::string &name, std::string &err)
    {
        std::string file = TracingDir() + "/events/" + group + "/" + name + "/id";
        int fd = open(file.c_str(), O_RDONLY);
        if (fd == -1)
        {
            err = strerror(errno);
            return -1;
        }
        char buff[8] = {0};
        int n = read(fd, buff, 6);
        close(fd);
        if (n == -1)
        {
            err = strerror(errno);
            return -1;
        }
        int i = 0;
        for (; '0' <= buff[i] && buff[i] <= '9' && i < 6; i++)
        {
        }
        buff[i] = 0;
        int id = std::atoi(buff);
        if (errno == ERANGE)
        {
            err = strerror(errno);
            return -1;
        }
        return id;
    }

    std::vector<TraceEventField> GetTraceEventFormat(const std::string &group, const std::string &name, std::string &err)
    {
        std::vector<TraceEventField> res;
        std::string file = TracingDir() + "/events/" + group + "/" + name + "/" + "format";
        std::ifstream fin;
        fin.open(file);
        if (!fin.is_open())
        {
            err = strerror(errno);
            return res;
        }
        char buff[1024]{0};
        while (fin.getline(buff, sizeof(buff)))
        {
            TraceEventField field;
            if (field.ParseFromLine(StringTrimSpace(std::string(buff))))
            {
                res.push_back(field);
            }
        }
        return res;
    }
}
