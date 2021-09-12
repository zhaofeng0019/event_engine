#include <string>
#include <vector>
#include <map>
struct MountInfo
{
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
};
