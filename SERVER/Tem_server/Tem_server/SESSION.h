#pragma once
#include "stdafx.h"
#include "OVER_PLUS.h"

class SESSION
{
	OVER_PLUS _recv_over;

public:
	mutex								_s_lock;
	SESSION_STATE						_state;
	atomic_bool							_is_active;		// 주위에 플레이어가 있는가?
	int									_id;
	SOCKET								_socket;
	short								x, y;
	char								_name[NAME_SIZE];
	int									_prev_remain;
	unordered_set <int>					_view_list;
	mutex								_vl;
	int									last_move_time;
	chrono::system_clock::time_point	move_start_time;
	lua_State*							_L;
	mutex								_ll;

	SESSION();
	~SESSION();

	void do_recv();

	void do_send(void* packet);
	void send_login_info_packet();
	void send_move_packet(SESSION* client);
	void send_add_player_packet(SESSION* client);
	void send_chat_packet(int c_id, const char* mess);
	void send_remove_player_packet(int c_id);
};


