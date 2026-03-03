#include "kernel/Monitor.h"
#include "kernel/Server.h"
#include "kernel/Log.h"

namespace skyfall {

Monitor::Monitor() {
	version_.store(0);
	check_version_ = 0;
	source_ = 0;
	destination_ = 0;
}

void Monitor::Trigger(uint32_t source, uint32_t destination) {
	source_ = source;
	destination_ = destination;
	version_++;
}

void Monitor::Check() {
	auto version = version_.load();
	if (version == check_version_) {
		if (destination_) {
			SKYFALL_ERROR(nullptr, "A message from [ :%08x ] to [ :%08x ] maybe in an endless loop (version = %d)", source_, destination_, version);
		}
	} else {
		check_version_ = version;
	}
}

} // namespace skyfall