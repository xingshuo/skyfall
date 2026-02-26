#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include <chrono>

namespace skyfall {

class TimeUtil {
	using time_point = std::chrono::time_point<std::chrono::steady_clock>;
public:
	static std::time_t Now();

private:
	TimeUtil();
	~TimeUtil() = default;

private:
	time_point starttime_point_; // 单调 time_point
	std::time_t start_milliseconds_; // 启动绝对时间
};

} // namespace skyfall

#endif