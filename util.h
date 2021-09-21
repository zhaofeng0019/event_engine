#ifndef EVENT_ENGINE_UTIL_INC_
#define EVENT_ENGINE_UTIL_INC_
#include <vector>
#include <string>
namespace event_engine
{
    std::vector<std::string> StringSplit(const std::string &content, char delim);
    std::string StringTrimSpace(const std::string &s);
    bool SafeMemcpy(void *dst, void *src, const int data_size, int &offset, const int total_size);
}
#endif