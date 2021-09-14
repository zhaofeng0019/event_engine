#ifndef EVENT_ENGINE_TRACE_EVENT_FIELD_INC_
#define EVENT_ENGINE_TRACE_EVENT_FIELD_INC_
#include <string>
namespace event_engine
{
    class TraceEventField
    {
    public:
        std::string name_;
        int offset_{0};
        int size_{0};
        int data_loc_size_{0};

        bool ParseFromLine(const std::string &line);
    };
}
#endif