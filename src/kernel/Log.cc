#include "kernel/Log.h"
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <ctime>
#include "util/TimeUtil.h"

namespace skyfall {

Logger::Logger() {
	state_ = State::Init;
	level_ = LogLevel::Info;
	thread_ = std::thread{&Logger::write, this};
}

Logger::~Logger() {
	Exit();
}

void Logger::Init(const std::string& logfile) {
	if (!logfile.empty()) {
		FILE *fp = std::fopen(logfile.data(), "w");
		assert(fp != nullptr);
		fp_.reset(fp);
	} else {
		fp_.reset(stdout);
	}
	state_.store(State::Ready);
}

void Logger::Exit() {
	if (state_.exchange(State::Stopped) == State::Stopped) {
		return;
	}
	if (thread_.joinable()) {
		thread_.join();
	}
	fp_.reset(nullptr);
}

void Logger::SetLevel(const std::string& lv) {
	if (lv == "DEBUG") {
		SetLevel(LogLevel::Debug);
	} else if (lv == "INFO") {
		SetLevel(LogLevel::Info);
	} else if (lv == "WARN") {
		SetLevel(LogLevel::Warn);
	} else if (lv == "ERROR") {
		SetLevel(LogLevel::Error);
	}
}

void Logger::Log(ContextSPtr ctx, LogLevel level, std::string_view msg) {
	if (level_ < level) {
		return;
	}
	auto ts = TimeUtil::Now();
	auto source = (ctx == nullptr) ? 0 : ctx->GetHandle();
	log_queue_.Emplace(ts, source, level, msg);
}

void Logger::write() {
	while (state_.load() == State::Init) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	while (state_.load() == State::Ready) {
		auto& mq = log_queue_.PopAll();
		if (mq.empty()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		} else {
			for (auto& item : mq) {
				doWrite(item);
			}
			mq.clear();
		}
	}

	auto& q = log_queue_.PopAll();
	for (auto& item : q) {
		doWrite(item);
	}
	q.clear();
}

static constexpr std::string_view levelString(LogLevel lv) {
	switch (lv) {
		case LogLevel::Fatal:
			return "FATAL";
		case LogLevel::Error:
			return "ERROR";
		case LogLevel::Warn:
			return "WARN";
		case LogLevel::Info:
			return "INFO";
		case LogLevel::Debug:
			return "DEBUG";
		default:
			return "UNKNOWN";
	}
}

void Logger::doWrite(const LogItem& item) {
	struct tm info;
	std::time_t ti = item.ts / 1000;
	int msec = static_cast<int>(item.ts % 1000);
	(void)localtime_r(&ti, &info);
	char time_fmt[256];
	strftime(time_fmt, sizeof(time_fmt), "%d/%m/%y %H:%M:%S", &info);

	std::string_view level_s = levelString(item.level);
	std::fprintf(fp_.get(), "%s.%03d [:%08x] [%.*s] ", time_fmt, msec, item.source_, static_cast<int>(level_s.length()), level_s.data());
	auto& msg = item.msg;
	std::fwrite(msg.data(), msg.size(), 1, fp_.get());
	if (msg.back() != '\n') {
		std::fputc('\n', fp_.get());
	}
	std::fflush(fp_.get());
}

} // namespace skyfall