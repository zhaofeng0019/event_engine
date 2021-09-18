#ifndef EVENT_ENGINE_MONITOR_INC_
#define EVENT_ENGINE_MONITOR_INC_
#include "perf_sample_record.h"
#include "ring_buffer.h"
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <thread>
#include <vector>
#include <tuple>
#include <functional>
namespace event_engine
{
    struct MonitorOption
    {
        unsigned long flag_{0};
        uint32_t ring_buff_page_{0};
        std::vector<int> pids_;
        std::string perf_event_dir_;
        std::vector<std::string> cgroup_;
        bool cgroup_all_{false};
    };

    struct PerfEventGroup
    {
        int pid_or_fd_{0};
        int group_fd_{0};
        int cpu{0};
        unsigned long flag_{0};
    };

    struct ProbeOption
    {
        std::string group_;
        std::string name_;
        std::string filter_;
        std::string address_;
        std::string output_;
        bool diabled_{true};
        bool on_return_{false};
        std::function<void(void *)> decoder_handle_{nullptr};
        perf_event_attr *attr_{nullptr};
        bool is_dynamic_{false};
        bool is_kprobe_{false};
    };

    class Monitor
    {
    private:
        std::mutex lock_;
        uint64_t next_probe_id_{1};                                                                          /*protect by lock_*/
        std::unordered_map<uint64_t, Decoder> stream_id_decoder_map_;                                        /*protect by lock_*/
        std::unordered_map<uint64_t, perf_event_attr> stream_id_attr_map_;                                   /* protect by lock_ */
        std::unordered_map<uint64_t, std::vector<int>> probe_id_fd_map_;                                /*protect by lock_*/
        std::unordered_map<uint64_t, std::vector<uint64_t>> probe_id_stream_id_map_;                         /*protect by lock_*/
        std::unordered_map<uint64_t, std::tuple<std::string, std::string, bool>> probe_id_dyamic_probe_map_; /*protect by lock_*/

        std::vector<PerfEventGroup> group_;
        std::vector<int> cgroup_fd_;
        std::unordered_map<int, RingBuffer> ring_buffer_map_;

        pollfd *poll_fd_{nullptr};
        bool is_running_{false};

        int cpus_{0};
        int stop_fd_{0};
        int done_fd_{0};

        void RunLoop(void);
        bool InitGroupLeaders(int pid_fd, bool is_fd, unsigned long flag, uint32_t ring_buffer_page, std::string &err);
        void ClearCgroupAndGroupFd();
        bool CpuTimeOffset(int cpu, int group_fd, RingBuffer &ring_buffer, std::string &err);

    public:
        bool IsRunning();
        bool Start(std::string &err);
        void Stop();
        Monitor(MonitorOption &option, std::string &err);
        Monitor(Monitor &) = delete;
        Monitor &operator=(Monitor &) = delete;
        bool EnableProbe(uint64_t probe_id, std::string &err);
        bool EnableAll(std::string &err);
        bool DisableProbe(uint64_t probe_id, std::string &err);
        bool DisableAll(std::string &err);
        uint64_t RegisterProbeEvent(ProbeOption &option, std::string &err);
        bool RemoveProbeEvent(uint64_t probe_id, std::string &err);
    };
}

#endif