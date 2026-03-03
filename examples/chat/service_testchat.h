#pragma once
#include <cassert>
#include <string>
#include <cstring>
#include <string_view>
#include <set>
#include <unordered_map>
#include <functional>
#include <time.h>
#include <unistd.h>
#include <cstdio>
#include <iterator>
#include "kernel/Server.h"
#include "kernel/Skyfall.h"
#include "kernel/Log.h"


class ChatService {
public:
	ChatService(std::string_view name):
		m_seq(0),
		m_handle(0),
		m_name(name) {}
	virtual ~ChatService() {
		SKYFALL_INFO(nullptr, "%s dtor: %x", m_name.c_str(), m_handle);
	}
	virtual int Init(skyfall::ContextSPtr ctx) = 0;
	void SetHandle(uint32_t h) {
		m_handle = h;
	}
	uint32_t GetHandle() {
		return m_handle;
	}
	void SetPeer(std::string_view name) {
		m_peer = name;
	}
	int Callback(skyfall::ContextSPtr ctx, void *ud, int type, int session, uint32_t source, const void *msg, size_t sz);

protected:
	int Timeout(skyfall::ContextSPtr ctx, int64_t interval_ms, std::function<void ()> cb);
	virtual void OnRequest(skyfall::ContextSPtr ctx, int session, uint32_t source, const void *, size_t);
	virtual void OnResponse(skyfall::ContextSPtr, int session, uint32_t, const void *, size_t);

protected:
	int m_seq;
	uint32_t m_handle;
	std::string m_name;
	std::string m_peer;

private:
	std::unordered_map<int, std::function<void ()>> m_timeouts;
};

class Guider final: public ChatService {
public:
	Guider(): ChatService("Guider") {}
	int Init(skyfall::ContextSPtr ctx) override;
};

class Sender final: public ChatService {
public:
	explicit Sender(std::string_view name):
		ChatService(name), m_cursor(0) {}
	int Init(skyfall::ContextSPtr ctx) override;

protected:
	void OnResponse(skyfall::ContextSPtr ctx, int session, uint32_t source, const void *msg, size_t sz) override;

private:
	std::set<int> m_requests;
	size_t m_cursor;
};

class Receiver final: public ChatService {
public:
	explicit Receiver(std::string_view name): ChatService(name) {}
	int Init(skyfall::ContextSPtr ctx) override;

protected:
	void OnRequest(skyfall::ContextSPtr ctx, int session, uint32_t source, const void *msg, size_t sz) override;
};