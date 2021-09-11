#ifndef EVENT_ENGINE_SYSTEMCALL_INC_
#define EVENT_ENGINE_SYSTEMCALL_INC_
#include <linux/perf_event.h>
namespace event_engine
{
	int OpenPerfEvent(perf_event_attr *attr, int cpu, int group_fd, unsigned int *flag);
}
#endif