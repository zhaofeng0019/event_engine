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
                    fields_.push_back(FieldData{
                        .name_ = field.name_,
                        .size_ = field.size_,
                        .ptr_ = data_ptr + field.offset_,
                    });
                }
                continue;
            }
            /* data_loc_data */
            int offset{0};
            if (field.data_loc_size_ == 4) /* {u16 offset, u16 len} */
            {
                uint16_t data_offset{0};
                uint16_t data_len{0};
                if (!SafeMemcpy(data_ptr, &data_offset, 2, offset, total_size) || !SafeMemcpy(data_ptr, &data_len, 2, offset, total_size))
                {
                    continue;
                }
                if (total_size >= data_offset + data_len)
                {
                    fields_.push_back(FieldData{
                        .name_ = field.name_,
                        .size_ = field.size_,
                        .ptr_ = data_ptr + offset,
                    });
                }
                continue;
            }
            /* {u32 offset, u32 len} */
            uint32_t data_offset{0};
            uint32_t data_len{0};
            if (!SafeMemcpy(data_ptr, &data_offset, 4, offset, total_size) || !SafeMemcpy(data_ptr, &data_len, 4, offset, total_size))
            {
                continue;
            }
            if (total_size >= data_offset + data_len)
            {
                fields_.push_back(FieldData{
                    .name_ = field.name_,
                    .size_ = field.size_,
                    .ptr_ = data_ptr + offset,
                });
            }
        }
    }
};