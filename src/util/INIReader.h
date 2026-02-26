/*
	This file is modified version from https://github.com/Tencent/Pebble/blob/master/src/common/ini_reader.h
*/

#ifndef INIREADER_H
#define INIREADER_H

#include <stdint.h>
#include <string>
#include <map>
#include <set>


namespace skyfall {

class INIReader final {
public:
	typedef std::map<std::string, std::string> FieldsMap;

	INIReader() = default;
	~INIReader() = default;

	/// @brief 解析文件
	/// @return 0 成功
	/// @return <0 失败
	/// @note 每次调用会清除上一次的解析结果
	int32_t Parse(const std::string& filename);

	void Clear();

	/// @brief 获取string类型字段的值，字段未配置时使用默认值
	std::string GetString(const std::string& section, const std::string& name,
					const std::string& default_value);

	// Get an integer (long) value from INI file, returning default_value
	// if not found or not a valid integer (decimal "1234", "-1234",
	// or hex "0x4d2").
	int32_t GetInt32(const std::string& section, const std::string& name,
						int32_t default_value);

	uint32_t GetUInt32(const std::string& section, const std::string& name,
						uint32_t default_value);

	int64_t GetInt64(const std::string& section, const std::string& name,
						int64_t default_value);

	uint64_t GetUInt64(const std::string& section, const std::string& name,
						uint64_t default_value);

	// Get a real (floating point double) value from INI file, returning
	// default_value if not found or not a valid floating point value
	// according to strtod().
	double GetReal(const std::string& section, const std::string& name, double default_value);

	// Get a boolean value from INI file, returning default_value
	// if not found or if not a valid true/false value. Valid true
	// values are "true", "1", and valid false values are
	// "false", "0" (not case sensitive).
	bool GetBoolean(const std::string& section, const std::string& name, bool default_value);

	const std::set<std::string>& GetSections() const {
		return sections_;
	};

	const FieldsMap *GetFields(const std::string& section);

private:
	int32_t parseFile(FILE* file);

private:
	std::set<std::string> sections_;
	std::map<std::string, FieldsMap> fields_;
};

} // namespace skyfall

#endif