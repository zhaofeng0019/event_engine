#include "util.h"
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string>

namespace event_engine
{
	int OpenPerfEvent(perf_event_attr *attr, int pid, int cpu, int group_fd, unsigned long flags)
	{
		return syscall(SYS_perf_event_open, attr, pid, cpu, group_fd, flags);
	}

	int EnablePerfEvent(int fd)
	{
		return ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
	}

	int DisablePerfEvent(int fd)
	{
		return ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
	}

	int SetPerfFilter(int fd, std::string filter)
	{
		return ioctl(fd, PERF_EVENT_IOC_SET_FILTER, filter.c_str());
	}
}
