#include "SESSION.h"


SESSION::SESSION()
{
	_id = -1;
	_socket = 0;
	x = y = 0;
	_name[0] = 0;
	_state = ST_FREE;
	_prev_remain = 0;
	hp, max_hp = 0;
}

SESSION::~SESSION() {}

void SESSION::do_recv()
{
	DWORD recv_flag = 0;
	memset(&_recv_over._over, 0, sizeof(_recv_over._over));
	_recv_over._wsabuf.len = CHAT_SIZE - _prev_remain;
	_recv_over._wsabuf.buf = _recv_over._send_buf + _prev_remain;

	WSARecv(_socket, &_recv_over._wsabuf, 1, 0, &recv_flag, &_recv_over._over, 0);
}

void SESSION::do_send(void* packet)
{
	OVER_PLUS* sdata = new OVER_PLUS{ reinterpret_cast<char*>(packet) };
	WSASend(_socket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, 0);
}

void SESSION::send_login_info_packet()
{
	SC_LOGIN_INFO_PACKET p;
	p.id = _id;
	p.size = sizeof(SC_LOGIN_INFO_PACKET);
	p.type = SC_LOGIN_INFO;
	p.x = x;
	p.y = y;
	p.hp = hp;
	p.max_hp = max_hp;
	do_send(&p);
}

void SESSION::send_move_packet(SESSION* client)
{
	SC_MOVE_OBJECT_PACKET p;
	p.id = client->_id;
	p.size = sizeof(SC_MOVE_OBJECT_PACKET);
	p.type = SC_MOVE_OBJECT;
	p.x = client->x;
	p.y = client->y;
	p.move_time = client->last_move_time;
	do_send(&p);
}

void SESSION::send_add_player_packet(SESSION* client)
{
	SC_ADD_OBJECT_PACKET add_packet;
	add_packet.id = client->_id;
	strcpy_s(add_packet.name, client->_name);
	add_packet.size = sizeof(add_packet);
	add_packet.type = SC_ADD_OBJECT;
	add_packet.x = client->x;
	add_packet.y = client->y;
	add_packet.hp = client->hp;
	_vl.lock();
	_view_list.insert(client->_id);
	_vl.unlock();
	do_send(&add_packet);
}

void SESSION::send_hit_player_packet(SESSION* client)
{
	SC_HIT_PACKET hit_packet;
	hit_packet.id = client->_id;
	hit_packet.hp = client->hp;
	hit_packet.size = sizeof(hit_packet);
	hit_packet.type = SC_HIT;
	do_send(&hit_packet);
}

void SESSION::send_death_player_packet(SESSION* client)
{
	SC_DEATH_PACKET death_packet;
	death_packet.id = client->_id;
	death_packet.x = client->x;
	death_packet.y = client->y;
	death_packet.size = sizeof(death_packet);
	death_packet.type = SC_DEATH;
	do_send(&death_packet);
}

void SESSION::send_attack_player_packet(SESSION* client)
{
	SC_ATTACK_OBJECT_PACKET attack_packet;
	attack_packet.id = client->_id;
	attack_packet.x = client->x;
	attack_packet.y = client->y;
	attack_packet.size = sizeof(attack_packet);
	attack_packet.type = SC_ATTACK_OBJECT;
	do_send(&attack_packet);
}

void SESSION::send_chat_packet(int c_id, const char* mess) {}

void SESSION::send_remove_player_packet(int c_id)
{
	_vl.lock();
	if (_view_list.count(c_id))
		_view_list.erase(c_id);
	else {
		_vl.unlock();
		return;
	}
	_vl.unlock();
	SC_REMOVE_OBJECT_PACKET p;
	p.id = c_id;
	p.size = sizeof(p);
	p.type = SC_REMOVE_OBJECT;
	do_send(&p);
}