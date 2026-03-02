#include "kernel/Timer.h"
#include <chrono>
#include "util/TimeUtil.h"

namespace skyfall {

int TimerManager::Timeout(uint32_t handle, int64_t interval_ms, int session) {
	if (interval_ms <= 0) {
		Message msg{0, session, nullptr, (size_t)PTYPE_RESPONSE << MESSAGE_TYPE_SHIFT};
		if (ContextPush(handle, msg)) {
			return -1;
		}
		return 0;
	}
	auto expired_time = now_ + interval_ms;
	if (expired_time <= 0) {
		SKYFALL_ERROR(nullptr, "timeout loopback, source:%u interval:%ld session:%d", handle, interval_ms, session);
		return -1;
	}
	addTimer(expired_time, handle, session);
	return 0;
}

void TimerManager::Update() {
	now_ = TimeUtil::Now();
	expired_.clear();
	{
		std::lock_guard<std::mutex> lock{ mutex_ };
		auto it = timers_.begin();
		while (it != timers_.end()) {
			if (it->first > now_) {
				break;
			}
			expired_.push_back(it->second);
			it = timers_.erase(it);
		}
	}
	for (auto& handler : expired_) {
		handler();
	}
}

size_t TimerManager::Size() const {
	std::lock_guard<std::mutex> lock{ mutex_ };
	return timers_.size();
}

int Timeout(uint32_t handle, int64_t interval_ms, int session) {
	return TimerManager::Instance().Timeout(handle, interval_ms, session);
}

} // namespace skyfall