#ifndef SKYFALL_CONFIG_H
#define SKYFALL_CONFIG_H

namespace skyfall {

struct Config {
	int thread;
	std::string module_path;
	std::string bootstrap;
	std::string logfile;
	std::string loglevel;
};

enum class ThreadType : int {
	Worker,
	Main,
	Timer,
	Monitor
};

} // namespace skyfall

#endif