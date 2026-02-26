#ifndef SKYFALL_MONITOR_H
#define SKYFALL_MONITOR_H

#include <atomic>

namespace skyfall {

class Monitor final {
public:
	Monitor();
	void Trigger(uint32_t source, uint32_t destination);
	void Check();

private:
	std::atomic<int> version_;
	int check_version_;
	uint32_t source_;
	uint32_t destination_;
};

} // namespace skyfall

#endif