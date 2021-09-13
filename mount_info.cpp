#include "mount_info.h"
#include <iostream>
namespace event_engine
{
	void MountInfo::ParseFromLine(const std::string &line)
	{
		auto fields = StringSplit(line, ' ');
		mount_id_ = std::atoi(fields[0].c_str());
		if (errno == ERANGE)
		{
			std::cerr << "fatal: cat't parse mount id " << fields[0] << std::endl;
			exit(errno);
		}
		parent_id_ = std::atoi(fields[1].c_str());
		if (errno == ERANGE)
		{
			std::cerr << "fatal: cat't parse parent id " << fields[1] << std::endl;
			exit(errno);
		}

		auto mm = StringSplit(fields[2], ':');
		minor_ = std::atoi(mm[0].c_str());
		if (errno == ERANGE)
		{
			std::cerr << "fatal: cat't parse minor " << fields[2] << std::endl;
			exit(errno);
		}
		major_ = std::atoi(mm[1].c_str());
		if (errno == ERANGE)
		{
			std::cerr << "fatal: cat't parse major" << fields[2] << std::endl;
			exit(errno);
		}

		root_ = fields[3];

		mount_point_ = fields[4];

		mount_options_ = StringSplit(fields[5], ',');

		int i;
		for (i = 6; fields[i] != "-"; i++)
		{
			auto value = StringSplit(fields[i], ':');
			optional_fields_[value[0]] = value[1];
		}

		filesystem_type_ = fields[i + 1];
		mount_source_ = fields[i + 2];
		auto super_options = StringSplit(fields[i + 3], ',');
		for (auto it : super_options)
		{
			auto value = StringSplit(it, '=');
			if (value.size() > 1)
			{
				super_options_[value[0]] = value[1];
			}
			else
			{
				super_options_[value[0]] = "";
			}
		}
	}
}
