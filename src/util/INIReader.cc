/*
	This file is modified version from https://github.com/Tencent/Pebble/blob/master/src/common/ini_reader.cpp
*/

#include "util/INIReader.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <string.h>
#include <cstdlib>
#include "util/StringUtil.h"

namespace skyfall {

int INIReader::Parse(const std::string& filename) {
	FILE *file = std::fopen(filename.data(), "r");
	if (nullptr == file) {
		std::fprintf(stderr, "open %s failed(%d:%s)\n", filename.data(), errno, strerror(errno));
		return -1;
	}
	Clear();
	int32_t ret = parseFile(file);
	fclose(file);
	return ret;
}

void INIReader::Clear() {
	sections_.clear();
	fields_.clear();
}

int32_t INIReader::parseFile(FILE *file) {
	static const int32_t MAX_BUFF_LEN = 2048;
	char buff[MAX_BUFF_LEN] = {0};

	int32_t line_no = 0;
	std::string utf8bom;
	utf8bom.push_back(0xEF);
	utf8bom.push_back(0xBB);
	utf8bom.push_back(0xBF);
	std::map<std::string, std::string>* fields_map = nullptr;
	while (fgets(buff, MAX_BUFF_LEN, file) != nullptr) {
		line_no++;
		std::string line(buff);
		// 0. 支持UTF-8 BOM
		if (1 == line_no && line.find_first_of(utf8bom) == 0) {
			line.erase(0, 3);
		}
		// 1. 去掉注释
		for (size_t i = 0; i < line.length(); ++i) {
			if (';' == line[i] || '#' == line[i]) {
				line.erase(i);
				break;
			}
		}
		// 2. 去掉首尾空格
		line = StringUtil::Strip(line);
		// 3. 去掉空行
		if (line.empty()) {
			continue;
		}
		// section
		if (line[0] == '[' && line[line.length() - 1] == ']') {
			std::string section(line.substr(1, line.length() - 2));
			section = StringUtil::Strip(section);
			if (section.empty()) {
				return line_no;
			}
			sections_.insert(section);
			fields_map = &(fields_[section]);
			continue;
		}

		if (nullptr == fields_map) {
			continue;
		}
		// fileds
		size_t pos = line.find('=');
		if (std::string::npos == pos) {
			continue;
		}
		std::string key = line.substr(0, pos);
		std::string value = line.substr(pos + 1);
		key = StringUtil::Strip(key);
		value = StringUtil::Strip(value);
		if (key.empty() || value.empty()) {
			continue;
		}

		(*fields_map)[key] = value;
	}

	return 0;
}

std::string INIReader::GetString(const std::string& section, const std::string& name, const std::string& default_value) {
	auto it = fields_.find(section);
	if (it == fields_.end()) {
		return default_value;
	}

	auto& fields_map = it->second;
	auto cit = fields_map.find(name);
	if (fields_map.end() == cit) {
		return default_value;
	}

	return cit->second;
}

int32_t INIReader::GetInt32(const std::string& section, const std::string& name, int32_t default_value) {
	std::string value = GetString(section, name, "");
	const char *begin = value.c_str();
	char *end = nullptr;

	auto n = std::strtol(begin, &end, 0);
	if (end == begin + value.size() && errno != ERANGE && (INT32_MIN <= n && n <= INT32_MAX)) {
		return static_cast<int32_t>(n);
	}
	return default_value;
}

uint32_t INIReader::GetUInt32(const std::string& section, const std::string& name, uint32_t default_value) {
	std::string value = GetString(section, name, "");
	const char *begin = value.c_str();
	char *end = nullptr;

	auto n = std::strtoul(begin, &end, 0);
	if (end == begin + value.size() && errno != ERANGE && n <= UINT32_MAX) {
		return static_cast<uint32_t>(n);
	}
	return default_value;
}

int64_t INIReader::GetInt64(const std::string& section, const std::string& name, int64_t default_value) {
	std::string value = GetString(section, name, "");
	const char *begin = value.c_str();
	char *end = nullptr;

	auto n = std::strtoll(begin, &end, 0);
	if (end == begin + value.size() && errno != ERANGE && (INT64_MIN <= n && n <= INT64_MAX)) {
		return static_cast<int64_t>(n);
	} else {
		return default_value;
	}
}

uint64_t INIReader::GetUInt64(const std::string& section, const std::string& name, uint64_t default_value) {
	std::string value = GetString(section, name, "");
	const char *begin = value.c_str();
	char *end = nullptr;

	auto n = std::strtoull(begin, &end, 0);
	if (end == begin + value.size() && errno != ERANGE && n <= UINT64_MAX) {
		return static_cast<uint64_t>(n);
	}
	return default_value;
}

double INIReader::GetReal(const std::string& section, const std::string& name, double default_value) {
	std::string value = GetString(section, name, "");
	const char *begin = value.c_str();
	char *end = nullptr;

	double n = std::strtod(begin, &end);
	if (end != begin && *end == '\0') {
		return n;
	} else {
		return default_value;
	}
}

bool INIReader::GetBoolean(const std::string& section, const std::string& name, bool default_value) {
	std::string value = GetString(section, name, "");
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});

	if (value == "true" || value == "1") {
		return true;
	} else if (value == "false" || value == "0") {
		return false;
	} else {
		return default_value;
	}
}

const INIReader::FieldsMap *INIReader::GetFields(const std::string& section) {
	auto it = fields_.find(section);
	if (it == fields_.end()) {
		return nullptr;
	}
	return &it->second;
}

} // namespace skyfall