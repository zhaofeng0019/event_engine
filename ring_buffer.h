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
        RingBuffer(int fd, int pages) : fd_(fd), pages_(pages){};
        ~RingBuffer(){};
        bool Mmap();
        void Unmap();
        uint64_t TimeRunning();
        std::vector<std::pair<int, char *>> Read();

    private:
        int fd_;
        int pages_;
        int size_;
        int data_size_;
        int data_mask_;
        char *data_;
        perf_event_mmap_page *meta_;
    };
}
#endif
