#pragma once
#include "stdafx.h"
#include "TIMER_EVENT.h"
#include "SESSION.h"
	//#include "OVER_PLUS.h"
#include "SectorManager.h"
#include "MapManager.h"

class GameManager
{
public:
	SOCKET server_socket, client_socket;
	HANDLE h_iocp;
	OVER_PLUS accept_over;
	concurrency::concurrent_priority_queue<TIMER_EVENT> timer_queue;
	array<SESSION, MAX_USER + MAX_NPC> clients;
	SectorManager st_mng;
	MapManager w_map_mng{};

	GameManager();
	~GameManager();
	void S_Bind_Listen();
	void S_Accept();
	void Make_threads();
	void Worker_thread();
	void Do_timer();
	void Disconnect(int cl_id);
	void Process_packet(int c_id, char* packet);
	int Get_new_Client_id();
	bool Can_see(int from, int to);
	void Init_NPC();
	void Do_npc_random_move(int npc_id);
	void WakeUpNPC(int npc_id, int waker);
};
