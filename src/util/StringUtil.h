#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#include <string>
#include <vector>

namespace skyfall {

class StringUtil
{
public:
	static std::vector<std::string> Split(const std::string& str, char sep, int count = -1);
	static std::string Format(const char* fmt, ...);
	static std::string Strip(const std::string& str);
};

} // namespace skyfall

#endif

