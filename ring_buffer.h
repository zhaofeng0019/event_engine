#ifndef EVENT_ENGINE_RING_BUFF_INC_
#define EVENT_ENGINE_RING_BUFF_INC_
#include <linux/perf_event.h>
#include <inttypes.h>
#include <vector>
#include <memory>
namespace event_engine
{
    class RingBuffer
    {
    public:
        int fd_{0};
        uint32_t pages_{0};

    private:
        int size_{0};
        int data_size_{0};
        int data_mask_{0};
        char *data_{nullptr};
        uint64_t timeOffset_{0};
        perf_event_mmap_page *meta_{nullptr};

    public:
        RingBuffer();
        RingBuffer(int fd, uint32_t pages);
        ~RingBuffer(){};
        bool Mmap(std::string &err);
        void Unmap();
        uint64_t TimeRunning();
        std::vector<std::pair<int, char *>> Read();
        void SetTimeOffset(uint64_t offset);
        uint64_t GetTimeOffset();
    };
}
#endif
