#pragma once
#include "kernel/Server.h"
#include "kernel/Skyfall.h"
#include "kernel/Log.h"
#include <cstring>
#include <string_view>
#include <chrono>

class SignalService final {
public:
	explicit SignalService(std::string_view name):
		m_handle(0),
		m_endless(1),
		m_name(name) {
		SKYFALL_INFO(nullptr, "%s ctor: %x", m_name.c_str(), m_handle);
	}
	~SignalService() {
		SKYFALL_INFO(nullptr, "%s dtor: %x", m_name.c_str(), m_handle);
	}
	int Init(skyfall::ContextSPtr ctx);
	void SetHandle(uint32_t h) {
		m_handle = h;
	}
	void BreakEndless() {
		m_endless = 0;
		SKYFALL_INFO(nullptr, "%s break endless: %x", m_name.c_str(), m_handle);
	}
	int Callback(skyfall::ContextSPtr ctx, void *, int type, int session, uint32_t source, const void *, size_t);

private:
	uint32_t m_handle;
	uint32_t m_endless;
	std::string m_name;
};