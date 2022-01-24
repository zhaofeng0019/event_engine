#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

#include "monitor.h"

class ThreadPool {
    int threads_;
    std::condition_variable cv_;
    std::mutex m_;
    std::queue<std::function<void(void)>> task_queue_;
    void WorkThread();
    std::atomic<bool> running_{false};

   public:
    void Run();
    void AddTask(std::function<void(void)> task);
    ThreadPool(int thread);
    void Stop();
};

ThreadPool::ThreadPool(int thread) : threads_(thread){};
void ThreadPool::Run() {
    running_.store(true);
    for (int i = 0; i < threads_; i++) {
        std::thread t(&ThreadPool::WorkThread, this);
        t.detach();
    }
}

void ThreadPool::AddTask(std::function<void(void)> task) {
    std::unique_lock<std::mutex> lock(m_);
    task_queue_.push(task);
    cv_.notify_one();
}

void ThreadPool::Stop() {
    running_.store(false);
    cv_.notify_all();
    return;
}

void ThreadPool::WorkThread() {
    while (true) {
        std::function<void(void)> task;
        {
            std::unique_lock<std::mutex> lock(m_);
            cv_.wait(lock, [this] {
                return (!task_queue_.empty() || !running_.load());
            });
            if (!running_) {
                return;
            }
            task = task_queue_.front();
            task_queue_.pop();
        }
        task();
    }
}

ThreadPool thread_pool(1);
event_engine::Monitor m;

struct AcceptEvent {
    uint32_t local_addr_{0};
    uint32_t remote_addr_{0};
    uint16_t local_port_{0};
    uint16_t remote_port_{0};
    uint32_t pid_{0};
    uint64_t time_stamp_{0};
};

void PrintAcceptEvent(AcceptEvent event) {
    std::cout << "\n================accept===================" << std::endl;

    time_t t = event.time_stamp_;
    std::cout << "time " << asctime(localtime(&t)) << std::endl;

    char buff[128] = {0};
    std::sprintf(buff, "local_addr %d.%d.%d.%d ", event.local_addr_ & 0xFF,
                 (event.local_addr_ >> 8) & 0xFF,
                 (event.local_addr_ >> 16) & 0xFF,
                 (event.local_addr_ >> 24) & 0xFF);
    std::cout << buff << std::endl;

    std::memset(buff, 0, 128);
    std::sprintf(buff, "remote_addr %d.%d.%d.%d ", event.remote_addr_ & 0xFF,
                 (event.remote_addr_ >> 8) & 0xFF,
                 (event.remote_addr_ >> 16) & 0xFF,
                 (event.remote_addr_ >> 24) & 0xFF);
    std::cout << buff << std::endl;

    std::memset(buff, 0, 128);
    std::sprintf(buff, "local_port %d ", event.local_port_);
    std::cout << buff << std::endl;

    std::memset(buff, 0, 128);
    std::sprintf(buff, "remote_port %d ",
                 ((event.remote_port_ & 0xFF) << 8) +
                     ((event.remote_port_ >> 8) & 0xFF));
    std::cout << buff << std::endl;

    std::cout << "pid " << event.pid_ << std::endl;

    std::cout << "=========================================\n" << std::endl;
}

void AcceptHandle(void *data) {
    event_engine::PerfSampleRecord *record =
        (event_engine::PerfSampleRecord *)data;
    AcceptEvent event;
    event.time_stamp_ = record->time_ / 1000000000;
    event_engine::FieldData field;
    field = record->raw_data_.field_map_["local_addr"];
    if (field.size_) {
        event.local_addr_ = *(uint32_t *)field.ptr_;
    }
    field = record->raw_data_.field_map_["remote_addr"];
    if (field.size_) {
        event.remote_addr_ = *(uint32_t *)field.ptr_;
    }

    field = record->raw_data_.field_map_["local_port"];
    if (field.size_) {
        event.local_port_ = *(uint16_t *)field.ptr_;
    }

    field = record->raw_data_.field_map_["remote_port"];
    if (field.size_) {
        event.remote_port_ = *(uint16_t *)field.ptr_;
    }
    event.pid_ = record->pid_;
    thread_pool.AddTask(std::bind(PrintAcceptEvent, event));
}

struct ExecEvent {
    std::string filename_;
    uint32_t pid_{0};
    uint64_t time_stamp_{0};
};

void PrintExecEvent(ExecEvent event) {
    std::cout << "\n================exec===================" << std::endl;
    time_t t = event.time_stamp_;
    std::cout << "time " << asctime(localtime(&t)) << std::endl;

    std::cout << "filename " << event.filename_ << std::endl;

    std::cout << "pid " << event.pid_ << std::endl;

    std::cout << "=========================================\n" << std::endl;
}

void ExecHandle(void *data) {
    event_engine::PerfSampleRecord *record =
        (event_engine::PerfSampleRecord *)data;
    ExecEvent event;
    std::unordered_map<std::string, event_engine::FieldData>::iterator it;
    event.time_stamp_ = record->time_ / 1000000000;
    auto field = record->raw_data_.field_map_["filename"];
    if (field.size_) {
        event.filename_ = std::string(field.ptr_);
    }
    field = record->raw_data_.field_map_["pid"];
    if (field.size_) {
        event.pid_ = *(uint32_t *)field.ptr_;
    }
    thread_pool.AddTask(std::bind(PrintExecEvent, event));
}

void signalHandler(int signum) {
    thread_pool.Stop();
    m.Stop();
    exit(0);
}

int main() {
    std::string err;
    event_engine::MonitorOption option{
        .all_events_ = true,
    };
    if (!m.Init(option, err)) {
        std::cout << "fail to init monitor " << err << std::endl;
        return 0;
    }

    event_engine::ProbeOption accept_option{
        .group_ = "xiaoff",
        .name_ = "tcp_accept",
        .filter_ = "ret!=0",
        .address_ = "inet_csk_accept",
        .output_ =
            "ret=$retval remote_addr=+0($retval):u32 "
            "local_addr=+4($retval):u32 remote_port=+12($retval):u16 "
            "local_port=+14($retval):u16",
        .on_return_ = true,
        .decoder_handle_ = AcceptHandle,
        .is_dynamic_ = true,
        .is_kprobe_ = true,
    };

    if (!m.RegisterProbeEvent(accept_option, err)) {
        std::cout << "register accept  fail " << err << std::endl;
        return 0;
    }

    event_engine::ProbeOption exec_option{
        .group_ = "sched",
        .name_ = "sched_process_exec",
        .decoder_handle_ = ExecHandle,
        .is_dynamic_ = false,
        .is_kprobe_ = false,
    };

    if (!m.RegisterProbeEvent(exec_option, err)) {
        std::cout << "register exec fail " << err << std::endl;
        return 0;
    }

    thread_pool.Run();

    if (!m.Start(err)) {
        std::cout << "monitor start err " << err << std::endl;
        return 0;
    }
    signal(SIGINT, signalHandler);
    while (true) {
        sleep(1024);
    }
    return 0;
}