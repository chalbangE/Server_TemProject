#pragma once
#include "stdafx.h"
#include "TIMER_EVENT.h"
#include "SESSION.h"
	//#include "OVER_PLUS.h"

class GameManager
{
public:
	SOCKET server_socket, client_socket;
	HANDLE h_iocp;
	OVER_PLUS accept_over;
	concurrency::concurrent_priority_queue<TIMER_EVENT> timer_queue;
	array<SESSION, MAX_USER + MAX_NPC> clients;

	GameManager();
	~GameManager();
	void S_Bind_Listen();
	void S_Accept();
	void Init_NPC();
	void Make_threads();
	void Worker_thread();
	void Do_timer();
	void Disconnect(int cl_id);
	void Process_packet(int c_id, char* packet);
	int Get_new_Client_id();
};