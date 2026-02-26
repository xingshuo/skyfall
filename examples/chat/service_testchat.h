#pragma once
#include <string>
#include <set>
#include "kernel/Server.h"
#include "kernel/Skyfall.h"
#include "kernel/Log.h"

class TestChat {
public:
	TestChat() {
		m_seq = 0;
	}
	~TestChat() = default;

	void SetName(std::string& name) {
		m_name = name;
	}
	const std::string& GetName() const {
		return m_name;
	}
	void SetPeer(std::string& peer) {
		m_peer = peer;
	}
	const std::string& GetPeer() const {
		return m_peer;
	}
	void SetContext(skyfall::ContextSPtr ctx) {
		m_ctx = ctx;
	}
	skyfall::ContextSPtr GetContext() {
		return m_ctx.lock();
	}
	int NewSession() {
		return ++m_seq;
	}
	void PrepareChat(skyfall::ContextSPtr ctx);
	void HandleRequest(skyfall::ContextSPtr ctx, int session, uint32_t source, const void *msg, size_t sz);
	void HandleResponse(skyfall::ContextSPtr ctx, int session, uint32_t source, const void *msg, size_t sz);

private:
	skyfall::ContextWPtr m_ctx;
	std::string m_name;
	std::string m_peer;
	std::set<int> m_requests;
	int m_seq;
};