#pragma once

#include "stdafx.h"
#include "ChessGame.h"
#include "Chess.h"
#include "define.h"
#include <cassert>

void CALLBACK recv_callback(DWORD err, DWORD recv_size, LPWSAOVERLAPPED pwsaover, DWORD sendflag);
void CALLBACK send_callback(DWORD err, DWORD sent_size, LPWSAOVERLAPPED pwsaover, DWORD sendflag);

extern WSABUF recv_wsabuf[1]; 
extern WSABUF send_wsabuf[1];
extern char buf[BUFSIZE];

// read_n_send 안의 지역변수로 했을경우 함수가 종료되면 주소값을 보내준것이 의미가 없음
extern WSAOVERLAPPED wsaover;

extern bool bshutdown; // 종료 조건 변수

extern ChessGame chessgame;

static void print_error(const char* msg, int err_no);

ChessGame::ChessGame()
{
	backgroud.Load(TEXT("IMG/Background.png"));

	w = backgroud.GetWidth();
	h = backgroud.GetHeight();
}

ChessGame::ChessGame(SOCKET& server) : sev_s{ server }
{ }

void ChessGame::init(SOCKET& server)
{
	sev_s = server;
	recv_wsabuf[0].buf = reinterpret_cast<char*>(&pos_p);
	recv_wsabuf[0].len = sizeof(POS_PACKET);
	send_wsabuf[0].buf = reinterpret_cast<char*>(&key_p);
	send_wsabuf[0].len = sizeof(KEY_PACKET);

	if (backgroud.IsNull()) {
		backgroud.Load(TEXT("IMG/Background.png"));

		w = backgroud.GetWidth();
		h = backgroud.GetHeight();
	}
}

void ChessGame::Drow(HDC& mdc)
{
	backgroud.Draw(mdc, 0, 0, w, h, 0, 0, w, h);

	for (auto& ch : chesses) {
		if (ch.second != nullptr)
			ch.second->Drow(mdc);
	}
}

void ChessGame::AddChess()
{
	ZeroMemory(&wsaover, sizeof(wsaover));   // recv에서 사용하기 전에 초기화
	memset(&wsaover, 0, sizeof(wsaover));
	int sed = WSASend(sev_s, send_wsabuf, 1, nullptr, 0, &wsaover, send_callback);
	if (0 != sed) {
		int err_no = WSAGetLastError();
		// 에러 겹친 i/o 작업을 진행하고 있습니다. 라고 나오는 게 정상임
		if (WSA_IO_PENDING != err_no)
			print_error("AddChess - WSASend", WSAGetLastError());
	}

	while (1) {
		do_recv();

		if (pos_p.id != -1) {
			const std::string str{ "IMG/Momonga" + std::to_string((pos_p.id % 10) + 1) + ".png" };
			RECT rt{ 0, 0, 100, 100 };
			Chess* ch = new Chess{ str, rt, pos_p.id };
			chesses.try_emplace(pos_p.id, ch);
			chesses[pos_p.id]->Move(pos_p.x, pos_p.y);

			id = pos_p.id;
		}
		else
			break;

	}
	std::cout << "만들고 체스 갯수 : " << chesses.size() << std::endl;
}

void ChessGame::MinusChess()
{
	key_p.type = LOGOUT_TYPE;
	key_p.id = id;

	DWORD sent_size{};
	int sed = WSASend(sev_s, send_wsabuf, 1, &sent_size, 0, &wsaover, send_callback);
	if (0 != sed) {
		int err_no = WSAGetLastError();
		// 에러 겹친 i/o 작업을 진행하고 있습니다. 라고 나오는 게 정상임
		if (WSA_IO_PENDING != err_no)
			print_error("MinusChess - WSASend", WSAGetLastError());
	}
}

void ChessGame::Move()
{
	DWORD recv_flag{ 0 };

	do_recv();

	std::cout << "Move - x : " << pos_p.x << "   y : " << pos_p.y << "\n";
}

void ChessGame::PressKey(WPARAM wParam)
{
	key_p.type = KEY_TYPE;
	key_p.key = wParam;
	DWORD sent_size{};

	int sed = WSASend(sev_s, send_wsabuf, 1, &sent_size, 0, &wsaover, send_callback);
	if (0 != sed) {
		int err_no = WSAGetLastError();
		// 에러 겹친 i/o 작업을 진행하고 있습니다. 라고 나오는 게 정상임
		if (WSA_IO_PENDING != err_no)
			print_error("PressKey - WSASend", WSAGetLastError());
	}

	Move();
}

void ChessGame::do_recv()
{
	DWORD recv_flag{ 0 };
	int res = WSARecv(sev_s, recv_wsabuf, 1, nullptr, &recv_flag, &wsaover, recv_callback);
	if (0 != res) {
		int err_no = WSAGetLastError();
		// 에러 겹친 i/o 작업을 진행하고 있습니다. 라고 나오는 게 정상임
		if (WSA_IO_PENDING != err_no)
			print_error("WSARecv", WSAGetLastError());
	}
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
	//#ifdef _DEBUG
	//assert(false);
	//#endif _DEBUG
	LocalFree(msg_buf);
}