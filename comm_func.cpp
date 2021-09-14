#include "comm_func.h"
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <cstring>

namespace event_engine
{

	int OpenPerfEvent(perf_event_attr *attr, int pid, int cpu, int group_fd, unsigned long flags)
	{
		int res = syscall(SYS_perf_event_open, attr, pid, cpu, group_fd, flags);
		return res;
	}

	int EnablePerfEvent(int fd)
	{
		int res = ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
		return res;
	}

	int DisablePerfEvent(int fd)
	{
		int res = ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
		return res;
	}

	int SetPerfFilter(int fd, const std::string &filter)
	{
		int res = ioctl(fd, PERF_EVENT_IOC_SET_FILTER, filter.c_str());
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
		char buff[1024] = {0};
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
				if (m.mount_point_.substr(0, sizeof("/sys/kernel/debug") - 1) == "/sys/kernel/debug")
				{
					return "/sys/kernel/debug/tracing";
				}
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

	int WriteTraceCommand(const std::string &name, const std::string &cmd)
	{
		std::string path = TracingDir() + "/" + name;
		int fd = open(path.c_str(), O_WRONLY | O_APPEND);
		if (fd == -1)
		{
			return errno;
		}
		auto s = write(fd, cmd.c_str(), cmd.size());
		close(fd);
		if (s == -1)
		{
			return errno;
		}
		return 0;
	}

	int AddProbe(const std::string &group, const std::string &name, const std::string &address /* uprobe may like /bin/bash:0x01 kprobe may like sys_open:0x01*/, const std::string &output, bool on_return, bool is_kprobe)
	{
		std::string cmd = on_return ? "r:" : "p:";
		cmd += group + "/" + name + " ";
		cmd += address + " ";
		cmd += output;
		return WriteTraceCommand(is_kprobe ? "kprobe_events" : "uprobe_events", cmd);
	}

	int RemoveProbe(const std::string &group, const std::string &name, bool is_kprobe)
	{
		return WriteTraceCommand(is_kprobe ? "kprobe_events" : "uprobe_events", "-:" + group + "/" + name);
	}

	int GetTraceEventID(const std::string &name)
	{
		std::string file = TracingDir() + "/events/" + name + "/id";
		int fd = open(file.c_str(), O_RDONLY);
		if (fd == -1)
		{
			return errno;
		}
		char buff[8] = {0};
		int n = read(fd, buff, 6);
		close(fd);
		if (n == -1)
		{
			return errno;
		}
		int i = 0;
		for (; '0' <= buff[i] && buff[i] <= '9' && i < 6; i++)
		{
		}
		buff[i] = 0;
		int id = std::atoi(buff);
		if (errno == ERANGE)
		{
			return errno;
		}
		return id;
	}

	std::vector<TraceEventField> GetTraceEventFormat(const std::string &group, const std::string &name)
	{
		std::vector<TraceEventField> res;
		std::string file = TracingDir() + "/events/" + group + "/" + name + "/" + "format";
		std::ifstream fin;
		fin.open(file);
		if (!fin.is_open())
		{
			return res;
		}
		char buff[1024] = {0};
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
