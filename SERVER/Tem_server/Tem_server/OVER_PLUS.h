#pragma once
#include <WS2tcpip.h>
#include <MSWSock.h>
#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
#include "ENUM.h"
#include "../../../CLIENT/protocol.h"

class OVER_PLUS
{
public:
	WSAOVERLAPPED _over;
	WSABUF _wsabuf;
	char _send_buf[CHAT_SIZE];
	COMP_TYPE _comp_type;
	int _ai_target_obj;

	OVER_PLUS() {
		_wsabuf.len = CHAT_SIZE;
		_wsabuf.buf = _send_buf;
		_comp_type = OP_RECV;
		ZeroMemory(&_over, sizeof(_over));
	}

	OVER_PLUS(char* packet) {
		_wsabuf.len = *(unsigned short*)packet;
		_wsabuf.buf = _send_buf;
		ZeroMemory(&_over, sizeof(_over));
		_comp_type = OP_SEND;
		memcpy(_send_buf, packet, _wsabuf.len);
	}
};

