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
        int start = 0;
        int end = s.size() - 1;
        while (start < s.size() && std::isspace(s[start]))
        {
            start++;
        }
        while (end != 0 && std ::isspace(s[end]))
        {
            end--;
        }
        return s.substr(start, end - start + 1);
    }

    bool SafeMemcpy(void *dst, void *src, const int data_size, int &offset, const int total_size)
    {
        if (total_size - offset < data_size)
        {
            return false;
        }
        std::memcpy(dst, (char *)src + offset, data_size);
        offset += data_size;
        return true;
    }
}
