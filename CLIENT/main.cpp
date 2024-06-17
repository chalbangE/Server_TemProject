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

POINT WIN_SIZE = { 840, 840 };
short TILE_NUMBER = 21;
short TILE_SIZE = WIN_SIZE.x / TILE_NUMBER;
short TILE_IMG_SIZE = (WIN_SIZE.x / TILE_NUMBER) * 2;

constexpr char SERVER_ADDR[] = "127.0.0.1";

enum UI_START {
	US_MY_HP, US_TARGET_HP, US_PARTY_HP
};
enum GAME_STATE {
	GS_LOGIN, GS_INGAME, GS_DEATH, GS_STOP
};

GAME_STATE							Game_state{GS_LOGIN};
short								my_motion;
unordered_map <int, Player>			players;
Player								my_info;
Player								rsp;
SOCKET								send_socket, server_soket;
WSAOVERLAPPED						wsaover;
vector<Effect>						effect;
char								w_map[W_HEIGHT][W_WIDTH]{};
array<POINT, 3>						ui_start{};
array<int, 3>						draw_hpbar_id{ -1 }; // hp바 그려야하는 애들 id / 직전에 때린 적, 파티원 1, 파티원 2 순서
char								chat_str[CHAT_SIZE]{};
char								now_chat_str[CHAT_SIZE]{};
chrono::system_clock::time_point	last_move_time;



void Client_Login(char* name);
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
		WIN_SIZE.x, WIN_SIZE.y, NULL, (HMENU)NULL, hinstance, NULL);
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
	HFONT hFont, OldFont;
	RECT window{ 0, 0, 840, 840 };

	static CImage ch_img, npc_img, other_ch_img, effect_img, hpbar_img, wall_img;
	static array<CImage, 2> bg_tile_img;
	static CImage login_bg_img, death_ui_img; // 4학년들은 안받을듯 ㅠㅠ

	static char name_str[NAME_SIZE]{};
	static bool control_on = false, chat_on = false;

	const wchar_t* fontPath = L"Ramche.ttf";
	AddFontResource(fontPath);

	// 메세지 처리하기

	if (Game_state == GS_LOGIN) {
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
				hpbar_img.Load(TEXT("IMG/Hpbar50-50.png"));
				wall_img.Load(TEXT("IMG/wall31-25.png"));
				login_bg_img.Load(TEXT("IMG/Login.png")); // 212 584
				death_ui_img.Load(TEXT("IMG/Death_ui.png"));
			}

			ui_start[US_TARGET_HP] = { 10, 10 };
			last_move_time = chrono::system_clock::now();
			break;
		}
		case WM_PAINT: {
			if (my_info.id != -1) {
				hdc = BeginPaint(hWnd, &ps);
				mdc = CreateCompatibleDC(hdc);
				HBitmap = CreateCompatibleBitmap(hdc, window.right, window.bottom);
				OldBitmap = (HBITMAP)SelectObject(mdc, (HBITMAP)HBitmap);
				FillRect(mdc, &window, 0);

				login_bg_img.Draw(mdc, 0, 0, WIN_SIZE.x, WIN_SIZE.y, 0, 0, WIN_SIZE.x, WIN_SIZE.y);

				{
					hFont = CreateFont(25, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Ramche");
					OldFont = (HFONT)SelectObject(mdc, hFont);
					SetTextColor(mdc, RGB(0, 0, 0));
					SetBkMode(mdc, RGB(255, 255, 255));

					// 출력할 텍스트 설정
					RECT rect;  
					rect.left = 212;     // 왼쪽 시작 좌표
					rect.top = 594;     // 아래쪽에서 시작
					rect.right = WIN_SIZE.x - 212;   // 가로 길이 제한

					// 내가 치고 있는 채팅
					DrawTextA(mdc, name_str, -1, &rect, DT_SINGLELINE | DT_CENTER);

					SelectObject(mdc, OldFont);
					DeleteObject(hFont);
				}

				BitBlt(hdc, 0, 0, window.right, window.bottom, mdc, 0, 0, SRCCOPY);

				DeleteObject(HBitmap);
				DeleteDC(mdc);
				EndPaint(hWnd, &ps);
			}
			break;
		}
		case WM_CHAR: {
			if (wParam == VK_BACK) {
				int len = strlen(name_str);
				if (len > 0)
					name_str[len - 1] = '\0';  // 마지막 문자 제거
			}
			else if (wParam == VK_RETURN) {
				Client_Login(name_str);
				strcpy_s(my_info.name, name_str);

				SetTimer(hWnd, 2, 10, 0);

				Game_state = GS_INGAME;
			}
			else if (strlen(name_str) < NAME_SIZE - 1) { // 공간이 남아 있을 때만 추가 
				int len = strlen(name_str);
				name_str[len] = (TCHAR)wParam;
				name_str[len + 1] = '\0';  // 문자열 끝에 null 문자 추가
			}

			InvalidateRect(hWnd, NULL, FALSE);
			break;
		}
		}
	}
	else if (Game_state == GS_INGAME) {
		switch (uMsg) {
		case WM_SIZE:
		case WM_MOVE: {

			break;
		}
		case WM_PAINT: {
			hdc = BeginPaint(hWnd, &ps);
			mdc = CreateCompatibleDC(hdc);
			HBitmap = CreateCompatibleBitmap(hdc, window.right, window.bottom);
			OldBitmap = (HBITMAP)SelectObject(mdc, (HBITMAP)HBitmap);
			FillRect(mdc, &window, 0);

			// 배경 타일 깔기
			{
				if (my_info.x % 2 == my_info.y % 2) {
					bg_tile_img[0].Draw(mdc, 0, 0, WIN_SIZE.x, WIN_SIZE.x, 0, 0, WIN_SIZE.x, WIN_SIZE.x);
				}
				else {
					bg_tile_img[1].Draw(mdc, 0, 0, WIN_SIZE.x, WIN_SIZE.x, 0, 0, WIN_SIZE.x, WIN_SIZE.x);
				}

				hPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
				oldPen = (HPEN)SelectObject(mdc, hPen);
				if (my_info.x < TILE_NUMBER / 2 || my_info.y < TILE_NUMBER / 2) {
					Rectangle(mdc, 0, 0, ((TILE_NUMBER / 2) - my_info.x) * TILE_SIZE, WIN_SIZE.x); // 세로 네모
					Rectangle(mdc, 0, 0, WIN_SIZE.x, ((TILE_NUMBER / 2) - my_info.y) * TILE_SIZE); // 가로 네모
				}
				if ((W_WIDTH - my_info.x) <= (TILE_NUMBER / 2) || (W_HEIGHT - my_info.y) <= (TILE_NUMBER / 2)) {
					Rectangle(mdc, ((WIN_SIZE.x / 2) + (W_WIDTH - my_info.x) * TILE_SIZE) - (TILE_SIZE / 2), 0, WIN_SIZE.x, WIN_SIZE.x); // 세로 네모
					Rectangle(mdc, 0, ((WIN_SIZE.x / 2) + (W_HEIGHT - my_info.y) * TILE_SIZE) - (TILE_SIZE / 2), WIN_SIZE.x, WIN_SIZE.x); // 세로 네모
				}
				SelectObject(mdc, oldPen);
				DeleteObject(hPen);
			}

			// 그리기 (플레이어)
			for (const auto& p : players) {
				if (p.second.id >= MAX_USER)
					npc_img.Draw(mdc, ((10 - (my_info.x - p.second.x)) * (TILE_SIZE)) + 5, ((10 - (my_info.y - p.second.y)) * (TILE_SIZE)) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (my_motion / 5) * 28, int(p.second.dir) * 28, 28, 28);
				else if (p.second.id == my_info.id) continue;
				else {
					other_ch_img.Draw(mdc, ((10 - (my_info.x - p.second.x)) * (TILE_SIZE)) + 5, ((10 - (my_info.y - p.second.y)) * (TILE_SIZE)) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (my_motion / 5) * 28, int(p.second.dir) * 28, 28, 28);

					hFont = CreateFont(10, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Ramche");
					OldFont = (HFONT)SelectObject(mdc, hFont);
					SetTextColor(mdc, RGB(0, 0, 0));
					SetBkMode(mdc, RGB(255, 255, 255));

					// 출력할 텍스트 설정
					RECT rect;
					rect.left = (my_info.x - p.second.x) * (TILE_SIZE);     // 왼쪽 시작 좌표
					rect.right = rect.left + TILE_SIZE;   // 가로 길이 제한
					rect.top = ((10 - (my_info.y - p.second.y)) * (TILE_SIZE)) + TILE_SIZE;    
					rect.bottom = ((10 - (my_info.y - p.second.y)) * (TILE_SIZE)) + TILE_SIZE + (TILE_SIZE / 2); 

					// 내가 치고 있는 채팅
					DrawTextA(mdc, p.second.name, -1, &rect, DT_WORDBREAK | DT_NOCLIP | DT_CENTER);

					SelectObject(mdc, OldFont);
					DeleteObject(hFont);
				}
			}

			for (int y = my_info.y - VIEW_RANGE; y < my_info.y + VIEW_RANGE; ++y) {
				for (int x = my_info.x - VIEW_RANGE; x < my_info.x + VIEW_RANGE; ++x) {
					if (w_map[y][x] == MI_FREE) continue;
					if (y < 0 || y >= W_HEIGHT || x < 0 || x >= W_WIDTH) continue;
					switch (w_map[y][x])
					{
					case MI_SOILD_WALL:
					case MI_CRACK_WALL:
					case MI_ITEM: {
						wall_img.Draw(mdc, ((10 - (my_info.x - x)) * (TILE_SIZE)) + 5, ((10 - (my_info.y - y)) * (TILE_SIZE)) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (int(w_map[y][x]) - 1) * 31, 0, 31, 25);
						break;
					}
					default:
						break;
					}
				}
			}

			// 이펙트
			for (const auto& e : effect) {
				effect_img.Draw(mdc, ((10 - (my_info.x - e.x)) * (TILE_SIZE)) + 5, ((10 - (my_info.y - e.y)) * (TILE_SIZE)) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (e.motion / 3) * 28, int(e.type) * 28, 28, 28);
			}

			ch_img.Draw(mdc, (10 * TILE_SIZE) + 5, (10 * TILE_SIZE) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (my_motion / 5) * 28, int(my_info.dir) * 28, 28, 28);

			// 내 HP바
			for (int i = 1; i <= my_info.max_hp; ++i) {
				if (i <= my_info.hp)
					hpbar_img.Draw(mdc, ui_start[US_MY_HP].x + ((i - 1) * 50), ui_start[US_MY_HP].y, 50, 50, 50, 0, 50, 50);
				else
					hpbar_img.Draw(mdc, ui_start[US_MY_HP].x + ((i - 1) * 50), ui_start[US_MY_HP].y, 50, 50, 0, 0, 50, 50);
			}
			// 마지막으로 때린 놈 HP바
			if (draw_hpbar_id[0] != -1) {
				if (draw_hpbar_id[0] < MAX_USER) {
					for (int i = 1; i <= players[draw_hpbar_id[0]].max_hp; ++i) {
						if (i <= players[draw_hpbar_id[0]].hp)
							hpbar_img.Draw(mdc, ui_start[US_TARGET_HP].x + ((i - 1) * 50), ui_start[US_TARGET_HP].y, 50, 50, 100, 0, 50, 50);
						else
							hpbar_img.Draw(mdc, ui_start[US_TARGET_HP].x + ((i - 1) * 50), ui_start[US_TARGET_HP].y, 50, 50, 0, 0, 50, 50);
					}
				}
				else {
					for (int i = 1; i <= players[draw_hpbar_id[0]].max_hp; ++i) {
						if (i <= players[draw_hpbar_id[0]].hp)
							hpbar_img.Draw(mdc, ui_start[US_TARGET_HP].x + ((i - 1) * 50), ui_start[US_TARGET_HP].y, 50, 50, 150, 0, 50, 50);
						else
							hpbar_img.Draw(mdc, ui_start[US_TARGET_HP].x + ((i - 1) * 50), ui_start[US_TARGET_HP].y, 50, 50, 0, 0, 50, 50);
					}
				}
			}

			// 채팅 그리기
			{
				hFont = CreateFont(18, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Ramche");
				OldFont = (HFONT)SelectObject(mdc, hFont);
				SetTextColor(mdc, RGB(0, 0, 0));
				SetBkMode(mdc, RGB(255, 255, 255));

				// 출력할 텍스트 설정
				RECT rect;
				rect.left = 10;     // 왼쪽 시작 좌표
				rect.top = WIN_SIZE.y;     // 아래쪽에서 시작
				rect.right = WIN_SIZE.x / 2;   // 가로 길이 제한

				// 내가 치고 있는 채팅
				DrawTextA(mdc, now_chat_str, -1, &rect, DT_WORDBREAK | DT_LEFT | DT_CALCRECT);
				int textHeight = rect.bottom - rect.top; // 텍스트의 높이
				rect.top -= textHeight + 10;     // 위로 올림
				rect.right = WIN_SIZE.x / 2;   // 가로 길이 제한
				DrawTextA(mdc, now_chat_str, -1, &rect, DT_WORDBREAK | DT_LEFT);

				rect.right = WIN_SIZE.x / 2;   // 가로 길이 제한
				SetBkMode(mdc, TRANSPARENT);
				// 내가 치고 있는 채팅 위로 직전 채팅
				DrawTextA(mdc, chat_str, -1, &rect, DT_WORDBREAK | DT_LEFT | DT_CALCRECT);
				textHeight = rect.bottom - rect.top; // 텍스트의 높이
				rect.top -= textHeight + 10;     // 위로 올림
				rect.right = WIN_SIZE.x / 2;   // 가로 길이 제한
				DrawTextA(mdc, chat_str, -1, &rect, DT_WORDBREAK | DT_LEFT);

				SelectObject(mdc, OldFont);
				DeleteObject(hFont);
			}

			BitBlt(hdc, 0, 0, window.right, window.bottom, mdc, 0, 0, SRCCOPY);

			DeleteObject(HBitmap);
			DeleteDC(mdc);
			EndPaint(hWnd, &ps);
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

			if (chat_on) {
				if (wParam == VK_BACK) {
					int len = strlen(now_chat_str);
					if (len > 0)
						now_chat_str[len - 1] = '\0';  // 마지막 문자 제거
				}
				else if (wParam == VK_RETURN) {
					chat_on = false;
					if (0 == strlen(now_chat_str)) break;

					CS_CHAT_PACKET p;
					strcpy_s(p.mess, CHAT_SIZE, now_chat_str);
					memset(now_chat_str, '\0', sizeof(now_chat_str));
					p.size = sizeof(p);
					p.type = CS_CHAT;

					Send_Packet(&p);
				}
				break;
			}

			switch (wParam) {
			case VK_CONTROL: {
				control_on = true;
				break;
			}
			case VK_RETURN: {
				chat_on = true;
				break;
			}
			case VK_SPACE: {
				for (int i = 0; i < 4; ++i) {
					CS_ATTACK_PACKET p;
					p.size = sizeof(p);
					p.type = CS_ATTACK;
					p.direction = i;

					Send_Packet(&p);
				}
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

			chrono::system_clock::time_point now = chrono::system_clock::now();
			if (-1 != direction && last_move_time <= now - 1s) {
				my_info.dir = direction;
				CS_MOVE_PACKET p;
				p.size = sizeof(p);
				p.type = CS_MOVE;
				if (control_on)
					direction += 4;
				p.direction = direction;

				Send_Packet(&p);
				last_move_time = now;
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
		case WM_CHAR: {
			if (strlen(now_chat_str) < CHAT_SIZE - 1 && chat_on) { // 공간이 남아 있을 때만 추가 
				if (wParam == VK_RETURN || wParam == VK_BACK) break;
				int len = strlen(now_chat_str);
				now_chat_str[len] = (TCHAR)wParam;
				now_chat_str[len + 1] = '\0';  // 문자열 끝에 null 문자 추가
			}
			break;
		}
		case WM_LBUTTONUP: {

			CS_ATTACK_PACKET p;
			p.size = sizeof(p);
			p.type = CS_ATTACK;
			p.direction = my_info.dir;

			Send_Packet(&p);

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
	}
	else if (Game_state == GS_DEATH) { 
		switch (uMsg) {
		case WM_PAINT: {
			hdc = BeginPaint(hWnd, &ps);
			mdc = CreateCompatibleDC(hdc);
			HBitmap = CreateCompatibleBitmap(hdc, window.right, window.bottom);
			OldBitmap = (HBITMAP)SelectObject(mdc, (HBITMAP)HBitmap);
			FillRect(mdc, &window, 0);

			// 배경 타일 깔기
			{
				if (my_info.x % 2 == my_info.y % 2) {
					bg_tile_img[0].Draw(mdc, 0, 0, WIN_SIZE.x, WIN_SIZE.x, 0, 0, WIN_SIZE.x, WIN_SIZE.x);
				}
				else {
					bg_tile_img[1].Draw(mdc, 0, 0, WIN_SIZE.x, WIN_SIZE.x, 0, 0, WIN_SIZE.x, WIN_SIZE.x);
				}

				hPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
				oldPen = (HPEN)SelectObject(mdc, hPen);
				if (my_info.x < TILE_NUMBER / 2 || my_info.y < TILE_NUMBER / 2) {
					Rectangle(mdc, 0, 0, ((TILE_NUMBER / 2) - my_info.x) * TILE_SIZE, WIN_SIZE.x); // 세로 네모
					Rectangle(mdc, 0, 0, WIN_SIZE.x, ((TILE_NUMBER / 2) - my_info.y) * TILE_SIZE); // 가로 네모
				}
				if ((W_WIDTH - my_info.x) <= (TILE_NUMBER / 2) || (W_HEIGHT - my_info.y) <= (TILE_NUMBER / 2)) {
					Rectangle(mdc, ((WIN_SIZE.x / 2) + (W_WIDTH - my_info.x) * TILE_SIZE) - (TILE_SIZE / 2), 0, WIN_SIZE.x, WIN_SIZE.x); // 세로 네모
					Rectangle(mdc, 0, ((WIN_SIZE.x / 2) + (W_HEIGHT - my_info.y) * TILE_SIZE) - (TILE_SIZE / 2), WIN_SIZE.x, WIN_SIZE.x); // 세로 네모
				}
				SelectObject(mdc, oldPen);
				DeleteObject(hPen);
			}


			// 그리기 (플레이어)
			for (const auto& p : players) {
				if (p.second.id >= MAX_USER)
					npc_img.Draw(mdc, ((10 - (my_info.x - p.second.x)) * (TILE_SIZE)) + 5, ((10 - (my_info.y - p.second.y)) * (TILE_SIZE)) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (my_motion / 5) * 28, int(p.second.dir) * 28, 28, 28);
				else if (p.second.id == my_info.id) continue;
				else {
					other_ch_img.Draw(mdc, ((10 - (my_info.x - p.second.x)) * (TILE_SIZE)) + 5, ((10 - (my_info.y - p.second.y)) * (TILE_SIZE)) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (my_motion / 5) * 28, int(p.second.dir) * 28, 28, 28);

					hFont = CreateFont(10, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Ramche");
					OldFont = (HFONT)SelectObject(mdc, hFont);
					SetTextColor(mdc, RGB(0, 0, 0));
					SetBkMode(mdc, RGB(255, 255, 255));

					// 출력할 텍스트 설정
					RECT rect;
					rect.left = (my_info.x - p.second.x) * (TILE_SIZE);     // 왼쪽 시작 좌표
					rect.right = rect.left + TILE_SIZE;   // 가로 길이 제한
					rect.top = ((10 - (my_info.y - p.second.y)) * (TILE_SIZE)) + TILE_SIZE;
					rect.bottom = ((10 - (my_info.y - p.second.y)) * (TILE_SIZE)) + TILE_SIZE + (TILE_SIZE / 2);

					// 내가 치고 있는 채팅
					DrawTextA(mdc, p.second.name, -1, &rect, DT_WORDBREAK | DT_NOCLIP | DT_CENTER);

					SelectObject(mdc, OldFont);
					DeleteObject(hFont);
				}
			}

			for (int y = my_info.y - VIEW_RANGE; y < my_info.y + VIEW_RANGE; ++y) {
				for (int x = my_info.x - VIEW_RANGE; x < my_info.x + VIEW_RANGE; ++x) {
					if (w_map[y][x] == MI_FREE) continue;
					if (y < 0 || y >= W_HEIGHT || x < 0 || x >= W_WIDTH) continue;
					switch (w_map[y][x])
					{
					case MI_SOILD_WALL:
					case MI_CRACK_WALL:
					case MI_ITEM: {
						wall_img.Draw(mdc, ((10 - (my_info.x - x)) * (TILE_SIZE)) + 5, ((10 - (my_info.y - y)) * (TILE_SIZE)) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (int(w_map[y][x]) - 1) * 31, 0, 31, 25);
						break;
					}
					default:
						break;
					}
				}
			}

			// 이펙트
			for (const auto& e : effect) {
				effect_img.Draw(mdc, ((10 - (my_info.x - e.x)) * (TILE_SIZE)) + 5, ((10 - (my_info.y - e.y)) * (TILE_SIZE)) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (e.motion / 3) * 28, int(e.type) * 28, 28, 28);
			}

			ch_img.Draw(mdc, (10 * TILE_SIZE) + 5, (10 * TILE_SIZE) + 5, TILE_SIZE - 10, TILE_SIZE - 10, (my_motion / 5) * 28, int(my_info.dir) * 28, 28, 28);

			// 내 HP바
			for (int i = 1; i <= my_info.max_hp; ++i) {
				if (i <= my_info.hp)
					hpbar_img.Draw(mdc, ui_start[US_MY_HP].x + ((i - 1) * 50), ui_start[US_MY_HP].y, 50, 50, 50, 0, 50, 50);
				else
					hpbar_img.Draw(mdc, ui_start[US_MY_HP].x + ((i - 1) * 50), ui_start[US_MY_HP].y, 50, 50, 0, 0, 50, 50);
			}
			// 마지막으로 때린 놈 HP바
			if (draw_hpbar_id[0] != -1) {
				if (draw_hpbar_id[0] < MAX_USER) {
					for (int i = 1; i <= players[draw_hpbar_id[0]].max_hp; ++i) {
						if (i <= players[draw_hpbar_id[0]].hp)
							hpbar_img.Draw(mdc, ui_start[US_TARGET_HP].x + ((i - 1) * 50), ui_start[US_TARGET_HP].y, 50, 50, 100, 0, 50, 50);
						else
							hpbar_img.Draw(mdc, ui_start[US_TARGET_HP].x + ((i - 1) * 50), ui_start[US_TARGET_HP].y, 50, 50, 0, 0, 50, 50);
					}
				}
				else {
					for (int i = 1; i <= players[draw_hpbar_id[0]].max_hp; ++i) {
						if (i <= players[draw_hpbar_id[0]].hp)
							hpbar_img.Draw(mdc, ui_start[US_TARGET_HP].x + ((i - 1) * 50), ui_start[US_TARGET_HP].y, 50, 50, 150, 0, 50, 50);
						else
							hpbar_img.Draw(mdc, ui_start[US_TARGET_HP].x + ((i - 1) * 50), ui_start[US_TARGET_HP].y, 50, 50, 0, 0, 50, 50);
					}
				}
			}

			// 채팅 그리기
			{
				hFont = CreateFont(18, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Ramche");
				OldFont = (HFONT)SelectObject(mdc, hFont);
				SetTextColor(mdc, RGB(0, 0, 0));
				SetBkMode(mdc, RGB(255, 255, 255));

				// 출력할 텍스트 설정
				RECT rect;
				rect.left = 10;     // 왼쪽 시작 좌표
				rect.top = WIN_SIZE.y;     // 아래쪽에서 시작
				rect.right = WIN_SIZE.x / 2;   // 가로 길이 제한

				// 내가 치고 있는 채팅
				DrawTextA(mdc, now_chat_str, -1, &rect, DT_WORDBREAK | DT_LEFT | DT_CALCRECT);
				int textHeight = rect.bottom - rect.top; // 텍스트의 높이
				rect.top -= textHeight + 10;     // 위로 올림
				rect.right = WIN_SIZE.x / 2;   // 가로 길이 제한
				DrawTextA(mdc, now_chat_str, -1, &rect, DT_WORDBREAK | DT_LEFT);

				rect.right = WIN_SIZE.x / 2;   // 가로 길이 제한
				SetBkMode(mdc, TRANSPARENT);
				// 내가 치고 있는 채팅 위로 직전 채팅
				DrawTextA(mdc, chat_str, -1, &rect, DT_WORDBREAK | DT_LEFT | DT_CALCRECT);
				textHeight = rect.bottom - rect.top; // 텍스트의 높이
				rect.top -= textHeight + 10;     // 위로 올림
				rect.right = WIN_SIZE.x / 2;   // 가로 길이 제한
				DrawTextA(mdc, chat_str, -1, &rect, DT_WORDBREAK | DT_LEFT);

				SelectObject(mdc, OldFont);
				DeleteObject(hFont);
			}

			death_ui_img.Draw(mdc, 0, 0, WIN_SIZE.x, WIN_SIZE.y, 0, 0, WIN_SIZE.x, WIN_SIZE.y);

			BitBlt(hdc, 0, 0, window.right, window.bottom, mdc, 0, 0, SRCCOPY);

			DeleteObject(HBitmap);
			DeleteDC(mdc);
			EndPaint(hWnd, &ps);
			break;
		}
		case WM_CHAR: {
			if (wParam == VK_SPACE) {
				Game_state = GS_INGAME;

				rsp.exp = my_info.exp;
				rsp.level = my_info.level;

				my_info = rsp;
				players[my_info.id] = rsp;

				CS_RESPAWN_PACKET p;
				p.size = sizeof(p);
				p.type = CS_RESPAWN;
				p.x = rsp.x;
				p.y = rsp.y;

				Send_Packet(&p);
			}

			InvalidateRect(hWnd, NULL, FALSE);
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
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, IParam);
}

void Client_Login(char* name)
{
	CS_LOGIN_PACKET p;
	p.size = sizeof(p);
	p.type = CS_LOGIN;
	strcpy_s(p.name, NAME_SIZE, name);

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
	static char save_buf[CHAT_SIZE * 2];
	char* buf = over->_wsabuf.buf;
	char recv_buf[CHAT_SIZE * 2];

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
		if (buf[2] == SC_CHAT) {
			cout << 'd' << endl;
		}
		WORD* byte = reinterpret_cast<WORD*>(buf);
		one_packet_size = *byte; // 패킷 하나 사이즈 등록하기
		if (one_packet_size > recv_size) { // 패킷 하나 사이즈보다 남은 버퍼 크기가 더 작으면 잘린거니까 save하기
			memcpy(save_buf, buf, recv_size);
			save_data_size = recv_size;
			break;
		}
		memcpy(recv_buf, buf, one_packet_size);
		if (recv_buf[2] == SC_CHAT) {
			cout << 'd' << endl;
		}
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

		my_info.id = packet->id;

		players[my_info.id].id = packet->id;
		players[my_info.id].hp = packet->hp;
		players[my_info.id].max_hp = packet->max_hp;
		players[my_info.id].exp = packet->exp;
		players[my_info.id].level = packet->level;
		players[my_info.id].x = packet->x;
		players[my_info.id].y = packet->y;
		
		strcpy_s(players[my_info.id].name, my_info.name);
		
		my_info = rsp = players[my_info.id];

		ui_start[US_MY_HP] = { WIN_SIZE.x - (my_info.max_hp * 50) - 10, WIN_SIZE.y - 60 };
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
		players[packet->id].max_hp = packet->max_hp;
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

		if (my_info.id == packet->id) {
			my_info.x = packet->x;
			my_info.y = packet->y;
		}

		break;
	}
	case SC_CHAT: {
		SC_CHAT_PACKET* packet = reinterpret_cast<SC_CHAT_PACKET*>(packet_ptr);

		snprintf(chat_str, sizeof(chat_str), "[%s] %s", players[packet->id].name, packet->mess);
		break;
	}
	case SC_STAT_CHANGE: {
		SC_STAT_CHANGE_PACKET* packet = reinterpret_cast<SC_STAT_CHANGE_PACKET*>(packet_ptr);
		break;
	}
	case SC_HP_UPDATE: {
		SC_HP_UPDATE_PACKET* packet = reinterpret_cast<SC_HP_UPDATE_PACKET*>(packet_ptr);

		// 아이템 먹었을 때
		if (packet->attack_id == -1) {
			snprintf(chat_str, sizeof(chat_str), "[시스템] %s가 뭔가 주워먹구 치료!", players[packet->id].name);
		}

		if (players[packet->id].hp > packet->hp) {
			// 딜 했을 때
			if (my_info.id == packet->attack_id)
				draw_hpbar_id[0] = packet->id;
			snprintf(chat_str, sizeof(chat_str), "[시스템] %s가 %s에게 얻어맞았다!", players[packet->id].name, players[packet->attack_id].name);
		}
		else if (players[packet->id].hp < packet->hp) {
			snprintf(chat_str, sizeof(chat_str), "[시스템] %s가 뭔가 주워먹구 치료!", players[packet->id].name);
		}

		players[packet->id].hp = packet->hp;

		if (my_info.id == packet->id) {
			my_info.hp = packet->hp;
		}
		break;
	}
	case SC_DEATH: {
		SC_DEATH_PACKET* packet = reinterpret_cast<SC_DEATH_PACKET*>(packet_ptr);
		
		if (packet->id == my_info.id) {
			effect.emplace_back(EFFECT_TYPE::ET_P_DEATH, packet->x, packet->y);
			my_info.hp = 0;
			Game_state = GS_DEATH;
			char msg = rand() % 3;
			if (msg == 0)
				snprintf(chat_str, sizeof(chat_str), "[시스템] 컨트롤이 부족해서 죽어버렸네요...");
			else if (msg == 1)
				snprintf(chat_str, sizeof(chat_str), "[시스템] 죽었다! 실력을 더 길러야겠어요");
			else if (msg == 2)
				snprintf(chat_str, sizeof(chat_str), "[시스템] 아... 딱 요 정도?");
		}
		else if (packet->id < MAX_USER) {
			effect.emplace_back(EFFECT_TYPE::ET_OP_DEATH, packet->x, packet->y);
			snprintf(chat_str, sizeof(chat_str), "[시스템] %s가 죽었다!", players[packet->id].name);
		}
		else {
			effect.emplace_back(EFFECT_TYPE::ET_NPC_DEATH, packet->x, packet->y);
			snprintf(chat_str, sizeof(chat_str), "[시스템] %s가 죽었다!", players[packet->id].name);
		}
		players.erase(packet->id);
		break;
	}
	case SC_ATTACK_OBJECT: {
		SC_ATTACK_OBJECT_PACKET* packet = reinterpret_cast<SC_ATTACK_OBJECT_PACKET*>(packet_ptr);

		if (packet->id == my_info.id)
			effect.emplace_back(EFFECT_TYPE::ET_P_ATTACK, packet->x, packet->y);
		else if (packet->id < MAX_USER)
			effect.emplace_back(EFFECT_TYPE::ET_OP_ATTACK, packet->x, packet->y);
		else
			effect.emplace_back(EFFECT_TYPE::ET_NPC_ATTACK, packet->x, packet->y);
		break;
	}
	case SC_CHANGE_MAP: {
		SC_CHANGE_MAP_PACKET* packet = reinterpret_cast<SC_CHANGE_MAP_PACKET*>(packet_ptr);

		w_map[packet->y][packet->x] = packet->what;
		if (packet->what == MI_ITEM) {
			// 아이템 여러개 만들지 고민 중
		}
		break;
	}
	case SC_RESPAWN: {
		SC_RESPAWN_PACKET* packet = reinterpret_cast<SC_RESPAWN_PACKET*>(packet_ptr);

		snprintf(chat_str, sizeof(chat_str), "[시스템] %s > 부 활 <", players[packet->id].name);
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