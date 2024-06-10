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
void GameManager::Init_NPC()
{
}

void GameManager::Make_threads()
{
	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back([this](){
			Worker_thread(); 
			});
	//thread timer_thread{ Do_timer };
	//timer_thread.join();
	for (auto& th : worker_threads)
		th.join();
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
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket),
					h_iocp, client_id, 0);
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
				int packet_size = p[0];
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
		case OP_SEND:
			delete ex_over;
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


bool Is_player(int object_id)
{
	return object_id < MAX_USER;
}
bool Is_npc(int object_id)
{
	return !Is_player(object_id);
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

		int s_x = clients[c_id].x / S_WIDTH;
		int s_y = clients[c_id].y / S_HEIGHT;

		st_mng.SLInsert(&clients[c_id]);

		for (int y = s_y - 1; y < s_y + 2; ++y) {
			for (int x = s_x - 1; x < s_x + 2; ++x) {
				if (y < 0 || y >= (W_HEIGHT / S_HEIGHT) + 1 || x < 0 || x >= (W_WIDTH / S_WIDTH) + 1) continue;
				st_mng._st_lock[y][x].lock();
				for (auto& cl : st_mng.sector_list[y][x]) {
					{
						lock_guard<mutex> ll(cl->_s_lock);
						if (ST_INGAME != cl->_state) continue;
					}
					if (cl->_id == c_id) continue;
					if (false == Can_see(c_id, cl->_id))
						continue;
					clients[c_id].send_add_player_packet(cl);
				}
				st_mng._st_lock[y][x].unlock();
			}
		}

		clients[c_id].send_login_info_packet();

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
		int s_y = y / S_HEIGHT;
		int s_x = x / S_WIDTH;

		if (s_x != clients[c_id].x / S_WIDTH || s_y != clients[c_id].y / S_HEIGHT) {
			st_mng.SLErase(&clients[c_id]);

			clients[c_id].x = x;
			clients[c_id].y = y;
			st_mng.SLInsert(&clients[c_id]);
		}

		unordered_set<int> near_list;
		clients[c_id]._vl.lock();
		unordered_set<int> old_vlist = clients[c_id]._view_list;
		clients[c_id]._vl.unlock();

		for (int y = s_y - 1; y < s_y + 2; ++y) {
			for (int x = s_x - 1; x < s_x + 2; ++x) {
				if (y < 0 || y >= (W_HEIGHT / S_HEIGHT) + 1 || x < 0 || x >= (W_WIDTH / S_WIDTH) + 1) continue;
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

		for (auto& pl : near_list) {
			clients[pl]._vl.lock();
			if (clients[pl]._view_list.count(c_id)) {
				clients[pl]._vl.unlock();
				clients[pl].send_move_packet(&clients[c_id]);
			}
			else {
				clients[pl]._vl.unlock();
				clients[pl].send_add_player_packet(&clients[c_id]);
			}

			if (old_vlist.count(pl) == 0)
				clients[c_id].send_add_player_packet(&clients[pl]);
		}

		for (auto& pl : old_vlist) {
			if (0 == near_list.count(pl)) {
				clients[c_id].send_remove_player_packet(pl);
				if (Is_player(pl))
					clients[pl].send_remove_player_packet(c_id);
			}
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

		int s_x = clients[c_id].x / S_HEIGHT;
		int s_y = clients[c_id].y / S_WIDTH;

		for (int y = s_y - 1; y < s_y + 2; ++y) {
			for (int x = s_x - 1; x < s_x + 2; ++x) {
				if (y < 0 || y >= (W_HEIGHT / S_HEIGHT) + 1 || x < 0 || x >= (W_WIDTH / S_WIDTH) + 1) continue;
				st_mng._st_lock[y][x].lock();
				for (auto& cl : st_mng.sector_list[y][x]) {
					if (cl->_state != ST_INGAME) continue;
					if (Can_see(c_id, cl->_id))
						cl->send_attack_player_packet(&attack);
					if (cl->_id == c_id) continue;
					if (cl->x == attack.x && cl->y == attack.y) {
						--cl->hp;

						if (cl->hp <= 0) 
							clients[c_id].send_death_player_packet(cl);
						else clients[c_id].send_hit_player_packet(cl);
					}
				}
				st_mng._st_lock[y][x].unlock();
			}
		}
	}
	}
}

bool GameManager::Can_see(int from, int to)
{
	if (abs(clients[from].x - clients[to].x) > VIEW_RANGE) return false;
	return abs(clients[from].y - clients[to].y) <= VIEW_RANGE;
}
