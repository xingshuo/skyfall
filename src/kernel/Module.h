#ifndef SKYFALL_MODULE_H
#define SKYFALL_MODULE_H

#include <string>
#include <dlfcn.h>
#include <unordered_map>
#include <string_view>
#include "kernel/Log.h"
#include "kernel/Skyfall.h"

namespace skyfall {

typedef void *(*DLCreate)(std::string_view param);
typedef int (*DLInit)(void *inst, ContextSPtr ctx, std::string_view param);
typedef void (*DLRelease)(void *inst);
typedef void (*DLSignal)(void *inst, int signo);

class Module final {
public:
	Module(std::string_view name, void *dl);
	void *Create(std::string_view param);
	int Init(void *inst, ContextSPtr ctx, std::string_view param);
	void Release(void *inst);
	void Signal(void *inst, int signo);

private:
	int openSys();
	friend class ModuleManager;

private:
	std::string name_;
	void *module_;
	DLCreate create_;
	DLInit init_;
	DLRelease release_;
	DLSignal signal_;
	bool is_opened_;
};

class ModuleManager final {
public:
	static ModuleManager& Instance() {
		static ModuleManager m;
		return m;
	}
	void Init(std::string_view path) {
		path_ = path;
		SKYFALL_INFO(nullptr, "ModuleManager Init %s", path_.data());
	}
	Module *Query(const std::string& name);

private:
	ModuleManager() = default;
	~ModuleManager() = default;
	void *openDL(std::string_view name);

private:
	std::mutex mutex_;
	std::string path_;
	std::unordered_map<std::string, Module*> modules_;
};

} // namespace skyfall

#endif