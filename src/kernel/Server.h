#ifndef SKYFALL_SERVER_H
#define SKYFALL_SERVER_H

#include <stdint.h>
#include <memory>
#include <cassert>
#include <unordered_map>
#include <shared_mutex>
#include <functional>
#include <atomic>
#include "kernel/Skyfall.h"
#include "kernel/Config.h"
#include "kernel/MsgQueue.h"

namespace skyfall {

class Module;

using ContextCallback = std::function<int (ContextSPtr ctx, void *ud, int type, int session, uint32_t source , const void *msg, size_t sz)>;

class Context final : public std::enable_shared_from_this<Context> {
public:
	Context(Module *mod, void *inst, uint32_t handle);
	~Context();
	uint32_t GetHandle() {
		return handle_;
	}
	int Send(uint32_t destination, int type, int session, void *data, size_t sz);
	int SendName(const std::string& addr, int type, int session, void *data, size_t sz);
	void SetCallback(ContextCallback cb, void *ud = nullptr);

	static int SendTo(uint32_t source, uint32_t destination, int type, int session, void *data, size_t sz, ContextSPtr ctx = nullptr);
	static int SendToName(uint32_t source, const std::string& addr, int type, int session, void *data, size_t sz, ContextSPtr ctx = nullptr);
	friend class HandleStorage;
	friend class Scheduler;

private:
	void dispatch(Message *msg);

private:
	Module *module_;
	void *instance_;
	uint32_t handle_;
	MsgQueue *queue_;
	ContextCallback callback_;
	void *cb_ud_;
};

class Node final {
public:
	static Node& Instance() {
		static Node node;
		return node;
	}
	static void InitThread(ThreadType t) {
		Node::cur_handle_ = static_cast<uint32_t>(-static_cast<int>(t));
	}
	static uint32_t CurrentHandle() {
		return Node::cur_handle_;
	}
	void Init() {
		total_ctx_.store(0);
	}
	int ContextTotal() {
		return total_ctx_.load(std::memory_order_relaxed);
	}
	int GetEnv(const std::string& key, std::string *value);
	int SetEnv(const std::string& key, const std::string& value);
	friend class Context;

private:
	Node() = default;
	~Node() = default;

private:
	static thread_local uint32_t cur_handle_;
	std::atomic<int> total_ctx_;
	mutable std::shared_mutex mutex_;
	std::unordered_map<std::string, std::string> env_;
};

int ContextPush(uint32_t handle, Message& msg);

} // namespace skyfall

#endif