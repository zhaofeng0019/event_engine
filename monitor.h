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
        unsigned long flag_{0};                                     /* open perf event flag */
        uint32_t ring_buff_page_{1024};                             /* page of one group ringbuffer default 1024(4M) */
        std::vector<uint32_t> pids_;                                /* pids that care about */
        std::string perf_event_dir_;                                /* specific perf_event_dri_ if not set ;it may auto find in mountinfo*/
        std::vector<std::string> cgroup_;                           /* cgroups that care about;offten use in container situation; for example i only want to monitor event in several containers*/
        bool all_events_{false};                                    /* care about all the pid in all the cgroups */
        std::function<void(std::string)> loop_err_handle_{nullptr}; /* call-back for loop err */
    };

    struct PerfEventGroup
    {
        int pid_or_fd_{0};
        int group_fd_{0};
        int cpu{0};
        unsigned long flag_{0};
        RingBuffer RingBuffer_;
    };

    struct ProbeOption
    {
        std::string group_;
        std::string name_;
        std::string filter_;
        std::string address_;
        std::string output_;
        bool disabled_{true};
        bool on_return_{false};
        std::function<void(void *)> decoder_handle_{nullptr}; /* in this function should not have block operation, so you may do some expensive operation async use a threadloop or something */
        perf_event_attr *attr_{nullptr};
        bool is_dynamic_{false};
        bool is_kprobe_{false};
    };

    class Monitor
    {
    private:
        std::mutex lock_;
        uint64_t next_probe_id_{1};                                                                           /*protect by lock_*/
        std::unordered_map<uint64_t, Decoder> stream_id_decoder_map_;                                         /*protect by lock_*/
        std::unordered_map<uint64_t, perf_event_attr> stream_id_attr_map_;                                    /* protect by lock_ */
        std::unordered_map<uint64_t, std::vector<int>> probe_id_fd_map_;                                      /*protect by lock_*/
        std::unordered_map<uint64_t, std::vector<uint64_t>> probe_id_stream_id_map_;                          /*protect by lock_*/
        std::unordered_map<uint64_t, std::tuple<std::string, std::string, bool>> probe_id_dynamic_probe_map_; /*protect by lock_*/

        std::vector<int> cgroup_fd_;
        std::vector<PerfEventGroup> group_;

        pollfd *poll_fd_{nullptr};

        bool is_running_{false};
        std::string loop_err_;
        std::function<void(std::string)> loop_err_handle_; /* call back for loop err */

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
        bool Init(MonitorOption option, std::string &err);
        Monitor() = default;
        Monitor(Monitor &) = delete;
        Monitor &operator=(Monitor &) = delete;
        bool EnableProbe(uint64_t probe_id, std::string &err);
        bool EnableAll(std::string &err);
        bool DisableProbe(uint64_t probe_id, std::string &err);
        bool DisableAll(std::string &err);
        uint64_t RegisterProbeEvent(ProbeOption option, std::string &err);
        bool RemoveProbeEvent(uint64_t probe_id, std::string &err);
    };
}

#endif