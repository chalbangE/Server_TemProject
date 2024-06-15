#if defined(DEBUG) | defined(_DEBUG) 
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console") 
#endif
#pragma once

#include "../SERVER/Tem_server/Tem_server/OVER_PLUS.h"
#include "Effect.h"
#include "Player.h"
	// #include "stdafx.h"
using namespace std;

extern bool bshutdown; // 종료 조건 변수
static void print_error(const char* msg, int err_no);

HINSTANCE g_hinst;
LPCTSTR IpszClass = L"Window Programming Lap";
LPCTSTR IpszWindowName = L"Window Programming Lap";

LRESULT CALLBACK WndProc(HWND hEnd, UINT iMessage, WPARAM wParam, LPARAM IParam);

short WIN_SIZE = 840;
short TILE_NUMBER = 21;
short TILE_SIZE = WIN_SIZE / TILE_NUMBER;
short TILE_IMG_SIZE = (WIN_SIZE / TILE_NUMBER) * 2;

constexpr char SERVER_ADDR[] = "127.0.0.1";

int								my_id = -1;
int								my_x, my_y;
int								my_exp, my_level;
short							my_motion;
char							my_dir;
unordered_map <int, Player>		players;
SOCKET							send_socket, server_soket;
WSAOVERLAPPED					wsaover;
vector<Effect>					effect;

void Client_Login();
void Send_Packet(void* packet);
void CALLBACK send_callback(DWORD err, DWORD sent_size, LPWSAOVERLAPPED pwsaover, DWORD sendflag);
void Recv_Packet();
void CALLBACK recv_callback(DWORD err, DWORD recv_size, LPWSAOVERLAPPED pwsaover, DWORD sendflag);
void Using_Packet(char* packet);

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hPrevinstance, LPSTR IpszCmdParam, int nCmdShow)
{ 
	// ------- 서버 붙이기 -------------------
	std::wcout.imbue(std::locale("korean")); // 한글로 오류 출력

	WSADATA WSAData{};
	int err = WSAStartup(MAKEWORD(2, 2), &WSAData);
	if (0 != err) {
		print_error("WSAStartup", WSAGetLastError());
	}

	server_soket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);

	connect(server_soket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));


	// ------ 윈프 초기설정 ---------------

	HWND hWnd;
	MSG Message;
	WNDCLASSEX WndClass;
	g_hinst = hinstance;

	WndClass.cbSize = sizeof(WndClass);
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = (WNDPROC)WndProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hinstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = IpszClass;
	WndClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassEx(&WndClass);

	hWnd = CreateWindow(IpszClass, L"TemProject", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_BORDER | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, 0, 0, 
		WIN_SIZE, WIN_SIZE, NULL, (HMENU)NULL, hinstance, NULL);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	while (GetMessage(&Message, 0, 0, 0)) {
		TranslateMessage(&Message);
		DispatchMessage(&Message);

		Recv_Packet();
		SleepEx(16, true);
	}

	return Message.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM IParam)
{
	srand(time(NULL));

	PAINTSTRUCT ps;
	HDC hdc{}; HDC mdc{};
	HBITMAP HBitmap, OldBitmap;
	HPEN hPen, oldPen;
	RECT window{ 0, 0, 840, 840 };

	static CImage ch_img, npc_img, other_ch_img, effect_img, hpbar_img;
	static array<CImage, 2> bg_tile_img;

	static bool control_on = false;

	// 메세지 처리하기
	switch (uMsg) {
	case WM_CREATE: {
		AdjustWindowRect(&window, WS_OVERLAPPEDWINDOW, false);         
		MoveWindow(hWnd, 150, 70, window.right - window.left, window.bottom - window.top, false);

		if (ch_img.IsNull()) {
			bg_tile_img[0].Load(TEXT("IMG/Tile_1.png"));
			bg_tile_img[1].Load(TEXT("IMG/Tile_2.png"));
			ch_img.Load(TEXT("IMG/player28-28.png"));
			other_ch_img.Load(TEXT("IMG/other-player28-28.png"));
			npc_img.Load(TEXT("IMG/npc28-28.png"));
			effect_img.Load(TEXT("IMG/Effect28-28.png"));
			hpbar_img.Load(TEXT("IMG/targetHPbar30-30.png"));
		}

		Client_Login();

		SetTimer(hWnd, 2, 10, 0);
		break;
	}
	case WM_SIZE:
	case WM_MOVE: {

		break;
	}
	case WM_PAINT: {
		if (my_id != -1) {
			hdc = BeginPaint(hWnd, &ps);
			mdc = CreateCompatibleDC(hdc);
			HBitmap = CreateCompatibleBitmap(hdc, window.right, window.bottom);
			OldBitmap = (HBITMAP)SelectObject(mdc, (HBITMAP)HBitmap);
			FillRect(mdc, &window, 0);

			// 배경 타일 깔기
			{
				if (my_x % 2 == my_y % 2) {
					bg_tile_img[0].Draw(mdc, 0, 0, WIN_SIZE, WIN_SIZE, 0, 0, WIN_SIZE, WIN_SIZE);
				}
				else {
					bg_tile_img[1].Draw(mdc, 0, 0, WIN_SIZE, WIN_SIZE, 0, 0, WIN_SIZE, WIN_SIZE);
				}

				hPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
				oldPen = (HPEN)SelectObject(mdc, hPen);
				if (my_x < TILE_NUMBER / 2 || my_y < TILE_NUMBER / 2) {
					Rectangle(mdc, 0, 0, ((TILE_NUMBER / 2) - my_x) * TILE_SIZE, WIN_SIZE); // 세로 네모
					Rectangle(mdc, 0, 0, WIN_SIZE, ((TILE_NUMBER / 2) - my_y) * TILE_SIZE); // 가로 네모
				}
				if ((W_WIDTH - my_x) <= (TILE_NUMBER / 2) || (W_HEIGHT - my_y) <= (TILE_NUMBER / 2)) {
					Rectangle(mdc, ((WIN_SIZE / 2) + (W_WIDTH - my_x) * TILE_SIZE) - (TILE_SIZE / 2), 0, WIN_SIZE, WIN_SIZE); // 세로 네모
					Rectangle(mdc, 0, ((WIN_SIZE / 2) + (W_HEIGHT - my_y) * TILE_SIZE) - (TILE_SIZE / 2), WIN_SIZE, WIN_SIZE); // 세로 네모
				}
				SelectObject(mdc, oldPen);
				DeleteObject(hPen);
			}



			// 그리기 (플레이어)
			for (const auto& p : players) {
				if (p.second.id >= MAX_USER)
					npc_img.Draw(mdc, ((10 - (my_x - p.second.x)) * (TILE_SIZE)) + 5, ((10 - (my_y - p.second.y)) * (TILE_SIZE)) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (my_motion / 5) * 28, int(p.second.dir) * 28, 28, 28);
				else if (p.second.id == my_id) continue;
				else other_ch_img.Draw(mdc, ((10 - (my_x - p.second.x)) * (TILE_SIZE)) + 5, ((10 - (my_y - p.second.y)) * (TILE_SIZE)) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (my_motion / 5) * 28, int(p.second.dir) * 28, 28, 28);

				hpbar_img.Draw(mdc, ((10 - (my_x - p.second.x)) * (TILE_SIZE)) + 5, ((10 - (my_y - p.second.y)) * (TILE_SIZE)) + 10, TILE_SIZE - 10, TILE_SIZE - 10, (p.second.hp - 1) * 30, 0, 30, 28);
			}
			// 이펙트
			for (const auto& e : effect) {
				effect_img.Draw(mdc, ((10 - (my_x - e.x)) * (TILE_SIZE)) + 5, ((10 - (my_y - e.y)) * (TILE_SIZE)) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (e.motion / 3) * 28, int(e.type) * 28, 28, 28);
			}

			ch_img.Draw(mdc, (10 * TILE_SIZE) + 5, (10 * TILE_SIZE) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (my_motion / 5) * 28, int(my_dir) * 28, 28, 28);


			BitBlt(hdc, 0, 0, window.right, window.bottom, mdc, 0, 0, SRCCOPY);

			SelectObject(mdc, OldBitmap);
			DeleteObject(HBitmap);
			DeleteDC(mdc);
			EndPaint(hWnd, &ps);
		}
		break;
	}
	case WM_TIMER: {

		switch (wParam)
		{
		case 2: {
			++my_motion;
			if (my_motion > 15)
				my_motion = 0;

			for (int i = 0; i < effect.size(); ++i) {
				effect[i].EF_Motion_Plus();

				if (effect[i].motion == -1) {
					effect.erase(effect.begin() + i);
					--i;
				}
			}

			InvalidateRect(hWnd, NULL, FALSE);
			break;
		}
		default:
			break;
		}
		break;
	}
	case WM_KEYDOWN: {
		//direction |  // 0 : RIGHT, 1 : LEFT, 2 : UP, 3 : DOWN
		char direction = -1;

		switch (wParam) {
		case VK_CONTROL: {
			control_on = true;
			break;
		}
		case 'd':
		case 'D': {
			direction = 0;
			break;
		}
		case 'a':
		case 'A': {
			direction = 1;
			break;
		}
		case 'w': 
		case 'W': {
			direction = 2;
			break;
		}
		case 's':
		case 'S': {
			direction = 3;
			break;
		}
		}

		if (-1 != direction) {
			my_dir = direction;
			CS_MOVE_PACKET p;
			p.size = sizeof(p);
			p.type = CS_MOVE;
			if (control_on)
				direction += 4;
			p.direction = direction;

			Send_Packet(&p);
		}
		break;
	}
	case WM_KEYUP: {
		switch (wParam) {
		case VK_CONTROL: {
			control_on = false;
			break;
		}
		}
		break;
	}
	case WM_LBUTTONUP: {

		if (1) { // 게임 중이라는 표시 해주기
			CS_ATTACK_PACKET p;
			p.size = sizeof(p);
			p.type = CS_ATTACK;
			p.direction = my_dir;

			Send_Packet(&p);
		}

		break;
	}
	case WM_DESTROY: {
		WSACleanup();
		PostQuitMessage(0);
		break;
	}
	default:
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, IParam);
}

void Client_Login()
{
	CS_LOGIN_PACKET p;
	p.size = sizeof(p);
	p.type = CS_LOGIN;
	p.name[0] = 'Y';
	p.name[1] = '\0';

	Send_Packet(&p);
}

void Recv_Packet()
{
	DWORD recv_flag = 0;
	OVER_PLUS* sdata = new OVER_PLUS();
	int res = WSARecv(server_soket, &sdata->_wsabuf, 1, 0, &recv_flag, &sdata->_over, recv_callback);
	if (0 != res) {
		int err_no = WSAGetLastError();
		// 에러 겹친 i/o 작업을 진행하고 있습니다. 라고 나오는 게 정상임
		if (WSA_IO_PENDING != err_no)
			print_error("Recv_Packet - WSARecv", WSAGetLastError());
	}
}
void CALLBACK recv_callback(DWORD err, DWORD recv_size, LPWSAOVERLAPPED pwsaover, DWORD sendflag)
{
	OVER_PLUS* over = reinterpret_cast<OVER_PLUS*>(pwsaover);

	static size_t save_data_size = 0;
	static size_t one_packet_size = 0;
	static char save_buf[CHAT_SIZE];
	char* buf = over->_wsabuf.buf;
	char recv_buf[CHAT_SIZE];

	if (save_data_size > 0) { // 전에 잘려서 저장해둔 패킷이 있으면 그거부터 하기
		memcpy(recv_buf, save_buf, save_data_size);
		memcpy(&recv_buf[save_data_size], buf, one_packet_size - save_data_size);
		buf += one_packet_size - save_data_size;
		recv_size -= one_packet_size - save_data_size;
		save_data_size = 0;
		Using_Packet(recv_buf);
	}

	while (1) {
		if (recv_size == 0) break; // 남은 데이터가 없으면 끝
		one_packet_size = buf[0]; // 패킷 하나 사이즈 등록하기
		if (one_packet_size > recv_size) { // 패킷 하나 사이즈보다 남은 버퍼 크기가 더 작으면 잘린거니까 save하기
			memcpy(save_buf, buf, recv_size);
			save_data_size = recv_size;
			break;
		}
		memcpy(recv_buf, buf, one_packet_size);
		Using_Packet(recv_buf);
		buf += one_packet_size;
		recv_size -= one_packet_size;
	}

	delete over;
}

void Using_Packet(char* packet_ptr)
{
	switch (packet_ptr[2])
	{
	case SC_LOGIN_INFO: {
		SC_LOGIN_INFO_PACKET* packet = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(packet_ptr);

		my_id = packet->id;
		my_x = packet->x;
		my_y = packet->y;

		players[my_id].id = packet->id;
		players[my_id].hp = packet->hp;
		players[my_id].max_hp = packet->max_hp;
		players[my_id].exp = packet->exp;
		players[my_id].level = packet->level;
		players[my_id].x = packet->x;
		players[my_id].y = packet->y;

		break;
	}
	case SC_LOGIN_FAIL: {
		cout << "로그인 실패!!!!" << endl;
		break;
	}
	case SC_ADD_OBJECT: {
		SC_ADD_OBJECT_PACKET* packet = reinterpret_cast<SC_ADD_OBJECT_PACKET*>(packet_ptr);

		players[packet->id].id = packet->id;
		players[packet->id].x = packet->x;
		players[packet->id].y = packet->y;
		players[packet->id].hp = packet->hp;
		strcpy_s(players[packet->id].name, packet->name);
		break;
	}
	case SC_REMOVE_OBJECT: {
		SC_REMOVE_OBJECT_PACKET* packet = reinterpret_cast<SC_REMOVE_OBJECT_PACKET*>(packet_ptr);

		players.erase(packet->id);
		break;
	}
	case SC_MOVE_OBJECT: {
		SC_MOVE_OBJECT_PACKET* packet = reinterpret_cast<SC_MOVE_OBJECT_PACKET*>(packet_ptr);
		Player* targetPlayer = &players[packet->id];

		//direction |  // 0 : RIGHT, 1 : LEFT, 2 : UP, 3 : DOWN
		targetPlayer->dir = packet->dir % 4;
		targetPlayer->x = packet->x;
		targetPlayer->y = packet->y;

		if (my_id == packet->id) {
			my_x = packet->x;
			my_y = packet->y;
		}

		break;
	}
	case SC_CHAT: {
		SC_CHAT_PACKET* packet = reinterpret_cast<SC_CHAT_PACKET*>(packet_ptr);
		break;
	}
	case SC_STAT_CHANGE: {
		SC_STAT_CHANGE_PACKET* packet = reinterpret_cast<SC_STAT_CHANGE_PACKET*>(packet_ptr);
		break;
	}
	case SC_HIT: {
		SC_HIT_PACKET* packet = reinterpret_cast<SC_HIT_PACKET*>(packet_ptr);

		players[packet->id].hp = packet->hp;
		cout << "[ " << packet->id << " ] 가 맞았습니다!" << endl;
		break;
	}
	case SC_DEATH: {
		SC_DEATH_PACKET* packet = reinterpret_cast<SC_DEATH_PACKET*>(packet_ptr);

		players.erase(packet->id);
		if (packet->id == my_id)
			effect.emplace_back(EFFECT_TYPE::ET_P_DEATH, packet->x, packet->y);
		else if (packet->id < MAX_USER)
			effect.emplace_back(EFFECT_TYPE::ET_OP_DEATH, packet->x, packet->y);
		else
			effect.emplace_back(EFFECT_TYPE::ET_NPC_DEATH, packet->x, packet->y);
		break;
	}
	case SC_ATTACK_OBJECT: {
		SC_ATTACK_OBJECT_PACKET* packet = reinterpret_cast<SC_ATTACK_OBJECT_PACKET*>(packet_ptr);

		if (packet->id == my_id)
			effect.emplace_back(EFFECT_TYPE::ET_P_ATTACK, packet->x, packet->y);
		else if (packet->id < MAX_USER)
			effect.emplace_back(EFFECT_TYPE::ET_OP_ATTACK, packet->x, packet->y);
		else
			effect.emplace_back(EFFECT_TYPE::ET_NPC_ATTACK, packet->x, packet->y);
		break;
	}
	default:
		break;
	}
}

void Send_Packet(void* packet)
{
	OVER_PLUS* sdata = new OVER_PLUS{ reinterpret_cast<char*>(packet) };
	int sed = WSASend(server_soket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, send_callback);
	if (0 != sed) {
		int err_no = WSAGetLastError();
		// 에러 겹친 i/o 작업을 진행하고 있습니다. 라고 나오는 게 정상임
		if (WSA_IO_PENDING != err_no)
			print_error("Send_Packet - WSASend", WSAGetLastError());
	}
}
void CALLBACK send_callback(DWORD err, DWORD sent_size, LPWSAOVERLAPPED pwsaover, DWORD sendflag)
{
	OVER_PLUS* over = reinterpret_cast<OVER_PLUS*>(pwsaover);
	delete over;
}

static void print_error(const char* msg, int err_no)
{
	WCHAR* msg_buf{};
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPWSTR>(&msg_buf), 0, NULL);
	std::cout << msg;
	std::wcout << L"\t에러 : " << msg_buf;
	while (true);
	LocalFree(msg_buf);
}