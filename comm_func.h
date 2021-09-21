#ifndef EVENT_ENGINE_SYSTEMCALL_INC_
#define EVENT_ENGINE_SYSTEMCALL_INC_
#include "mount_info.h"
#include "trace_event_field.h"
#include <linux/perf_event.h>
#include <string>
#include <vector>
#include <map>

namespace event_engine
{
    int OpenPerfEvent(perf_event_attr *attr, int pid_or_fd, int cpu, int group_fd, unsigned long flag, std::string &err);
    uint64_t GetStreamId(int fd, std::string &err);
    int EnablePerfEvent(int fd, std::string &err);
    int DisablePerfEvent(int fd, std::string &err);
    int SetPerfFilter(int fd, const std::string &filter, std::string &err);
    std::vector<MountInfo> Mounts();
    std::string TracingDir();
    std::string PerfEventDir();
    int WriteTraceCommand(const std::string &name, const std::string &cmd, std::string &err);
    int AddProbe(const std::string &group, const std::string &name, const std::string &address /* uprobe may like /bin/bash:0x01 kprobe may like sys_open:0x01*/, const std::string &output, bool on_return, bool is_kprobe, std::string &err);
    int RemoveProbe(const std::string &group, const std::string &name, bool is_kprobe, std::string &err);
    int GetTraceEventID(const std::string &group, const std::string &name, std::string &err);
    std::vector<TraceEventField> GetTraceEventFormat(const std::string &group, const std::string &name, std::string &err);
}
#endif