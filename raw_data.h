#ifndef EVENT_ENGINE_RAW_DATA_INC_
#define EVENT_ENGINE_RAW_DATA_INC_
#include <string>
#include <unordered_map>
#include <vector>

#include "trace_event_field.h"
namespace event_engine {
struct FieldData {
    uint32_t size_{0};   /* size of field value*/
    char *ptr_{nullptr}; /* ptr of field ;should not use out of decorder_handle
                            function */
};
class RawData {
   public:
    std::unordered_map<std::string, FieldData> field_map_;
};
void ParseRawData(std::vector<TraceEventField> &trace_fields, char *data_ptr,
                  const int total_size, RawData &raw_data);
}  // namespace event_engine
#endif