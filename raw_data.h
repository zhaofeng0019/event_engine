#ifndef EVENT_ENGINE_RAW_DATA_INC_
#define EVENT_ENGINE_RAW_DATA_INC_
#include <vector>
#include <string>
#include "trace_event_field.h"
namespace event_engine
{
    struct FieldData
    {
        std::string name_;
        int size_{0};
        char *ptr_{nullptr};
    };
    class RawData
    {
    public:
        std::vector<FieldData> fields_;
        void Parse(std::vector<TraceEventField> &trace_fields, char *data_ptr, const int total_size);
    };
}
#endif