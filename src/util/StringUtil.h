#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#include <string>
#include <string_view>
#include <vector>

namespace skyfall {

class StringUtil
{
public:
	static std::vector<std::string> Split(std::string_view str, char sep, int count = -1);
	static std::string Format(const char* fmt, ...);
	static std::string Strip(std::string_view str);
};

} // namespace skyfall

#endif

