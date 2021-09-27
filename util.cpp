#include "util.h"
#include <sstream>
#include <cstring>
namespace event_engine
{
    std::vector<std::string> StringSplit(const std::string &content, char delim)
    {
        std::vector<std::string> res;
        std::istringstream in(content);
        std::string token;
        while (getline(in, token, delim))
        {
            res.push_back(token);
        }
        return res;
    }

    std::string StringTrimSpace(const std::string &s)
    {
        if (s.size() == 0)
        {
            return "";
        }
        int start = 0;
        int end = s.size() - 1;
        while (start < s.size() && std::isspace(s[start]))
        {
            start++;
        }
        while (end != 0 && std::isspace(s[end]))
        {
            end--;
        }
        return s.substr(start, end - start + 1);
    }

    RawStream::RawStream(char *ptr, int total_size) : ptr_(ptr), total_size_(total_size)
    {
    }

    bool RawStream::Read(void *dst, int data_size)
    {
        if (total_size_ - offset_ < data_size)
        {
            return false;
        }
        std::memcpy(dst, ptr_ + offset_, data_size);
        offset_ += data_size;
        return true;
    }

    void RawStream::SetOffset(int offset)
    {
        offset_ = offset;
    }

    int RawStream::GetOffset() const
    {
        return offset_;
    }

    char *RawStream::GetPtr() const
    {
        return ptr_;
    }
}
