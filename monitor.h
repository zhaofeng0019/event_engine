#ifndef EVENT_ENGINE_MONITOR_INC_
#define EVENT_ENGINE_MONITOR_INC_
#include "perf_sample_record.h"
#include <mutex>
#include <unordered_map>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <thread>
#include <vector>
#include <tuple>
#include <functional>
namespace event_engine
{
    class Monitor
    {
    private:
        std::mutex lock_;
        std::unordered_map<uint64_t, Decoder> decoder_map_;                                         /*protect by lock_*/
        std::unordered_map<uint64_t, std::vector<uint64_t>> id_stream_id_map_;                      /*protect by lock_*/
        std::unordered_map<uint64_t, std::tuple<std::string, std::string, bool>> dyamic_probe_map_; /*protect by lock_*/
        uint64_t next_probe_id_{0};                                                                 /*protect by lock_*/

        int stop_fd_{0};
        int done_fd_{0};
        pollfd *poll_fd_{nullptr};

        void RunLoop(void);

    public:
        bool Start(std::string &err);
        void Stop();
        uint64_t AddStaticProbe(const std::string &group, const std::string &name, const std::string &filter, std::function<void(void *)> decoder_handle, std::string &err);
        uint64_t AddDynamicProbe(const std::string &group, const std::string &name, const std::string &address, const std::string &output, bool on_return, bool is_kprobe, const std::string &filter, std::function<void(void *)> decoder_handle, std::string &err);
        void RemoveProbe(uint64_t probe_id);
    };
}

#endif