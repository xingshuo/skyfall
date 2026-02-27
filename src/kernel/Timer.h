#ifndef SKYFALL_TIMER_H
#define SKYFALL_TIMER_H

#include <mutex>
#include <map>
#include <thread>
#include <ctime>
#include "kernel/Skyfall.h"
#include "kernel/Log.h"
#include "kernel/Server.h"
#include "util/TimeUtil.h"

namespace skyfall {

class TimerManager final {
public:
	static TimerManager& Instance() {
		static TimerManager t;
		return t;
	}
	void Init() {
		now_ = TimeUtil::Now();
		SKYFALL_INFO(nullptr, "TimerManager Init");
	}

	TimerManager(const TimerManager&) = delete;
	TimerManager& operator=(const TimerManager&) = delete;

	int Timeout(uint32_t handle, int64_t interval_ms, int session);
	std::time_t Now() const {
		return now_;
	};
	void Update();
	size_t Size() const;

private:
	TimerManager() = default;
	~TimerManager() = default;

	template<typename... Args>
	void add(time_t expired_time, Args&&... args) {
		timers_.emplace(expired_time, TimerNode{ std::forward<Args>(args)... });
	}

private:
	class TimerNode {
	public:
		TimerNode(uint32_t handle, int session) {
			handle_ = handle;
			session_ = session;
		}
		void operator()() const {
			SKYFALL_DEBUG(nullptr, "timer run handle:%u: session:%d", handle_, session_);
			Message msg{0, session_, nullptr, (size_t)PTYPE_RESPONSE << MESSAGE_TYPE_SHIFT};
			ContextPush(handle_, msg);
		}
	private:
		uint32_t handle_;
		int session_;
	};

private:
	mutable std::mutex mutex_;
	std::multimap<int64_t, TimerNode> timers_;
	std::vector<TimerNode> expired_;
	std::time_t now_;
};

} // namespace skyfall

#endif