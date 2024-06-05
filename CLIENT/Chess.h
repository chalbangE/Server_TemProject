#pragma once
#include "stdafx.h"

class Chess
{
	RECT imgsize{};
	RECT drowimgsize{};
	CImage img{};
	char	type;
	int		id;
	int		hp;
	int		max_hp;
	int		exp;
	int		level;
	short	x, y;

public:
	Chess();
	Chess(const std::string& str, RECT rt, int id);
	void Drow(HDC& mdc);
	void Move(int x, int y);
};

