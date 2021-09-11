#include "util.h"
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string>
#include <errno.h>

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
}
