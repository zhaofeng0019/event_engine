#ifndef EVENT_ENGINE_PERF_SAMPLE_RECORD_INC_
#define EVENT_ENGINE_PERF_SAMPLE_RECORD_INC_
#include <linux/perf_event.h>
#include <vector>
#include <cinttypes>
#include <map>
namespace event_engine
{
    class SampleReadFormat
    {
    public:
        struct value
        {
            uint64_t value_;
            uint64_t id_;
        };

    public:
        uint64_t time_enabled_;
        uint64_t time_running_;
        std::vector<value> values_;

        bool Read(char *data_ptr, const int total_size, int &offset, uint64_t format);
    };

    class BranchEntry
    {
    public:
        uint64_t from_;
        uint64_t to_;
        bool mispred_;
        bool predicted_;
        bool intx_;
        bool abort_;
        uint16_t cycles_;
    };

    class PerfSampleRecord
    {
    public:
        struct perf_event_header header_;
        uint64_t sample_id_;
        uint64_t ip_;
        uint32_t pid_;
        uint32_t tid_;
        uint64_t time_;
        uint64_t addr_;
        uint64_t id_;
        uint64_t stream_id_;
        uint32_t cpu_;
        uint64_t period_;
        SampleReadFormat read_format_;
        std::vector<uint64_t> ips_;
        uint64_t raw_data_size_;
        char *raw_data_;
        std::vector<BranchEntry> branches_;
        uint64_t user_abi_;
        std::vector<uint64_t> user_args_;
        std::vector<uint64_t> stack_data_;
        uint64_t weight_;
        uint64_t data_src_;
        uint64_t transaction_;
        uint64_t intr_abi_;
        std::vector<uint64_t> intr_regs_;

        bool Read(char *data_ptr, const int total_size, int &offset, std::map<uint64_t, perf_event_attr> attr_map, perf_event_attr *default_attr);
    };

}
#endif