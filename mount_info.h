#ifndef EVENT_ENGINE_MOUNTINFO_INC__
#define EVENT_ENGINE_MOUNTINFO_INC__
#include "util.h"
#include <string>
#include <vector>
#include <map>
namespace event_engine
{
    class MountInfo
    {
    public:
        uint mount_id_;
        uint parent_id_;
        uint major_;
        uint minor_;
        std::string root_;
        std::string mount_point_;
        std::vector<std::string> mount_options_;
        std::map<std::string, std::string> optional_fields_;
        std::string filesystem_type_;
        std::string mount_source_;
        std::map<std::string, std::string> super_options_;

        void ParseFromLine(const std::string &line);
    };
}
#endif