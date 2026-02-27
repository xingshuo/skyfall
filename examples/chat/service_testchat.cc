#include "service_testchat.h"
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <cstdio>
#include <iterator>
#include "util/StringUtil.h"

static const char *const GMessageTable[] = {
	"Hello world",
	"Nice to meet you",
	"See you tomorrow",
	"Have a good night",
	"Goodbye"
};

static const int kDelayChatMs = 2000;

void TestChat::PrepareChat(skyfall::ContextSPtr ctx) {
	auto session = NewSession();
	SKYFALL_INFO(ctx, "%s new chat timer session: %d delay time:%d", GetName().c_str(), session, kDelayChatMs);
	skyfall::Timeout(ctx->GetHandle(), kDelayChatMs, session);
}

void TestChat::HandleRequest(skyfall::ContextSPtr ctx, int session, uint32_t source, const void *msg, size_t sz) {
	SKYFALL_INFO(ctx, "%s recv ping msg '%.*s' session:%d from peer %x", GetName().c_str(),static_cast<int>(sz), static_cast<const char*>(msg), session, source);
	ctx->SendName(GetPeer(), skyfall::PTYPE_RESPONSE, session, const_cast<void*>(msg), sz);
}

void TestChat::HandleResponse(skyfall::ContextSPtr ctx, int session, uint32_t source, const void *msg, size_t sz) {
	static size_t cursor = 0;
	if (m_requests.count(session) > 0) { // chat msg response
		SKYFALL_INFO(ctx, "%s recv pong msg '%.*s' session:%d from peer %x", GetName().c_str(),static_cast<int>(sz), static_cast<const char*>(msg), session, source);
		if (cursor >= std::size(GMessageTable)) {
			auto peer_handle = skyfall::FindHandle(GetPeer());
			SKYFALL_INFO(ctx, "exit peer: %s", GetPeer().c_str());
			skyfall::ContextExit(peer_handle);
			SKYFALL_INFO(ctx, "exit self: %s", GetName().c_str());
			skyfall::ContextExit(ctx->GetHandle());
		} else {
			PrepareChat(ctx);
		}
	} else { // timer response
		auto session = NewSession();
		m_requests.insert(session);
		const char *msg = GMessageTable[cursor++];
		SKYFALL_INFO(ctx, "%s new chat text '%s' session: %d", GetName().c_str(), msg, session);
		ctx->SendName(GetPeer(), skyfall::PTYPE_TEXT, session, static_cast<void *>(const_cast<char*>(msg)), strlen(msg));
	}
}

static int
_cb(skyfall::ContextSPtr ctx, void *ud, int type, int session, uint32_t source, const void *msg, size_t sz) {
	SKYFALL_DEBUG(ctx, "run callback type:%d sesson:%d source:%u msg:%p sz:%llu", type, session, source, msg, sz);
	TestChat *app = static_cast<TestChat*>(ud);
	switch(type) {
	case skyfall::PTYPE_TEXT:
		app->HandleRequest(ctx, session, source, msg, sz);
		break;
	case skyfall::PTYPE_RESPONSE:
		app->HandleResponse(ctx, session, source, msg, sz);
		break;
	}
	return 0;
}

extern "C" void*
testchat_create(void) {
	return static_cast<void *>(new TestChat());
}

extern "C" void
testchat_release(TestChat* app) {
	delete app;
}

extern "C" int
testchat_init(TestChat *app, skyfall::ContextSPtr ctx, char *parm) {
	if (parm == nullptr || parm[0] == '\0' || strcmp(parm, "bootstrap") == 0) {
		SKYFALL_INFO(ctx, "bootstrap start!");
		SKYFALL_INFO(ctx, "bootstrap service handle: %x", ctx->GetHandle());

		auto Bob = skyfall::ContextNew("testchat", "Bob Alice");
		assert(Bob != nullptr);
		skyfall::SetHandleName(Bob->GetHandle(), "Bob");

		auto Alice = skyfall::ContextNew("testchat", "Alice Bob");
		assert(Alice != nullptr);
		skyfall::SetHandleName(Alice->GetHandle(), "Alice");
	
		SKYFALL_INFO(ctx, "bootstrap init done");
		skyfall::ContextExit(ctx->GetHandle());
		SKYFALL_INFO(ctx, "bootstrap exit!");
	} else {
		auto ret = skyfall::StringUtil::Split(parm, ' ');
		auto my_name = ret[0];
		auto peer_name = ret[1];
		SKYFALL_INFO(ctx, "%s start!", my_name.c_str());
		SKYFALL_INFO(ctx, "%s service handle: %x", my_name.c_str(), ctx->GetHandle());
		app->SetName(my_name);
		app->SetPeer(peer_name);
		app->SetContext(ctx);
		ctx->SetCallback(_cb, app);
		if (my_name == "Bob") {
			app->PrepareChat(ctx);
		}
	}
	return 0;
}