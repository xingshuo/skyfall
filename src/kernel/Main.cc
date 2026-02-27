#include <csignal>
#include <memory>
#include "kernel/Skyfall.h"
#include "kernel/Config.h"
#include "kernel/Log.h"
#include "kernel/Server.h"
#include "kernel/Timer.h"
#include "kernel/Handle.h"
#include "kernel/Module.h"
#include "kernel/Scheduler.h"
#include "util/StringUtil.h"
#include "util/INIReader.h"

static void registerSignal() {
	std::signal(SIGHUP, SIG_IGN);
	std::signal(SIGQUIT, SIG_IGN);
	std::signal(SIGPIPE, SIG_IGN);
}

int main(int argc, char *argv[]) {
	const char * config_file = nullptr;
	if (argc > 1) {
		config_file = argv[1];
	} else {
		fprintf(stderr, "Need a config file.\n");
		return 1;
	}

	registerSignal();

	skyfall::INIReader reader;
	int ret = reader.Parse(config_file);
	if (ret != 0) {
		fprintf(stderr, "Parse config file failed: %s, %d\n", config_file, ret);
		return 1;
	}
	skyfall::Config config;
	config.thread = reader.GetInt32("app", "thread", 4);
	config.module_path = reader.GetString("app", "module_path", ""); // ./examples/chat/?.so
	config.bootstrap = reader.GetString("app", "bootstrap", ""); // testchat bootstrap
	config.logfile = reader.GetString("log", "logfile", "");
	config.loglevel = reader.GetString("log", "loglevel", "INFO");

	skyfall::Logger::Instance().Init(config.logfile);
	skyfall::Logger::Instance().SetLevel(config.loglevel);

	skyfall::Node::Instance().Init();
	skyfall::HandleStorage::Instance().Init();
	skyfall::GlobalMQ::Instance().Init();
	skyfall::ModuleManager::Instance().Init(config.module_path);
	skyfall::TimerManager::Instance().Init();

	auto cmdline = skyfall::StringUtil::Split(config.bootstrap, ' ', 1);
	const char *name = cmdline[0].data();
	const char *args = "";
	if (cmdline.size() > 1) {
		args = cmdline[1].data();
	}
	if (skyfall::ContextNew(name, args) == nullptr) {
		SKYFALL_FATAL(nullptr, "Bootstrap error : %s", config.bootstrap);
	}

	auto sched = std::make_unique<skyfall::Scheduler>(config.thread);
	sched->Join();

	skyfall::Logger::Instance().Exit();

	return 0;	
}