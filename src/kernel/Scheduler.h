#ifndef SKYFALL_SCHEDULER_H
#define SKYFALL_SCHEDULER_H

#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <memory>
#include "kernel/Monitor.h"
#include "kernel/MsgQueue.h"
#include "kernel/Handle.h"

namespace skyfall {

class Scheduler final {
public:
	explicit Scheduler(int N);
	~Scheduler();
	void Join();

private:
	void workerRoutine(int id);
	void monitorRoutine();
	void timerRoutine();

	void dispatch(Monitor *m, MsgQueue *q);
	void wakeup(int count);

private:
	std::mutex mutex_;
	std::condition_variable cond_;
	std::unique_ptr<std::thread[]> threads_;
	std::unique_ptr<Monitor[]> monitors_;
	int worker_num_;
	std::atomic<int> sleep_num_;
	bool quit_;
};

} // namespace skyfall

#endif