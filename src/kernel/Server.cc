#include "kernel/Server.h"
#include <memory>
#include <atomic>
#include <cstring>
#include <string_view>
#include "kernel/Log.h"
#include "kernel/Module.h"
#include "kernel/Handle.h"
#include "kernel/Skyfall.h"

namespace skyfall {

thread_local uint32_t Node::cur_handle_ = static_cast<uint32_t>(-static_cast<int>(ThreadType::Main));

int Node::GetEnv(const std::string& key, std::string *value) {
	std::shared_lock<std::shared_mutex> lock(mutex_);
	auto iter = env_.find(key);
	if (iter == env_.end()) {
		return 1;
	}
	*value = iter->second;
	return 0;
}

int Node::SetEnv(const std::string& key, const std::string& value) {
	std::unique_lock<std::shared_mutex> lock(mutex_);
	if (env_.find(key) != env_.end()) {
		return 1;
	}
	env_[key] = value;
	return 0;
}

Context::Context(Module *mod, void *inst, uint32_t handle):
	module_(mod), instance_(inst),
	handle_(handle), is_endless_signal_(false), queue_(new MsgQueue(handle)),
	callback_(nullptr), cb_ud_(nullptr)	{
	Node::Instance().total_ctx_++;
}

Context::~Context() {
	module_->Release(instance_);
	// NOTICE: queue_延迟销毁
	queue_->MarkRelease();
	Node::Instance().total_ctx_--;
	SKYFALL_DEBUG(nullptr, "context destroy handle: %x", handle_);
}

int Context::Send(uint32_t destination, int type, int session, void *data, size_t sz) {
	return Context::SendTo(handle_, destination, type, session, data, sz, shared_from_this());
}

int Context::SendName(const std::string& addr, int type, int session, void *data, size_t sz) {
	return Context::SendTo(handle_, HandleStorage::Instance().FindHandle(addr), type, session, data, sz, shared_from_this());
}

int Context::SendTo(uint32_t source, uint32_t destination, int type, int session, void *data, size_t sz, ContextSPtr ctx) {
	int nocopy = type & PTYPE_TAG_DONTCOPY;
	if (destination == 0) {
		SKYFALL_ERROR(ctx, "Destination address can't be 0, source: %x", source);
		if (data != nullptr && nocopy) {
			free(data);
		}
		return -1;
	}
	if ((sz & MESSAGE_TYPE_MASK) != sz) {
		SKYFALL_ERROR(ctx, "The message to %x is too large, source: %x", destination, source);
		if (data != nullptr && nocopy) {
			free(data);
		}
		return -2;
	}
	if (!nocopy && data != nullptr) {
		char *new_data = static_cast<char *>(malloc(sz + 1));
		memcpy(new_data, data, sz);
		new_data[sz] = '\0';
		data = new_data;
	}
	type &= 0xff;
	sz |= (size_t)type << MESSAGE_TYPE_SHIFT;

	Message msg{source, session, data, sz};
	if (ContextPush(destination, msg)) {
		SKYFALL_ERROR(ctx, "Send Message to %x failed, source: %x", destination, source);
		if (data != nullptr) {
			// NOTICE: always free here!
			free(data);
		}
		return -3;
	}
	return 0;
}

int Context::SendToName(uint32_t source, const std::string& addr, int type, int session, void *data, size_t sz, ContextSPtr ctx) {
	return Context::SendTo(source, HandleStorage::Instance().FindHandle(addr), type, session, data, sz, ctx);
}

void Context::SetCallback(ContextCallback cb, void *ud) {
	callback_ = std::move(cb);
	cb_ud_ = ud;
}

void Context::Signal(int signo) {
	module_->Signal(instance_, signo);
}

void Context::EndlessSignalEnable(int enable) {
	is_endless_signal_ = static_cast<bool>(enable);
}

void Context::dispatch(Message *msg) {
	if (callback_ == nullptr) {
		free(msg->data);
		return;
	}
	Node::cur_handle_ = handle_;
	int type = msg->sz >> MESSAGE_TYPE_SHIFT;
	size_t sz = msg->sz & MESSAGE_TYPE_MASK;
	int reserve_msg = callback_(shared_from_this(), cb_ud_, type, msg->session, msg->source, msg->data, sz);
	if (!reserve_msg) {
		free(msg->data);
	}
}

void Context::endless() {
	if (is_endless_signal_) {
		Signal(0);
	}
}

ContextSPtr ContextNew(const std::string& name, std::string_view param) {
	return HandleStorage::Instance().NewContext(name, param);
}

int ContextPush(uint32_t handle, Message& msg) {
	return HandleStorage::Instance().PushContext(handle, msg);
}

void ContextExit(uint32_t handle) {
	SKYFALL_INFO(nullptr, "KILL :%x source: %x", handle, Node::Instance().CurrentHandle());
	HandleStorage::Instance().RetireContext(handle);
}

void Abort() {
	SKYFALL_WARN(nullptr, "Abort!");
	HandleStorage::Instance().RetireAllContext();
}

} // namespace skyfall