#include "kernel/MsgQueue.h"
#include <mutex>
#include <vector>
#include <cassert>
#include "kernel/Skyfall.h"
#include "kernel/Server.h"

namespace skyfall {

GlobalMQ::GlobalMQ() {
	head_ = nullptr;
	tail_ = nullptr;
}

void GlobalMQ::Push(MsgQueue *q) {
	std::lock_guard<std::mutex> lock(mutex_);
	assert(q->next_ == nullptr);
	if (tail_ != nullptr) {
		tail_->next_ = q;
		tail_ = q;
	} else {
		head_ = tail_ = q;
	}
}

MsgQueue *GlobalMQ::Pop() {
	std::lock_guard<std::mutex> lock(mutex_);
	auto *q = head_;
	if (q != nullptr) {
		head_ = q->next_;
		if (head_ == nullptr) {
			assert(q == tail_);
			tail_ = nullptr;
		}
		q->next_ = nullptr;
	}
	return q;
}


MsgQueue::MsgQueue(uint32_t handle) {
	next_ = nullptr;
	handle_ = handle;
	in_global_ = true;
	is_release_ = false;
}

MsgQueue::~MsgQueue() {
	Message msg;
	while (!Pop(&msg)) {
		free(msg.data);
		// report error to the message source
		Context::SendTo(handle_, msg.source, PTYPE_ERROR, 0, nullptr, 0);
	}
	SKYFALL_DEBUG(nullptr, "msgqueue destroy handle: %x", handle_);
}

void MsgQueue::Push(Message& msg) {
	std::lock_guard<std::mutex> lock(mutex_);
	queue_.push(msg);
	if (!in_global_) {
		in_global_ = true;
		GlobalMQ::Instance().Push(this);
	}
}

int MsgQueue::Pop(Message *out) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (queue_.empty()) {
		in_global_ = false;
		return 1;
	}
	*out = queue_.front();
	queue_.pop();
	return 0;
}

size_t MsgQueue::Size() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return queue_.size();
}

void MsgQueue::MarkRelease() {
	std::lock_guard<std::mutex> lock(mutex_);
	assert(!is_release_);
	is_release_ = true;
	if (!in_global_) {
		GlobalMQ::Instance().Push(this);
	}
}

void MsgQueue::Release() {
	std::unique_lock<std::mutex> lock(mutex_);
	if (is_release_) {
		lock.unlock();
		delete this;
	} else {
		GlobalMQ::Instance().Push(this);
		SKYFALL_WARN(nullptr, "msgqueue re-push on release, handle:%x", handle_);
	}
}

} // namespace skyfall