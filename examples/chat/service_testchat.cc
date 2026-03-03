#include "service_testchat.h"
#include "util/StringUtil.h"

static const char *const GMessageTable[] = {
	"Hello world",
	"Nice to meet you",
	"See you tomorrow",
	"Have a good night",
	"Goodbye"
};

static const int kDelayChatMs = 1000;

int ChatService::Callback(skyfall::ContextSPtr ctx, void *, int type, int session, uint32_t source, const void *msg, size_t sz) {
	switch(type) {
	case skyfall::PTYPE_TEXT:
		OnRequest(ctx, session, source, msg, sz);
		break;
	case skyfall::PTYPE_RESPONSE:
		OnResponse(ctx, session, source, msg, sz);
		break;
	default:
		SKYFALL_WARN(ctx, "%s recv unknown msg type:%d session:%d from %x", m_name.c_str(), type, session, source);
		break;
	}
	return 0;
}

int ChatService::Timeout(skyfall::ContextSPtr ctx, int64_t interval_ms, std::function<void ()> cb) {
	m_timeouts[++m_seq]	= std::move(cb);
	SKYFALL_INFO(ctx, "%s new timer session: %d delay time:%lld", m_name.c_str(), m_seq, interval_ms);
	return skyfall::Timeout(ctx->GetHandle(), interval_ms, m_seq);
}

void ChatService::OnRequest(skyfall::ContextSPtr ctx, int session, uint32_t source, const void *, size_t) {
	SKYFALL_ERROR(ctx, "%s recv unknown msg session:%d from %x", m_name.c_str(), session, source);
}

void ChatService::OnResponse(skyfall::ContextSPtr, int session, uint32_t, const void *, size_t) {
	if (auto iter = m_timeouts.find(session); iter != m_timeouts.end()) {
		auto cb = std::move(iter->second);
		m_timeouts.erase(iter);
		cb();
	}
}


int Guider::Init(skyfall::ContextSPtr ctx) {
	auto handle = ctx->GetHandle();
	SetHandle(handle);
	SKYFALL_INFO(ctx, "bootstrap service start: %x", handle);
	assert(skyfall::ContextNew("testchat", "Receiver Alice Bob") != nullptr);
	assert(skyfall::ContextNew("testchat", "Sender Bob Alice") != nullptr);
	SKYFALL_INFO(ctx, "bootstrap init done");
	skyfall::ContextWPtr weak_ctx = ctx;
	ctx->SetCallback([this, weak_ctx](auto&&... args) {
		int ret = Callback(std::forward<decltype(args)>(args)...);
		if (auto shared_ctx = weak_ctx.lock()) {
			shared_ctx->SetCallback(nullptr);
		}
		return ret;
	});
	Timeout(ctx, 0, [handle] {
		skyfall::ContextExit(handle);
		SKYFALL_INFO(nullptr, "bootstrap service exit!");
	});
	return 0;
}


int Sender::Init(skyfall::ContextSPtr ctx) {
	auto handle = ctx->GetHandle();
	SetHandle(handle);
	skyfall::SetHandleName(handle, m_name);
	SKYFALL_INFO(ctx, "sender service %s start: %x", m_name.c_str(), handle);
	ctx->SetCallback([this](auto&&... args) {
		return Callback(std::forward<decltype(args)>(args)...);
	});
	skyfall::ContextWPtr weak_ctx = ctx;
	Timeout(ctx, kDelayChatMs, [this, weak_ctx] {
		auto shared_ctx = weak_ctx.lock();
		if (shared_ctx == nullptr) {
			return;
		}
		m_requests.insert(++m_seq);
		const char *msg = GMessageTable[m_cursor++];
		SKYFALL_INFO(shared_ctx, "%s new chat text '%s' session: %d", m_name.c_str(), msg, m_seq);
		shared_ctx->SendName(m_peer, skyfall::PTYPE_TEXT, m_seq, static_cast<void*>(const_cast<char*>(msg)), strlen(msg));
	});
	return 0;
}

void Sender::OnResponse(skyfall::ContextSPtr ctx, int session, uint32_t source, const void *msg, size_t sz) {
	// chat msg response
	if (m_requests.count(session) > 0) {
		m_requests.erase(session);
		SKYFALL_INFO(ctx, "%s recv pong msg '%.*s' session:%d from peer %x", m_name.c_str(), static_cast<int>(sz), static_cast<const char*>(msg), session, source);
		if (m_cursor >= std::size(GMessageTable)) {
			auto peer_handle = skyfall::FindHandle(m_peer);
			SKYFALL_INFO(ctx, "%s exit peer: %s", m_name.c_str(), m_peer.c_str());
			skyfall::ContextExit(peer_handle);
			SKYFALL_INFO(ctx, "%s exit self", m_name.c_str());
			skyfall::ContextExit(ctx->GetHandle());
		} else {
			Timeout(ctx, kDelayChatMs, [this] {
				m_requests.insert(++m_seq);
				const char *chat_msg = GMessageTable[m_cursor++];
				SKYFALL_INFO(nullptr, "%s new chat text '%s' session: %d", m_name.c_str(), chat_msg, m_seq);
				skyfall::Context::SendToName(GetHandle(), m_peer, skyfall::PTYPE_TEXT, m_seq, static_cast<void*>(const_cast<char*>(chat_msg)), strlen(chat_msg));
			});
		}
		return;
	}
	// check timer timeout
	ChatService::OnResponse(ctx, session, source, msg, sz);
}


int Receiver::Init(skyfall::ContextSPtr ctx) {
	auto handle = ctx->GetHandle();
	SetHandle(handle);
	skyfall::SetHandleName(handle, m_name);
	SKYFALL_INFO(ctx, "receiver service %s start: %x", m_name.c_str(), handle);
	ctx->SetCallback([this](auto&&... args) {
		return Callback(std::forward<decltype(args)>(args)...);
	});
	return 0;
}

void Receiver::OnRequest(skyfall::ContextSPtr ctx, int session, uint32_t source, const void *msg, size_t sz) {
	SKYFALL_INFO(ctx, "%s recv ping msg '%.*s' session:%d from peer %x", m_name.c_str(),static_cast<int>(sz), static_cast<const char*>(msg), session, source);
	ctx->SendName(m_peer, skyfall::PTYPE_RESPONSE, session, const_cast<void*>(msg), sz);
}


extern "C" ChatService *
testchat_create(std::string_view param) {
	if (param == "bootstrap" || param == "") {
		return static_cast<ChatService *>(new Guider);
	}
	auto ret = skyfall::StringUtil::Split(param, ' ');
	assert(ret.size() == 3);
	auto type = ret[0];
	auto self_name = ret[1];
	auto peer_name = ret[2];
	if (type == "Sender") {
		auto sender = new Sender(self_name);
		sender->SetPeer(peer_name);
		return static_cast<ChatService *>(sender);
	} else {
		auto receiver = new Receiver(self_name);
		receiver->SetPeer(peer_name);
		return static_cast<ChatService *>(receiver);
	}
}

extern "C" void
testchat_release(ChatService *app) {
	delete app;
}

extern "C" int
testchat_init(ChatService *app, skyfall::ContextSPtr ctx, std::string_view) {
	return app->Init(ctx);
}