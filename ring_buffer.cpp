#include "ring_buffer.h"
#include <unistd.h>
#include <sys/mman.h>
#include <atomic>
#include <cstring>
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
		if (maped == nullptr)
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

	std::vector<std::shared_ptr<char>> RingBuffer::Read()
	{
		int data_tail = meta_->data_tail;
		int data_head = std::atomic_load_explicit((std::atomic<uint64_t> *)&meta_->data_head, std::memory_order_acquire);
		int data_begin, data_end;
		std::vector<std::shared_ptr<char>> res;
		while (data_tail != data_head)
		{
			data_begin = data_tail & data_mask_;
			data_end = data_head & data_mask_;
			if (data_end > data_begin)
			{
				std::shared_ptr<char> buff(new char[data_end - data_begin], std::default_delete<char[]>());
				std::memcpy(buff.get(), data_ + data_begin, data_end - data_begin);
				res.push_back(buff);
			}
			else
			{
				std::shared_ptr<char> buff(new char[data_size_ - data_begin + data_end], std::default_delete<char[]>());
				std::memcpy(buff.get(), data_ + data_begin, data_size_ - data_begin);
				std::memcpy(buff.get(), data_, data_end);
				res.push_back(buff);
			}
			data_tail = data_head;
			std::atomic_store_explicit((std::atomic<uint64_t> *)&meta_->data_tail, data_tail, std::memory_order_release);
			data_head = std::atomic_load_explicit((std::atomic<uint64_t> *)&meta_->data_head, std::memory_order_acquire);
		}
		return res;
	}
}
