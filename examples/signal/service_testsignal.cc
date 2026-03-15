#include "service_testsignal.h"

int SignalService::Init(skyfall::ContextSPtr ctx) {
	auto handle = ctx->GetHandle();
	SetHandle(handle);
	SKYFALL_INFO(ctx, "service %s start: %x", m_name.c_str(), handle);
	ctx->EndlessSignalEnable(1);
	ctx->SetCallback([this](auto&&... args) {
		return Callback(std::forward<decltype(args)>(args)...);
	});
	ctx->Send(handle, skyfall::PTYPE_RESPONSE, 0, nullptr, 0);
	return 0;
}

int SignalService::Callback(skyfall::ContextSPtr ctx, void *, int type, int session, uint32_t source, const void *, size_t) {
	assert(type == skyfall::PTYPE_RESPONSE && session == 0 && source == m_handle);
	while (m_endless) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	SKYFALL_INFO(ctx, "%s exit self", m_name.c_str());
	ctx->SetCallback(nullptr);
	skyfall::ContextExit(ctx->GetHandle());
	return 0;
}

extern "C" SignalService *
testsignal_create(std::string_view param) {
	return new SignalService(param);
}

extern "C" void
testsignal_release(SignalService *app) {
	delete app;
}

extern "C" int
testsignal_init(SignalService *app, skyfall::ContextSPtr ctx, std::string_view) {
	return app->Init(ctx);
}

extern "C" void
testsignal_signal(SignalService *app, int signo) {
	if (signo == 0) {
		app->BreakEndless();
	}
}