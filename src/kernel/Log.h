#ifndef SKYFALL_LOG_H
#define SKYFALL_LOG_H

#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <mutex>
#include "util/StringUtil.h"
#include "kernel/Server.h"

namespace skyfall {	

enum class LogLevel {
	Fatal,
	Error,
	Warn,
	Info,
	Debug
};

class Logger final {
public:
	static Logger& Instance() {
		static Logger log;
		return log;
	}
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	void Init(const std::string& logfile);
	void Exit();
	void SetLevel(LogLevel lv) {
		level_ = lv;
	}
	void SetLevel(const std::string& lv);
	LogLevel GetLevel() const {
		return level_;
	}
	void Log(ContextSPtr ctx, LogLevel level, std::string_view msg);

	template<typename... Args>
	void LogF(ContextSPtr ctx, LogLevel level, const char *fmt, Args&&... args) {
		if (level_ < level) {
			return;
		}
		if (fmt == nullptr) {
			return;
		}
		std::string s = StringUtil::Format(fmt, std::forward<Args>(args)...);
		Log(ctx, level, std::string_view{s});
	}

private:
	Logger();
	~Logger();

	void write();

	struct LogItem;
	void doWrite(const LogItem& item);

private:
	enum class State {
		Init,
		Ready,
		Stopped
	};
	struct LogItem {
		LogItem(std::time_t ts, uint32_t src, LogLevel lv, std::string_view s) {
			ts = ts;
			source_ = src;
			level = lv;
			msg = s;
		};
		std::time_t ts;
		uint32_t source_;
		LogLevel level;
		std::string msg;
	};
	struct FileDeleter {
		void operator()(FILE* f) const {
			if (f == nullptr || f == stdout) {
				return;
			}
			std::fflush(f);
			std::fclose(f);
		}
	};
	class LogQueue {
	public:
		void Push(LogItem&& msg) {
			std::lock_guard<std::mutex> lock(mutex_);
			write_q_.push_back(std::forward<LogItem>(msg));
		}
		template<typename... Args>
		void Emplace(Args&&... args) {
			std::lock_guard<std::mutex> lock(mutex_);
			write_q_.emplace_back(std::forward<Args>(args)...);
		}
		std::vector<LogItem>& PopAll() {
			std::lock_guard<std::mutex> lock(mutex_);
			write_q_.swap(read_q_);
			return read_q_;
		}
	private:
		mutable std::mutex mutex_;
		std::vector<LogItem> read_q_;
		std::vector<LogItem> write_q_;
	};

	std::atomic<State> state_;
	std::atomic<LogLevel> level_;
	std::unique_ptr<std::FILE, FileDeleter> fp_;
	std::thread thread_;
	LogQueue log_queue_;
};

} // namespace skyfall

#define SKYFALL_DEBUG(ctx, fmt, ...) \
	skyfall::Logger::Instance().LogF(ctx, skyfall::LogLevel::Debug, fmt " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)

#define SKYFALL_INFO(ctx, fmt, ...) \
	skyfall::Logger::Instance().LogF(ctx, skyfall::LogLevel::Info, fmt " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)

#define SKYFALL_WARN(ctx, fmt, ...) \
	skyfall::Logger::Instance().LogF(ctx, skyfall::LogLevel::Warn, fmt " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)

#define SKYFALL_ERROR(ctx, fmt, ...) \
	skyfall::Logger::Instance().LogF(ctx, skyfall::LogLevel::Error, fmt " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)

#define SKYFALL_FATAL(ctx, fmt, ...) \
	do {		\
		skyfall::Logger::Instance().LogF(ctx, skyfall::LogLevel::Fatal, fmt " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__);	\
		skyfall::Logger::Instance().Exit();	\
		exit(1);	\
	} while (0)

#endif