#include "GameManager.h"


GameManager::GameManager()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	server_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	S_Bind_Listen();

	Init_NPC();

	S_Accept();

	Make_threads();
}
GameManager::~GameManager()
{
	closesocket(server_socket);
	WSACleanup();
}

void GameManager::S_Bind_Listen()
{
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(server_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(server_socket, SOMAXCONN);
}
void GameManager::S_Accept()
{
	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(server_socket), h_iocp, 9999, 0);
	client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	accept_over._comp_type = OP_ACCEPT;
	AcceptEx(server_socket, client_socket, accept_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &accept_over._over);
}

void GameManager::Make_threads()
{
	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back( &GameManager::Worker_thread, this );
	thread timer_thread{ &GameManager::Do_timer, this };
	timer_thread.join();
	for (auto& th : worker_threads)
		th.join();	
}

bool Is_player(int object_id)
{
	return object_id < MAX_USER;
}
bool Is_npc(int object_id)
{
	return !Is_player(object_id);
}

void GameManager::Worker_thread()
{
	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_PLUS* ex_over = reinterpret_cast<OVER_PLUS*>(over);
		if (FALSE == ret) {
			if (ex_over->_comp_type == OP_ACCEPT) cout << "Accept Error";
			else {
				cout << "GQCS Error on client[" << key << "]\n";
				Disconnect(static_cast<int>(key));
				if (ex_over->_comp_type == OP_SEND) delete ex_over;
				continue;
			}
		}

		if ((0 == num_bytes) && ((ex_over->_comp_type == OP_RECV) || (ex_over->_comp_type == OP_SEND))) {
			Disconnect(static_cast<int>(key));
			if (ex_over->_comp_type == OP_SEND) delete ex_over;
			continue;
		}

		switch (ex_over->_comp_type) {
		case OP_ACCEPT: {
			int client_id = Get_new_Client_id();
			if (client_id != -1) {
				{
					lock_guard<mutex> ll(clients[client_id]._s_lock);
					clients[client_id]._state = ST_ALLOC;
				}
				clients[client_id].x = 0;
				clients[client_id].y = 0;
				clients[client_id]._id = client_id;
				clients[client_id]._name[0] = 0;
				clients[client_id]._prev_remain = 0;
				clients[client_id].hp = 4;
				clients[client_id].max_hp = 4;
				clients[client_id]._socket = client_socket;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), h_iocp, client_id, 0);
				clients[client_id].do_recv();
				client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else {
				cout << "Max user exceeded.\n";
			}
			ZeroMemory(&accept_over._over, sizeof(accept_over._over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(server_socket, client_socket, accept_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &accept_over._over);			
			break;
		}
		case OP_RECV: {
			int remain_data = num_bytes + clients[key]._prev_remain;
			char* p = ex_over->_send_buf;
			while (remain_data > 0) {
				WORD* byte = reinterpret_cast<WORD*>(p);
				int packet_size = *byte;
				if (packet_size <= remain_data) {
					Process_packet(static_cast<int>(key), p);
					p = p + packet_size;
					remain_data = remain_data - packet_size;
				}
				else break;
			}
			clients[key]._prev_remain = remain_data;
			if (remain_data > 0) {
				memcpy(ex_over->_send_buf, p, remain_data);
			}
			clients[key].do_recv();
			break;
		}
		case OP_SEND: {
			delete ex_over;
			break;
		}
		case OP_NPC_MOVE: {
			bool keep_alive = false;
			int s_y = clients[key].y / S_HEIGHT;
			int s_x = clients[key].x / S_WIDTH;
			for (int y = s_y - 1; y < s_y + 2; ++y) {
				for (int x = s_x - 1; x < s_x + 2; ++x) {
					if (y < 0 || y >= (W_HEIGHT / S_HEIGHT) || x < 0 || x >= (W_WIDTH / S_WIDTH)) continue;
					st_mng._st_lock[y][x].lock();
					for (auto& j : st_mng.sector_list[y][x]) {
						if (j->_state != ST_INGAME) continue;
						if (!Is_npc(j->_id)) continue;
						if (Can_see(static_cast<int>(key), j->_id)) {
							keep_alive = true;
							ex_over->_ai_target_obj = j->_id;
							break;
						}
					}
					st_mng._st_lock[y][x].unlock();
				}
			}
			if (true == keep_alive) {
				Do_npc_random_move(static_cast<int>(key));
				TIMER_EVENT ev{ key, chrono::system_clock::now() + 1s, EV_RANDOM_MOVE, 0 };
				timer_queue.push(ev);
			}
			else {
				clients[key]._is_active = false;
			}
			delete ex_over;
			break;
		}
			break;
		}
	}
}

void GameManager::Do_timer()
{
	while (true) {
		TIMER_EVENT ev;
		auto current_time = chrono::system_clock::now();

		if (true == timer_queue.try_pop(ev)) {
			if (ev.wakeup_time > current_time) {
				timer_queue.push(ev);		// 최적화 필요
				// timer_queue에 다시 넣지 않고 처리해야 한다.
				this_thread::sleep_for(1ms);  // 실행시간이 아직 안되었으므로 잠시 대기
				continue;
			}
			switch (ev.event_id) {
			case EV_RANDOM_MOVE:
				OVER_PLUS* ov = new OVER_PLUS;
				ov->_comp_type = OP_NPC_MOVE;
				PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->_over);
				break;
			}
			continue;		// 즉시 다음 작업 꺼내기
		}
		this_thread::sleep_for(1ms);   // timer_queue가 비어 있으니 잠시 기다렸다가 다시 시작
	}
}

void GameManager::WakeUpNPC(int npc_id, int waker)
{
	if (clients[npc_id]._is_active || Is_player(npc_id)) return;
	bool old_state = false;
	if (false == atomic_compare_exchange_strong(&clients[npc_id]._is_active, &old_state, true))
		return;
	TIMER_EVENT ev{ npc_id, chrono::system_clock::now(), EV_RANDOM_MOVE, 0 };
	timer_queue.push(ev);
}

void GameManager::Npc_Attacks(SESSION* npc, SESSION* player)
{
	player->_vl.lock();
	unordered_set<int> vlist = player->_view_list;
	player->_vl.unlock();

	player->hp_change(-1);
	for (auto& a : vlist) {
		if (Is_npc(a)) continue;
		if (player->hp <= 0) 
			clients[a].send_death_player_packet(player, npc);
		else 
			clients[a].send_hp_update_packet(player, npc);
	}

	if (player->hp <= 0) {
		player->send_death_player_packet(player, npc);
	}
	else
		player->send_hp_update_packet(player, npc);
}


void GameManager::Init_NPC()
{
	cout << "NPC intialize begin.\n";
	for (int i = MAX_USER; i < MAX_USER + MAX_NPC; ++i) {
		while (1) {
			clients[i].x = rand() % W_WIDTH;
			clients[i].y = rand() % W_HEIGHT;

			if (w_map_mng.map[clients[i].y][clients[i].x] == MI_CRACK_WALL || w_map_mng.map[clients[i].y][clients[i].x] == MI_SOILD_WALL) continue;
			
			break;
		}
		clients[i]._id = i;
		clients[i].max_hp = 2;
		clients[i].hp = 2;
		sprintf_s(clients[i]._name, "NPC%d", i);
		clients[i]._state = ST_INGAME;

		st_mng.SLInsert(&clients[i]);
	}
	cout << "NPC initialize end.\n";
}
void GameManager::Do_npc_random_move(int npc_id)
{
	SESSION* npc = &clients[npc_id];
	unordered_set<int> old_vl;
	int s_y = clients[npc_id].y / S_HEIGHT;
	int s_x = clients[npc_id].x / S_WIDTH;

	for (int y = s_y - 1; y < s_y + 2; ++y) {
		for (int x = s_x - 1; x < s_x + 2; ++x) {
			if (y < 0 || y >= (W_HEIGHT / S_HEIGHT) || x < 0 || x >= (W_WIDTH / S_WIDTH)) continue;
			st_mng._st_lock[y][x].lock();
			for (auto& j : st_mng.sector_list[y][x]) {
				if (ST_INGAME != j->_state) continue;
				if (true == Is_npc(j->_id)) continue;
				if (true == Can_see(npc->_id, j->_id))
					old_vl.insert(j->_id);
			}
			st_mng._st_lock[y][x].unlock();
		}
	}

	int x = npc->x;
	int y = npc->y;
	npc->dir = rand() % 4;
	switch (npc->dir) {
	case 0: if (x < (W_WIDTH - 1)) x++; break;
	case 1: if (x > 0) x--; break;
	case 2: if (y > 0) y--; break;
	case 3: if (y < (W_HEIGHT - 1)) y++; break;
	}
	if (w_map_mng.map[y][x] == MI_CRACK_WALL || w_map_mng.map[y][x] == MI_SOILD_WALL) return;
	else if (w_map_mng.map[y][x] == MI_ITEM) {
		npc->hp_change(1);
	}

	s_y = y / S_HEIGHT;
	s_x = x / S_WIDTH;

	if (s_x != npc->x / S_WIDTH || s_y != npc->y / S_HEIGHT) {
		st_mng.SLErase(npc);

		npc->x = x;
		npc->y = y;
		st_mng.SLInsert(npc);
	}

	npc->x = x;
	npc->y = y;

	unordered_set<int> new_vl;
	for (int y = s_y - 1; y < s_y + 2; ++y) {
		for (int x = s_x - 1; x < s_x + 2; ++x) {
			if (y < 0 || y >= (W_HEIGHT / S_HEIGHT) || x < 0 || x >= (W_WIDTH / S_WIDTH)) continue;
			st_mng._st_lock[y][x].lock();
			for (auto& j : st_mng.sector_list[y][x]) {
				if (ST_INGAME != j->_state) continue;
				if (true == Is_npc(j->_id)) continue;
				if (true == Can_see(npc->_id, j->_id))
					new_vl.insert(j->_id);

				if (j->x == npc->x && j->y == npc->y)
					Npc_Attacks(npc, j);
			}
			st_mng._st_lock[y][x].unlock();
		}
	}

	for (auto pl : new_vl) {
		if (0 == old_vl.count(pl)) {
			// 플레이어의 시야에 등장
			clients[pl].send_add_player_packet(npc);
		}
		else {
			// 플레이어가 계속 보고 있음.
			clients[pl].send_move_packet(npc);
			if (w_map_mng.map[y][x] == MI_ITEM) {
				clients[pl].send_hp_update_packet(npc, -1);
				clients[pl].send_change_map_packet(x, y, MI_FREE);
			}
		}
	}
	///vvcxxccxvvdsvdvds
	for (auto pl : old_vl) {
		if (0 == new_vl.count(pl)) {
			clients[pl]._vl.lock();
			if (0 != clients[pl]._view_list.count(npc->_id)) {
				clients[pl]._vl.unlock();
				clients[pl].send_remove_player_packet(npc->_id);
			}
			else {
				clients[pl]._vl.unlock();
			}
		}
	}
}

void GameManager::Disconnect(int c_id)
{
	clients[c_id]._vl.lock();
	unordered_set <int> vl = clients[c_id]._view_list;
	clients[c_id]._vl.unlock();
	for (auto& p_id : vl) {
		if (Is_npc(p_id)) continue;
		auto& pl = clients[p_id];
		{
			lock_guard<mutex> ll(pl._s_lock);
			if (ST_INGAME != pl._state) continue;
		}
		if (pl._id == c_id) continue;
		pl.send_remove_player_packet(c_id);
	}
	closesocket(clients[c_id]._socket);

	st_mng.SLErase(&clients[c_id]);

	lock_guard<mutex> ll(clients[c_id]._s_lock);
	clients[c_id]._state = ST_FREE;
}

int GameManager::Get_new_Client_id()
{
	for (int i = 0; i < MAX_USER; ++i) {
		lock_guard <mutex> ll{ clients[i]._s_lock };
		if (clients[i]._state == ST_FREE)
			return i;
	}
	return -1;
}

void GameManager::Process_packet(int c_id, char* packet)
{
	switch (packet[2]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		strcpy_s(clients[c_id]._name, p->name);
		{
			lock_guard<mutex> ll{ clients[c_id]._s_lock };
			clients[c_id].x = rand() % W_WIDTH;
			clients[c_id].y = rand() % W_HEIGHT;
			clients[c_id]._state = ST_INGAME;
		}

		clients[c_id].hp = 4;
		clients[c_id].max_hp = 4;

		int s_x = clients[c_id].x / S_WIDTH;
		int s_y = clients[c_id].y / S_HEIGHT;

		st_mng.SLInsert(&clients[c_id]);


		clients[c_id].send_login_info_packet();

		// 주변 적이나 플레이어 정보 등록
		for (int y = s_y - 1; y < s_y + 2; ++y) {
			for (int x = s_x - 1; x < s_x + 2; ++x) {
				if (y < 0 || y >= (W_HEIGHT / S_HEIGHT) || x < 0 || x >= (W_WIDTH / S_WIDTH)) continue;
				st_mng._st_lock[y][x].lock();
				for (auto& cl : st_mng.sector_list[y][x]) {
					{
						lock_guard<mutex> ll(cl->_s_lock);
						if (ST_INGAME != cl->_state) continue;
					}
					if (cl->_id == c_id) continue;
					if (false == Can_see(c_id, cl->_id)) continue;
					if (Is_player(cl->_id)) cl->send_add_player_packet(&clients[c_id]);
					else WakeUpNPC(cl->_id, c_id);
					clients[c_id].send_add_player_packet(cl);
				}
				st_mng._st_lock[y][x].unlock();
			}
		}

		// 맵 정보 등록 (주변 섹터만)
		for (int l_y = s_y - 1; l_y < s_y + 2; ++l_y) {
			for (int l_x = s_x - 1; l_x < s_x + 2; ++l_x) {
				if (l_y < 0 || l_y >= (W_HEIGHT / S_HEIGHT) || l_x < 0 || l_x >= (W_WIDTH / S_WIDTH)) continue;
				w_map_mng.m_lock[l_y][l_x].lock();
				for (int y = l_y * S_HEIGHT; y < (l_y + 1) * S_HEIGHT; ++y) {
					for (int x = l_x * S_WIDTH; x < (l_x + 1) * S_WIDTH; ++x) {
						if (w_map_mng.map[y][x] == static_cast<char>(MI_FREE)) continue;
						clients[c_id].send_change_map_packet(x, y, w_map_mng.map[y][x]);
					}
				}
				w_map_mng.m_lock[l_y][l_x].unlock();
			}
		}

		break;
	}
	case CS_MOVE: {
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
		clients[c_id].last_move_time = p->move_time;
		short x = clients[c_id].x;
		short y = clients[c_id].y;
		
		//direction |  // 0 : RIGHT, 1 : LEFT, 2 : UP, 3 : DOWN
		
		switch (p->direction) {
		case 0: if (x < W_WIDTH - 1) x++; break;
		case 1: if (x > 0) x--; break;
		case 2: if (y > 0) y--; break;
		case 3: if (y < W_HEIGHT - 1) y++; break;
		}
		if (w_map_mng.map[y][x] == MI_CRACK_WALL || w_map_mng.map[y][x] == MI_SOILD_WALL) {
			x = clients[c_id].x;
			y = clients[c_id].y;
		}
		else if (w_map_mng.map[y][x] == MI_ITEM) {
			clients[c_id].hp_change(1);
		}

		if (p->direction > 3)
			p->direction -= 4;
		clients[c_id].dir = p->direction;
		int s_y = y / S_HEIGHT;
		int s_x = x / S_WIDTH;

		if (s_x != clients[c_id].x / S_WIDTH || s_y != clients[c_id].y / S_HEIGHT) {
			st_mng.SLErase(&clients[c_id]);

			clients[c_id].x = x;
			clients[c_id].y = y;
			st_mng.SLInsert(&clients[c_id]);

			// 맵 정보 등록 (주변 섹터만)
			for (int l_y = s_y - 1; l_y < s_y + 2; ++l_y) {
				for (int l_x = s_x - 1; l_x < s_x + 2; ++l_x) {
					if (l_y < 0 || l_y >= (W_HEIGHT / S_HEIGHT) || l_x < 0 || l_x >= (W_WIDTH / S_WIDTH)) continue;
					w_map_mng.m_lock[l_y][l_x].lock();
					for (int y = l_y * S_HEIGHT; y < (l_y + 1) * S_HEIGHT; ++y) {
						for (int x = l_x * S_WIDTH; x < (l_x + 1) * S_WIDTH; ++x) {
							if (w_map_mng.map[y][x] == static_cast<char>(MI_FREE)) continue;
							clients[c_id].send_change_map_packet(x, y, w_map_mng.map[y][x]);
						}
					}
					w_map_mng.m_lock[l_y][l_x].unlock();
				}
			}
		}

		unordered_set<int> near_list;
		clients[c_id]._vl.lock();
		unordered_set<int> old_vlist = clients[c_id]._view_list;
		clients[c_id]._vl.unlock();

		for (int y = s_y - 1; y < s_y + 2; ++y) {
			for (int x = s_x - 1; x < s_x + 2; ++x) {
				if (y < 0 || y >= (W_HEIGHT / S_HEIGHT) || x < 0 || x >= (W_WIDTH / S_WIDTH)) continue;
				st_mng._st_lock[y][x].lock();
				for (auto& cl : st_mng.sector_list[y][x]) {
					if (cl->_state != ST_INGAME) continue;
					if (cl->_id == c_id) continue;
					if (Can_see(c_id, cl->_id))
						near_list.insert(cl->_id);
				}
				st_mng._st_lock[y][x].unlock();
			}
		}

		clients[c_id].x = x;
		clients[c_id].y = y;
		clients[c_id].send_move_packet(&clients[c_id]);
		if (w_map_mng.map[y][x] == MI_ITEM) {
			clients[c_id].send_hp_update_packet(&clients[c_id], -1);
			clients[c_id].send_change_map_packet(x, y, MI_FREE);
		}


		for (auto& pl : near_list) {
			if (Is_player(pl)) {
				clients[pl]._vl.lock();
				if (clients[pl]._view_list.count(c_id)) {
					clients[pl]._vl.unlock();
					clients[pl].send_move_packet(&clients[c_id]);
					
					if (w_map_mng.map[y][x] == MI_ITEM) {
						clients[pl].send_hp_update_packet(&clients[c_id], -1);
						clients[pl].send_change_map_packet(x, y, MI_FREE);
					}
				}
				else {
					clients[pl]._vl.unlock();
					clients[pl].send_add_player_packet(&clients[c_id]);
					if (w_map_mng.map[y][x] == MI_ITEM) {
						// 여기서는 hp 정보가 add에 들어있어서 hp_update 안해줘도 됨
						clients[pl].send_change_map_packet(x, y, MI_FREE);
					}
				}
			}
			else {
				WakeUpNPC(pl, c_id);

				if (clients[c_id].x == clients[pl].x && clients[c_id].y == clients[pl].y)
					Npc_Attacks(&clients[pl], &clients[c_id]);
			}

			if (old_vlist.count(pl) == 0) {
				clients[c_id].send_add_player_packet(&clients[pl]);
				if (w_map_mng.map[y][x] == MI_ITEM) {
					// 여기서는 hp 정보가 add에 들어있어서 hp_update 안해줘도 됨
					clients[pl].send_change_map_packet(x, y, MI_FREE);
				}
			}
		}

		for (auto& pl : old_vlist) {
			if (0 == near_list.count(pl)) {
				clients[c_id].send_remove_player_packet(pl);
				if (Is_player(pl))
					clients[pl].send_remove_player_packet(c_id);
			}
		}

		if (w_map_mng.map[y][x] == MI_ITEM) {
			w_map_mng.Change_Map(x, y, MI_FREE);
		}
		break;
	}
	case CS_ATTACK: {
		CS_ATTACK_PACKET* p = reinterpret_cast<CS_ATTACK_PACKET*>(packet);

		SESSION attack;
		attack.x = clients[c_id].x;
		attack.y = clients[c_id].y;
		attack._id = c_id;

		//direction |  // 0 : RIGHT, 1 : LEFT, 2 : UP, 3 : DOWN
		switch (p->direction) {
		case 0: if (attack.x < W_WIDTH - 1) attack.x++; break;
		case 1: if (attack.x > 0) attack.x--; break;
		case 2: if (attack.y > 0) attack.y--; break;
		case 3: if (attack.y < W_HEIGHT - 1) attack.y++; break;
		}

		int s_x = attack.x / S_HEIGHT;
		int s_y = attack.y / S_WIDTH;

		// 부술 수 있는 벽 공격하면 뿌수기
		bool blocken_wall = false;
		if (w_map_mng.map[attack.y][attack.x] == static_cast<char>(MI_CRACK_WALL)) {
			w_map_mng.Change_Map(attack.x, attack.y, MI_ITEM);
			blocken_wall = true;
			clients[c_id].send_change_map_packet(attack.x, attack.y, w_map_mng.map[attack.y][attack.x]);
		}

		// 섹터 말고 뷰리스트 도는거 함 고민해보자
		for (int y = s_y - 1; y < s_y + 2; ++y) {
			for (int x = s_x - 1; x < s_x + 2; ++x) {
				if (y < 0 || y >= (W_HEIGHT / S_HEIGHT) + 1 || x < 0 || x >= (W_WIDTH / S_WIDTH) + 1) continue;
				st_mng._st_lock[y][x].lock();
				for (auto& cl : st_mng.sector_list[y][x]) {
					if (cl->_state != ST_INGAME) continue;
					if (Can_see(c_id, cl->_id))
						cl->send_attack_player_packet(&attack);
					if (cl->_id == c_id) continue;
					if (blocken_wall) {
						cl->send_change_map_packet(attack.x, attack.y, w_map_mng.map[attack.y][attack.x]);
					}
					else if (cl->x == attack.x && cl->y == attack.y) { // 공격에 맞은 놈 cl
						cl->hp_change(-1);

						clients[c_id]._vl.lock();
						for (auto& a : clients[c_id]._view_list) {
							if (cl->hp <= 0) {
								if (Is_player(clients[a]._id))
									clients[a].send_death_player_packet(cl, &clients[c_id]);
							}
							else {
								clients[a].send_hp_update_packet(cl, &clients[c_id]);
								if (Is_player(cl->_id))
									cl->send_hp_update_packet(cl, &clients[c_id]);
							}
						}
						clients[c_id]._vl.unlock();							
						
						if (cl->hp <= 0) {
							clients[c_id].send_death_player_packet(cl, &clients[c_id]);
						}
						else 
							clients[c_id].send_hp_update_packet(cl, &clients[c_id]);
					}
				}
				st_mng._st_lock[y][x].unlock();
			}
		}

		break;
	}
	case CS_CHAT: {
		CS_CHAT_PACKET* p = reinterpret_cast<CS_CHAT_PACKET*>(packet);

		int s_x = clients[c_id].x / S_HEIGHT;
		int s_y = clients[c_id].y / S_WIDTH;

		for (int y = s_y - 1; y < s_y + 2; ++y) {
			for (int x = s_x - 1; x < s_x + 2; ++x) {
				if (y < 0 || y >= (W_HEIGHT / S_HEIGHT) + 1 || x < 0 || x >= (W_WIDTH / S_WIDTH) + 1) continue;
				st_mng._st_lock[y][x].lock();
				for (auto& cl : st_mng.sector_list[y][x]) {
					if (cl->_state != ST_INGAME) continue;
					cl->send_chat_packet(c_id, p->mess);
				}
				st_mng._st_lock[y][x].unlock();
			}
		}
		break;
	}
	case CS_RESPAWN: {
		CS_RESPAWN_PACKET* p = reinterpret_cast<CS_RESPAWN_PACKET*>(packet);

		clients[c_id].hp_change(clients[c_id].max_hp);

		int s_x = p->x / S_WIDTH;
		int s_y = p->y / S_HEIGHT;

		if (s_x != clients[c_id].x / S_WIDTH || s_y != clients[c_id].y / S_HEIGHT) {
			st_mng.SLErase(&clients[c_id]);

			clients[c_id].x = p->x;
			clients[c_id].y = p->y;
			st_mng.SLInsert(&clients[c_id]);

			// 맵 정보 등록 (주변 섹터만)
			for (int l_y = s_y - 1; l_y < s_y + 2; ++l_y) {
				for (int l_x = s_x - 1; l_x < s_x + 2; ++l_x) {
					if (l_y < 0 || l_y >= (W_HEIGHT / S_HEIGHT) || l_x < 0 || l_x >= (W_WIDTH / S_WIDTH)) continue;
					w_map_mng.m_lock[l_y][l_x].lock();
					for (int y = l_y * S_HEIGHT; y < (l_y + 1) * S_HEIGHT; ++y) {
						for (int x = l_x * S_WIDTH; x < (l_x + 1) * S_WIDTH; ++x) {
							if (w_map_mng.map[y][x] == static_cast<char>(MI_FREE)) continue;
							clients[c_id].send_change_map_packet(x, y, w_map_mng.map[y][x]);
						}
					}
					w_map_mng.m_lock[l_y][l_x].unlock();
				}
			}
		}

		clients[c_id].x = p->x;
		clients[c_id].y = p->y;

		clients[c_id].send_respawn_packet(c_id);
		// 주변 적이나 플레이어 정보 등록
		for (int y = s_y - 1; y < s_y + 2; ++y) {
			for (int x = s_x - 1; x < s_x + 2; ++x) {
				if (y < 0 || y >= (W_HEIGHT / S_HEIGHT) || x < 0 || x >= (W_WIDTH / S_WIDTH)) continue;
				st_mng._st_lock[y][x].lock();
				for (auto& cl : st_mng.sector_list[y][x]) {
					{
						lock_guard<mutex> ll(cl->_s_lock);
						if (ST_INGAME != cl->_state) continue;
					}
					if (cl->_id == c_id) continue;
					if (false == Can_see(c_id, cl->_id)) continue;
					if (Is_player(cl->_id)) {
						cl->send_add_player_packet(&clients[c_id]);
						cl->send_respawn_packet(c_id);
					}
					else WakeUpNPC(cl->_id, c_id);
					clients[c_id].send_add_player_packet(cl);
				}
				st_mng._st_lock[y][x].unlock();
			}
		}
		break;
	}
	}
}

bool GameManager::Can_see(int from, int to)
{
	if (abs(clients[from].x - clients[to].x) > VIEW_RANGE) return false;
	return abs(clients[from].y - clients[to].y) <= VIEW_RANGE;
}

void GameManager::DataBase() {
	// Allocate environment handle  
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

	cout << "Set the ODBC version environment attribute  " << endl;
	// Set the ODBC version environment attribute  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);

		// Allocate connection handle  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

			// Set login timeout to 5 seconds  
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

				// Connect to data source  
				retcode = SQLConnect(hdbc, (SQLWCHAR*)L"Server_TermProject_Yum", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);

				// Allocate statement handle  
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
					retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);

					// Process data  
					if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
						SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
					}

					SQLDisconnect(hdbc);
				}

				SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
			}
		}
		SQLFreeHandle(SQL_HANDLE_ENV, henv);
	}
}