#include "util/TimeUtil.h"

namespace skyfall {

TimeUtil::TimeUtil() {
	starttime_point_ = std::chrono::steady_clock::now();
	start_milliseconds_ = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
}

std::time_t TimeUtil::Now() {
	static TimeUtil t;
	auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t.starttime_point_
	);
	return t.start_milliseconds_ + diff.count();
}

} // namespace skyfall