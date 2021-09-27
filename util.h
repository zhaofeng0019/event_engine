#ifndef EVENT_ENGINE_UTIL_INC_
#define EVENT_ENGINE_UTIL_INC_
#include <vector>
#include <string>
namespace event_engine
{
    std::vector<std::string> StringSplit(const std::string &content, char delim);
    std::string StringTrimSpace(const std::string &s);
    class RawStream
    {
    public:
        RawStream(char *ptr, int total_size);
        ~RawStream() = default;
        bool Read(void *dst, int data_size);
        void SetOffset(int offset);
        int GetOffset() const;
        char *GetPtr() const;

    private:
        char *ptr_{nullptr};
        int offset_{0};
        const int total_size_;
    };
}
#endif