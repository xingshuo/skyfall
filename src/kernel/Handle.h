#ifndef SKYFALL_HANDLE_H
#define SKYFALL_HANDLE_H

#include <unordered_map>
#include <shared_mutex>
#include <stdint.h>
#include "kernel/Log.h"
#include "kernel/Server.h"

namespace skyfall {

class HandleStorage {
public:
	static HandleStorage& Instance() {
		static HandleStorage h;
		return h;
	}
	void Init() {
		SKYFALL_INFO(nullptr, "HandleStorage Init");
	}
	ContextSPtr NewContext(const char *name, const char *param);
	int RetireContext(uint32_t handle);
	void RetireAllContext();
	ContextSPtr FindContext(uint32_t handle);
	int PushContext(uint32_t handle, Message& msg);
	int SetHandleName(const std::string& name, uint32_t handle);
	uint32_t FindHandle(const std::string& name);

private:
	HandleStorage() {
		handle_index_ = 1;
	}
	~HandleStorage() = default;

private:
	mutable std::shared_mutex mutex_;
	std::unordered_map<uint32_t, ContextSPtr> contexts_;
	std::unordered_map<std::string, uint32_t> handles_;
	uint32_t handle_index_;
};

} // namespace skyfall

#endif