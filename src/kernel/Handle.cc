#include "kernel/Handle.h"
#include <mutex>
#include <vector>
#include <cassert>
#include <memory>
#include "kernel/Log.h"
#include "kernel/Module.h"
#include "kernel/Server.h"
#include "kernel/MsgQueue.h"

namespace skyfall {

ContextSPtr HandleStorage::NewContext(const std::string& name, std::string_view param) {
	Module *mod = ModuleManager::Instance().Query(name);
	if (mod == nullptr) {
		return nullptr;
	}
	void *inst = mod->Create(param);
	if (inst == nullptr) {
		return nullptr;
	}

	ContextSPtr ctx = nullptr;
	{
		std::unique_lock<std::shared_mutex> lock(mutex_);
		uint32_t handle = handle_index_;
		while (1) {
			if (handle == 0) {
				handle = 1;
			}
			if (contexts_.find(handle) == contexts_.end()) {
				ctx = std::make_shared<Context>(mod, inst, handle);
				contexts_[handle] = ctx;
				handle_index_ = handle + 1;
				break;
			}
			handle++;
		}
	}

	MsgQueue *q = ctx->queue_;
	int ret = mod->Init(inst, ctx, param);
	if (ret == 0) {
		GlobalMQ::Instance().Push(q);
		SKYFALL_INFO(ctx, "LAUNCH SUCCEED %s %s", name.data(), param.data());
		return ctx;
	} else {
		SKYFALL_ERROR(ctx, "LAUNCH FAILED %s %s", name.data(), param.data());
		RetireContext(ctx->handle_);
		q->Release();
		return nullptr;
	}
}

int HandleStorage::RetireContext(uint32_t handle) {
	std::unique_lock<std::shared_mutex> lock(mutex_);
	 auto iter = contexts_.find(handle);
	 if (iter == contexts_.end()) {
		  return 0;
	 }
	 int ret = 0;
	 auto ctx = iter->second;
	 if (ctx != nullptr && ctx->handle_ == handle) {
			ret = 1;
			contexts_.erase(handle);
			for (auto it = handles_.begin(); it != handles_.end(); ) {
				if (it->second == handle) {
					it = handles_.erase(it);
				} else {
					++it;
				}
			}
	 }
	 return ret;
}

void HandleStorage::RetireAllContext() {
	std::vector<uint32_t> handles;
	{
		std::shared_lock<std::shared_mutex> lock(mutex_);
		for (const auto& pair : contexts_) {
			handles.emplace_back(pair.first);
		}
	}
	for (const auto& hdl : handles) {
		RetireContext(hdl);
	}
}

ContextSPtr HandleStorage::FindContext(uint32_t handle) const {
	std::shared_lock<std::shared_mutex> lock(mutex_);
	auto iter = contexts_.find(handle);
	if (iter == contexts_.end()) {
		return nullptr;
	}
	return iter->second;
}

int HandleStorage::PushContext(uint32_t handle, Message& msg) const {
	auto ctx = FindContext(handle);
	if (ctx == nullptr) {
		SKYFALL_WARN(nullptr, "push context failed, context not find %x", handle);
		return -1;
	}
	ctx->queue_->Push(msg);
	return 0;
}

int HandleStorage::SetHandleName(const std::string& name, uint32_t handle) {
	std::unique_lock<std::shared_mutex> lock(mutex_);
	if (contexts_.find(handle) == contexts_.end()) {
		return -1;
	}
	if (handles_.find(name) != handles_.end()) {
		return -2;
	}
	handles_[name] = handle;
	return 0;
}

uint32_t HandleStorage::FindHandle(const std::string& name) const {
	std::shared_lock<std::shared_mutex> lock(mutex_);
	auto iter = handles_.find(name);
	if (iter == handles_.end()) {
		return 0;
	}
	return iter->second;
}

int SetHandleName(uint32_t handle, const std::string& name) {
	return HandleStorage::Instance().SetHandleName(name, handle);
}

uint32_t FindHandle(const std::string& name) {
	return HandleStorage::Instance().FindHandle(name);
}

} // namespace skyfall