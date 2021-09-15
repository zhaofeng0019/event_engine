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
            uint64_t value_{0};
            uint64_t id_{0};
        };

    public:
        uint64_t time_enabled_{0};
        uint64_t time_running_{0};
        std::vector<value> values_;

        bool Read(char *data_ptr, const int total_size, int &offset, uint64_t format);
    };

    struct BranchEntry
    {
        uint64_t from_{0};
        uint64_t to_{0};
        bool mispred_{false};
        bool predicted_{false};
        bool intx_{false};
        bool abort_{false};
        uint16_t cycles_{0};
    };

    class PerfSampleRecord
    {
    public:
        struct perf_event_header header_;
        uint64_t sample_id_{0};
        uint64_t ip_{0};
        uint32_t pid_{0};
        uint32_t tid_{0};
        uint64_t time_{0};
        uint64_t addr_{0};
        uint64_t id_{0};
        uint64_t stream_id_{0};
        uint32_t cpu_{0};
        uint64_t period_{0};
        SampleReadFormat read_format_;
        std::vector<uint64_t> ips_;
        uint64_t raw_data_size_{0};
        char *raw_data_{nullptr};
        std::vector<BranchEntry> branches_;
        uint64_t user_abi_{0};
        std::vector<uint64_t> user_args_;
        std::vector<uint64_t> stack_data_;
        uint64_t weight_{0};
        uint64_t data_src_{0};
        uint64_t transaction_{0};
        uint64_t intr_abi_{0};
        std::vector<uint64_t> intr_regs_;

        bool Read(char *data_ptr, const int total_size, int &offset, std::map<uint64_t, perf_event_attr> attr_map, perf_event_attr *default_attr);
    };

}
#endif