#include "kernel/Module.h"
#include <mutex>
#include <cassert>
#include <dlfcn.h>
#include "util/StringUtil.h"

namespace skyfall {

Module::Module(std::string_view name, void* dl):
	name_(name),
	module_(dl),
	create_(nullptr),
	init_(nullptr),
	release_(nullptr),
	signal_(nullptr),
	is_opened_(false) {}

void *Module::Create() {
	if (create_ != nullptr) {
		return create_();
	} else {
		return (void *)(intptr_t)(~0);
	}
}

int Module::Init(void *inst, ContextSPtr ctx, std::string_view parm) {
	return init_(inst, ctx, parm);
}

void Module::Release(void *inst) {
	if (release_ != nullptr) {
		release_(inst);
	}
}

void Module::Signal(void *inst, int signo) {
	if (signal_ != nullptr) {
		signal_(inst, signo);
	}
}

int Module::openSys() {
	assert(!is_opened_);
	is_opened_ = true;

	create_ = (DLCreate)dlsym(module_, (name_ + "_create").c_str());
	init_ = (DLInit)dlsym(module_, (name_ + "_init").c_str());
	release_ = (DLRelease)dlsym(module_, (name_ + "_release").c_str());
	signal_ = (DLSignal)dlsym(module_, (name_ + "_signal").c_str());
	return init_ == nullptr;
}


void *ModuleManager::openDL(std::string_view name) {
	void *dl = nullptr;
	auto path_list = StringUtil::Split(path_, ';');
	for (auto iter = path_list.begin(); iter != path_list.end(); iter++) {
		auto pos = iter->find("?");
		if (pos == std::string::npos) {
			SKYFALL_FATAL(nullptr, "Invalid C service path: <%s>", path_.data());
		}
		iter->replace(pos, 1, name);
		dl = dlopen(iter->c_str(), RTLD_NOW | RTLD_GLOBAL);
		if (dl != nullptr) {
			break;
		}
	}

	if (dl == nullptr) {
		SKYFALL_ERROR(nullptr, "try open %s failed: %s",name.data(), dlerror());
	}
	return dl;
}

Module *ModuleManager::Query(const std::string& name) {
	std::lock_guard<std::mutex> lock { mutex_ };

	auto iter = modules_.find(name);
	if (iter != modules_.end()) {
		return iter->second;
	}
	void *dl = openDL(name);
	if (dl == nullptr) {
		return nullptr;
	}
	auto *mod = new Module(name, dl);
	if (mod->openSys() == 0) {
		modules_[name] = mod;
		return mod;
	} else {
		delete mod;
		SKYFALL_ERROR(nullptr, "open dl symbol error: %s", name.data());
		return nullptr;
	}
}

} // namespace skyfall