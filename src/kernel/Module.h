#ifndef SKYFALL_MODULE_H
#define SKYFALL_MODULE_H

#include <string>
#include <dlfcn.h>
#include <unordered_map>
#include "kernel/Log.h"
#include "kernel/Skyfall.h"

namespace skyfall {

typedef void *(*DLCreate)(void);
typedef int (*DLInit)(void *inst, ContextSPtr ctx, const char *parm);
typedef void (*DLRelease)(void *inst);
typedef void (*DLSignal)(void *inst, int signo);

class Module final {
public:
	Module(const std::string& name, void *dl);
	void *Create();
	int Init(void *inst, ContextSPtr ctx, const char *parm);
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
};

class ModuleManager final {
public:
	static ModuleManager& Instance() {
		static ModuleManager m;
		return m;
	}
	void Init(const std::string& path) {
		path_ = path;
		SKYFALL_INFO(nullptr, "ModuleManager Init %s", path_.data());
	}
	Module *Query(const std::string& name);

private:
	ModuleManager() = default;
	~ModuleManager() = default;
	void *openDL(const std::string& name);

private:
	std::mutex mutex_;
	std::string path_;
	std::unordered_map<std::string, Module*> modules_;
};

} // namespace skyfall

#endif