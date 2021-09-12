#include "util.h"
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <cstring>

namespace event_engine
{
	int OpenPerfEvent(perf_event_attr *attr, int pid, int cpu, int group_fd, unsigned long flags)
	{
		int res = syscall(SYS_perf_event_open, attr, pid, cpu, group_fd, flags);
		return res < 0 ? errno : res;
	}

	int EnablePerfEvent(int fd)
	{
		int res = ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
		return res < 0 ? errno : res;
	}

	int DisablePerfEvent(int fd)
	{
		int res = ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
		return res < 0 ? errno : res;
	}

	int SetPerfFilter(int fd, std::string filter)
	{
		int res = ioctl(fd, PERF_EVENT_IOC_SET_FILTER, filter.c_str());
		return res < 0 ? errno : res;
	}

	std::vector<std::string> StringSplit(const std::string &content, char delim)
	{
		std::vector<std::string> res;
		std::istringstream in(content);
		std::string token;
		while (getline(in, token, delim))
		{
			res.push_back(token);
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
		char buff[1024] = {0};
		while (fin.getline(buff, sizeof(buff)))
		{
			MountInfo info;
			std::string line(buff);
			auto fields = StringSplit(line, ' ');
			info.mount_id_ = std::atoi(fields[0].c_str());
			if (errno == ERANGE)
			{
				std::cerr << "fatal: cat't parse mount id " << fields[0] << std::endl;
				exit(errno);
			}
			info.parent_id_ = std::atoi(fields[1].c_str());
			if (errno == ERANGE)
			{
				std::cerr << "fatal: cat't parse parent id " << fields[1] << std::endl;
				exit(errno);
			}

			auto mm = StringSplit(fields[2], ':');
			info.minor_ = std::atoi(mm[0].c_str());
			if (errno == ERANGE)
			{
				std::cerr << "fatal: cat't parse minor " << fields[2] << std::endl;
				exit(errno);
			}
			info.major_ = std::atoi(mm[1].c_str());
			if (errno == ERANGE)
			{
				std::cerr << "fatal: cat't parse major" << fields[2] << std::endl;
				exit(errno);
			}

			info.root_ = fields[3];

			info.mount_point_ = fields[4];

			info.mount_options_ = StringSplit(fields[5], ',');

			int i;
			for (i = 6; fields[i] != "-"; i++)
			{
				auto value = StringSplit(fields[i], ':');
				info.optional_fields_[value[0]] = value[1];
			}

			info.filesystem_type_ = fields[i + 1];
			info.mount_source_ = fields[i + 2];
			auto super_options = StringSplit(fields[i + 3], ',');
			for (auto it : super_options)
			{
				auto value = StringSplit(it, '=');
				if (value.size() > 1)
				{
					info.super_options_[value[0]] = value[1];
				}
				else
				{
					info.super_options_[value[0]] = "";
				}
			}
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

	int WriteTraceCommand(std::string name, std::string cmd)
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

	int AddProbe(std::string group, std::string name, std::string address /* uprobe may like /bin/bash:0x01 kprobe may like sys_open:0x01*/, std::string output, bool on_return, bool is_kprobe)
	{
		std::string cmd = on_return ? "r:" : "p:";
		cmd += group + "/" + name + " ";
		cmd += address + " ";
		cmd += output;
		return WriteTraceCommand(is_kprobe ? "kprobe_events" : "uprobe_events", cmd);
	}

	int RemoveProbe(std::string group, std::string name, bool is_kprobe)
	{
		return WriteTraceCommand(is_kprobe ? "kprobe_events" : "uprobe_events", "-:" + group + "/" + name);
	}

	int GetTraceEventID(std::string name)
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
}
