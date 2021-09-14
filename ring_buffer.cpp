#include "ring_buffer.h"
#include <unistd.h>
#include <sys/mman.h>
#include <atomic>
#include <cstring>
#include <iostream>
namespace event_engine
{
    bool RingBuffer::Mmap()
    {
        if (pages_ & (pages_ - 1) != 0) // pages not pow of 2
        {
            return false;
        }
        int page_size = getpagesize();
        size_ = page_size * (pages_ + 1);
        data_size_ = page_size * pages_;
        data_mask_ = data_size_ - 1;
        void *maped = mmap(nullptr, size_, PROT_WRITE | PROT_READ, MAP_SHARED, fd_, 0);
        if (maped == MAP_FAILED)
        {
            return false;
        }
        data_ = (char *)maped + page_size;
        meta_ = (perf_event_mmap_page *)maped;
        return true;
    }

    void RingBuffer::Unmap()
    {
        munmap((void *)data_, size_);
    }

    uint64_t RingBuffer::TimeRunning()
    {
        return std::atomic_load_explicit((std::atomic<uint64_t> *)&meta_->time_running, std::memory_order_relaxed);
    }

    std::vector<std::pair<int, char *>> RingBuffer::Read()
    {
        int data_tail = meta_->data_tail;
        int data_head = std::atomic_load_explicit((std::atomic<uint64_t> *)&meta_->data_head, std::memory_order_acquire);
        int data_begin, data_end;
        std::vector<std::pair<int, char *>> res; // free manual
        while (data_tail != data_head)
        {
            data_begin = data_tail & data_mask_;
            data_end = data_head & data_mask_;
            if (data_end > data_begin)
            {
                char *buff = new char[data_end - data_begin];
                if (buff != nullptr)
                {
                    std::memcpy(buff, data_ + data_begin, data_end - data_begin);
                    res.push_back(std::pair<int, char *>{data_end - data_begin, buff});
                }
            }
            else
            {
                char *buff = new char[data_size_ - data_begin + data_end];
                if (buff != nullptr)
                {
                    std::memcpy(buff, data_ + data_begin, data_size_ - data_begin);
                    std::memcpy(buff, data_, data_end);
                    res.push_back(std::pair<int, char *>{data_size_ - data_begin + data_end, buff});
                }
            }
            data_tail = data_head;
            std::atomic_store_explicit((std::atomic<uint64_t> *)&meta_->data_tail, data_tail, std::memory_order_release);
            data_head = std::atomic_load_explicit((std::atomic<uint64_t> *)&meta_->data_head, std::memory_order_acquire);
        }
        return res;
    }
}
