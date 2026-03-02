#include "util/StringUtil.h"
#include <algorithm>
#include <cstdarg>

namespace skyfall {

std::vector<std::string> StringUtil::Split(std::string_view str, char sep, int count) {
	std::vector<std::string> res;
	if (count == 0) {
		res.emplace_back(str);
		return res;
	}
	std::string_view::const_iterator cur = str.begin();
	std::string_view::const_iterator end = str.end();
	std::string_view::const_iterator next = std::find(cur, end, sep);

	while (next != end) {
		res.emplace_back(cur, next);
		cur = next + 1;
		if (count > 0 && --count == 0) {
			next = end;
			break;
		}
		next = std::find(cur, end, sep);
	}

	res.emplace_back(cur, next);
	return res;
}

std::string StringUtil::Format(const char* fmt, ...) {
	if (fmt == nullptr) {
		return std::string("");
	}

	static const int buf_sz = 1024;
	std::string res;
	res.resize(buf_sz);

	va_list ap;
	va_start(ap, fmt);
	// NOTICE: 返回的是预期写入的字节数，而非实际写入的，不包括结尾的'\0'
	int len = vsnprintf(res.data(), res.size(), fmt, ap);
	va_end(ap);

	if (len < 0) { // 格式化失败
		return std::string("");
	}
	if (len < static_cast<int>(buf_sz)) { // 完全写入
		res.resize(len);
		return res;
	}
	// 重置为加上'\0'的长度
	res.resize(len + 1);
	va_start(ap, fmt);
	vsnprintf(res.data(), res.size(), fmt, ap);
	va_end(ap);
	// 重置为移除'\0'的长度
	res.resize(len);
	return res;
}

std::string StringUtil::Strip(std::string_view str)
{
	std::string res;
	if (!str.empty()) {
		const char *cur = str.data();
		const char *end = cur + str.size();

		while (cur < end) {
			if (!isspace(*cur)) {
				break;
			}
			cur++;
		}

		while (end > cur) {
			if (!isspace(*(end - 1))) {
				break;
			}
			end--;
		}

		if (end > cur) {
			res.assign(cur, end - cur);
		}
	}
	return res;
}

} // namespace skyfall