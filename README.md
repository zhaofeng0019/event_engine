# event_engine

event_engine based on perf 

support 

1. kprobe
2. uprobe
3. static event under /sys/kernel/debug/tracing/events/

example:
    make example && ./a.out
```
root@ubuntu:~/code/event_engine# ./a.out

================exec===================
time Tue Sep 21 23:31:47 2021

filename /bin/sh
pid 275389
=========================================


================exec===================
time Tue Sep 21 23:31:47 2021

filename /usr/bin/grep
pid 275391
=========================================


================exec===================
time Tue Sep 21 23:31:47 2021

filename /usr/bin/ls
pid 275390
=========================================


================accept===================
time Tue Sep 21 23:31:48 2021

local_addr 127.0.0.6
remote_addr 127.0.0.6
local_port 22
remote_port 56940
pid 899
=========================================


================exec===================
time Tue Sep 21 23:31:48 2021

filename /usr/sbin/sshd
pid 275392
=========================================
```


usage:
    for example you want to monitor accept event, you fill the appropriate field in ProbeOption, and it will auto register it as below  
    then you should write a handle funciton, raw_data_.field_map_ is a map that key is the field name of /sys/kernel/debug/tracing/xx/xx/format below, remember the ptr_ field of struct FieldData is only valid in handle function, and there should be not block operation in handle function, so in example.cpp i use a simple threadpool


```
root@ubuntu:~# cat /sys/kernel/debug/tracing/kprobe_events
r128:xiaoff/tcp_accept inet_csk_accept ret=$retval remote_addr=+0($retval):u32 local_addr=+4($retval):u32 remote_port=+12($retval):u16 local_port=+14($retval):u16

root@ubuntu:~# cat /sys/kernel/debug/tracing/events/xiaoff/tcp_accept/format
name: tcp_accept
ID: 1878
format:
        field:unsigned short common_type;       offset:0;       size:2; signed:0;
        field:unsigned char common_flags;       offset:2;       size:1; signed:0;
        field:unsigned char common_preempt_count;       offset:3;       size:1; signed:0;
        field:int common_pid;   offset:4;       size:4; signed:1;

        field:unsigned long __probe_func;       offset:8;       size:8; signed:0;
        field:unsigned long __probe_ret_ip;     offset:16;      size:8; signed:0;
        field:u64 ret;  offset:24;      size:8; signed:0;
        field:u32 remote_addr;  offset:32;      size:4; signed:0;
        field:u32 local_addr;   offset:36;      size:4; signed:0;
        field:u16 remote_port;  offset:40;      size:2; signed:0;
        field:u16 local_port;   offset:42;      size:2; signed:0;

print fmt: "(%lx <- %lx) ret=0x%Lx remote_addr=%u local_addr=%u remote_port=%u local_port=%u", REC->__probe_func, REC->__probe_ret_ip, REC->ret, REC->remote_addr, REC->local_addr, REC->remote_port, REC->local_port
```


```cpp
// example.cpp

void AcceptHandle(void *data)
{
    event_engine::PerfSampleRecord *record = (event_engine::PerfSampleRecord *)data;
    AcceptEvent event;
    event.time_stamp_ = record->time_ / 1000000000;
    event_engine::FieldData field;
    field = record->raw_data_.field_map_["local_addr"]; 
    if (field.size_)
    {
        event.local_addr_ = *(uint32_t *)field.ptr_;
    }
    field = record->raw_data_.field_map_["remote_addr"];
    if (field.size_)
    {
        event.remote_addr_ = *(uint32_t *)field.ptr_;
    }

    field = record->raw_data_.field_map_["local_port"];
    if (field.size_)
    {
        event.local_port_ = *(uint16_t *)field.ptr_;
    }

    field = record->raw_data_.field_map_["remote_port"];
    if (field.size_)
    {
        event.remote_port_ = *(uint16_t *)field.ptr_;
    }
    event.pid_ = record->pid_;
    thread_poll.AddTask(std::bind(PrintAcceptEvent, event));
}

// raw_data.h
namespace event_engine
{
    struct FieldData
    {
        uint32_t size_{0};   /* size of field value*/
        char *ptr_{nullptr}; /* ptr of field ;should not use out of decorder_handle function */
    };
    class RawData
    {
    public:
        std::unordered_map<std::string, FieldData> field_map_;
        void Parse(std::vector<TraceEventField> &trace_fields, char *data_ptr, const int total_size);
    };
}

```





[https://www.kernel.org/doc/html/latest/trace/kprobetrace.html](https://www.kernel.org/doc/html/latest/trace/kprobetrace.html)


