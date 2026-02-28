#ifndef SKYFALL_H
#define SKYFALL_H

#include <stdint.h>
#include <memory>

namespace skyfall {

enum : uint32_t {
	PTYPE_RESPONSE = 0,
	PTYPE_TEXT = 1,
	PTYPE_SYSTEM = 2,
	PTYPE_ERROR = 3,

	PTYPE_TAG_DONTCOPY = 0x10000,
};

class Context;
typedef std::weak_ptr<Context> ContextWPtr;
typedef std::shared_ptr<Context> ContextSPtr;

ContextSPtr ContextNew(const char *name, const char *param);
int Timeout(uint32_t handle, int64_t interval_ms, int session);
int SetHandleName(uint32_t handle, const char *name);
uint32_t FindHandle(const std::string& name);
void ContextExit(uint32_t handle);

} // namespace skyfall

#endif