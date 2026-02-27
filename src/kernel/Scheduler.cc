#include "kernel/Scheduler.h"
#include "kernel/Timer.h"
#include "kernel/Log.h"

namespace skyfall {

#define CHECK_EXIT if (skyfall::Node::Instance().ContextTotal() == 0) break;

Scheduler::Scheduler(int N) {
	worker_num_ = N;
	sleep_num_.store(0);
	quit_ = false;
	monitors_ = std::make_unique<Monitor[]>(N);
	threads_ = std::make_unique<std::thread[]>(N+2);
	threads_[0] = std::thread{&Scheduler::monitorRoutine, this};
	threads_[1] = std::thread{&Scheduler::timerRoutine, this};
	for (int i = 0; i < N; i++) {
		threads_[i+2] = std::thread{&Scheduler::workerRoutine, this, i};
	}
}

Scheduler::~Scheduler() {
	Join();
}

void Scheduler::Join() {
	for (int i = 0; i < worker_num_+2; i++) {
		if (threads_[i].joinable()) {
			threads_[i].join();
		}
	}
	SKYFALL_INFO(nullptr, "scheduler quit!");
}

void Scheduler::workerRoutine(int id) {
	Node::InitThread(ThreadType::Worker);
	Monitor *m = &monitors_[id];
	while (!quit_) {
		MsgQueue *q = GlobalMQ::Instance().Pop();
		if (q == nullptr) {
			std::unique_lock<std::mutex> lock(mutex_);
			sleep_num_++;
			if (!quit_) {
				cond_.wait(lock);
			}
			sleep_num_--;
		} else {
			dispatch(m, q);
		}
	}
}

void Scheduler::monitorRoutine() {
	Node::InitThread(ThreadType::Monitor);
	while (1) {
		CHECK_EXIT
		for (int i = 0; i < worker_num_; i++) {
			monitors_[i].Check();
		}
		for (int i = 0; i < 5; i++) {
			CHECK_EXIT
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
}

void Scheduler::timerRoutine() {
	Node::InitThread(ThreadType::Timer);
	while (1) {
		TimerManager::Instance().Update();
		CHECK_EXIT
		wakeup(1);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	std::lock_guard<std::mutex> lock(mutex_);
	quit_ = true;
	cond_.notify_all();
}

void Scheduler::dispatch(Monitor *m, MsgQueue *q) {
	auto handle = q->GetHandle();
	auto ctx = HandleStorage::Instance().FindContext(handle);
	if (ctx == nullptr) {
		q->Release();
		return;
	}
	size_t n = q->Size();
	if (n == 0) {
		n = 1;
	}
	Message msg;
	for (size_t i = 0; i < n; i++) {
		if (q->Pop(&msg)) {
			return;
		}
		m->Trigger(msg.source, handle);
		ctx->dispatch(&msg);
		m->Trigger(0, 0);
	}
	GlobalMQ::Instance().Push(q);
}

void Scheduler::wakeup(int count) {
	auto sleep_num = sleep_num_.load(std::memory_order_relaxed);
	if (sleep_num >= count) {
		cond_.notify_one();
	}
}

} // namespace skyfall