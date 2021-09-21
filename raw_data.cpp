#include "raw_data.h"
#include "util.h"
namespace event_engine
{
    void RawData::Parse(std::vector<TraceEventField> &trace_fields, char *data_ptr, const int total_size)
    {
        for (auto field : trace_fields)
        {
            if (field.data_loc_size_ == 0)
            {
                /* pure data */
                if (total_size >= field.offset_ + field.size_)
                {
                    field_map_[field.name_] = FieldData{
                        .size_ = field.size_,
                        .ptr_ = data_ptr + field.offset_,
                    };
                }
                continue;
            }
            /* data_loc_data */
            int offset = field.offset_;
            if (field.data_loc_size_ == 4) /* {u16 offset, u16 len} */
            {
                uint16_t data_offset{0};
                uint16_t data_len{0};
                if (!SafeMemcpy(&data_offset, data_ptr, 2, offset, total_size) || !SafeMemcpy(&data_len, data_ptr, 2, offset, total_size))
                {
                    continue;
                }
                if (total_size >= data_offset + data_len)
                {
                    field_map_[field.name_] = FieldData{
                        .size_ = data_len,
                        .ptr_ = data_ptr + data_offset,
                    };
                }
                continue;
            }
            /* {u32 offset, u32 len} */
            uint32_t data_offset{0};
            uint32_t data_len{0};
            if (!SafeMemcpy(&data_offset, data_ptr, 4, offset, total_size) || !SafeMemcpy(&data_len, data_ptr, 4, offset, total_size))
            {
                continue;
            }
            if (total_size >= data_offset + data_len)
            {
                field_map_[field.name_] = FieldData{
                    .size_ = data_len,
                    .ptr_ = data_ptr + data_offset,
                };
            }
        }
    }
};