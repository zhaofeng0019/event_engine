#ifndef EVENT_ENGINE_SYSTEMCALL_INC_
#define EVENT_ENGINE_SYSTEMCALL_INC_
#include "types.h"
#include <linux/perf_event.h>
#include <string>
#include <vector>
#include <map>

namespace event_engine
{
	int OpenPerfEvent(perf_event_attr *attr, int cpu, int group_fd, unsigned int *flag);
	int EnablePerfEvent(int fd);
	int DisablePerfEvent(int fd);
	int SetPerfFilter(int fd, std::string filter);
	std::vector<std::string> StringSplit(const std::string &content, char delim);
	std::vector<MountInfo> Mounts();
	std::string TracingDir();
	std::string PerfEventDir();
	int WriteTraceCommand(std::string name, std::string cmd);
	int AddProbe(std::string group, std::string name, std::string address /* uprobe may like /bin/bash:0x01 kprobe may like sys_open:0x01*/, std::string output, bool on_return, bool is_kprobe);
	int RemoveProbe(std::string group, std::string name, bool is_kprobe);
	int GetTraceEventID(std::string name);
}
#endif