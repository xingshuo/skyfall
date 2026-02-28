#ifndef SKYFALL_MSG_QUEUE_H
#define SKYFALL_MSG_QUEUE_H

#include <cstdint>
#include <vector>
#include <queue>
#include <mutex>
#include "kernel/Log.h"

namespace skyfall {

struct Message {
	uint32_t source;
	int session;
	void *data;
	size_t sz;
};

// type is encoding in Message.sz high 8bit
enum : size_t {
    MESSAGE_TYPE_MASK  = SIZE_MAX >> 8,
    MESSAGE_TYPE_SHIFT = (sizeof(size_t) - 1) * 8
};

class MsgQueue final {
public:
	MsgQueue(uint32_t handle);
	void Push(Message& msg);
	int Pop(Message *out);
	size_t Size() const;
	void MarkRelease();
	void Release();
	uint32_t GetHandle() {
		return handle_;
	}
	friend class GlobalMQ;

private:
	~MsgQueue();

private:
	MsgQueue *next_;
	uint32_t handle_;
	bool in_global_;
	bool is_release_;
	mutable std::mutex mutex_;
	std::queue<Message> queue_;
};

class GlobalMQ final {
public:
	static GlobalMQ& Instance() {
		static GlobalMQ mq;
		return mq;
	}
	void Init() {
		SKYFALL_INFO(nullptr, "GlobalMQ Init");
	}
	GlobalMQ(const GlobalMQ&) = delete;
	GlobalMQ& operator=(const GlobalMQ&) = delete;

	void Push(MsgQueue *q);
	MsgQueue *Pop();

private:
	GlobalMQ();
	~GlobalMQ() = default;

private:
	MsgQueue *head_;
	MsgQueue *tail_;
	std::mutex mutex_;
};

} // namespace skyfall

#endif