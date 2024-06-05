#pragma once

#include "stdafx.h"
#include "Chess.h"
#include "define.h"

extern WSABUF recv_wsabuf[1];
extern WSABUF send_wsabuf[1];
extern char buf[BUFSIZE];
extern SOCKET server_s;

// read_n_send 안의 지역변수로 했을경우 함수가 종료되면 주소값을 보내준것이 의미가 없음
extern WSAOVERLAPPED wsaover;

extern bool bshutdown; // 종료 조건 변수

Chess::Chess()
{
}

Chess::Chess(const std::string& str, RECT rt, int id) : drowimgsize(rt), id(id)
{
	std::wstring wstr{ L"" };

	wstr.assign(str.begin(), str.end());

	img.Load(wstr.c_str());

	imgsize.right = img.GetWidth();
	imgsize.bottom = img.GetHeight();
}

void Chess::Drow(HDC& mdc)
{
	img.Draw(mdc, x * drowimgsize.right, y * drowimgsize.bottom, drowimgsize.right, drowimgsize.bottom, 0, 0, imgsize.right, imgsize.bottom);
}

void Chess::Move(int change_x, int change_y)
{
	x = change_x;
	y = change_y;
}
