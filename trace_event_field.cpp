#include "trace_event_field.h"
#include "util.h"
namespace event_engine
{
    bool TraceEventField::ParseFromLine(const std::string &line)
    {
        auto tokens = StringSplit(line, ';');
        if (tokens.size() < 4)
        {
            return false;
        }
        std::string name;
        for (auto token : tokens)
        {
            token = StringTrimSpace(token);
            auto parts = StringSplit(token, ':');
            if (parts.size() != 2)
            {
                return false;
            }
            if (parts[0] == "field")
            {
                name = parts[1];
                continue;
            }
            if (parts[0] == "offset")
            {
                offset_ = std::atoi(parts[1].c_str());
                if (errno == ERANGE)
                {
                    return false;
                }
                continue;
            }
            if (parts[0] == "size")
            {
                size_ = std::atoi(parts[1].c_str());
                if (errno == ERANGE)
                {
                    return false;
                }
                continue;
            }
        }
        auto name_parts = StringSplit(name, ' ');
        name_ = name_parts[name_parts.size() - 1];
        if (name.substr(0, sizeof("__data_loc") - 1) == "__data_loc")
        {
            data_loc_size_ = size_;
            if ((data_loc_size_ != 4 && data_loc_size_ != 8) || name_parts.size() < 2 || name_parts[name_parts.size() - 2] != "char[]")
            {
                return false;
            }
        }
        return true;
    }
}