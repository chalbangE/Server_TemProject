#pragma once
#include "stdafx.h"

class MapManager
{
public:
	char map[W_HEIGHT][W_WIDTH]{};
	mutex m_lock[(W_HEIGHT / S_HEIGHT) + 1][(W_WIDTH / S_HEIGHT) + 1]{};

	MapManager();

	void Init_Map();
	void Change_Map(int x, int y, char what);
};
