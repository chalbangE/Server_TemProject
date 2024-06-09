#pragma once
#include "stdafx.h"
#include "SESSION.h"

class SectorManager
{
public:
	unordered_set<int> sector_list[(W_HEIGHT / S_HEIGHT) + 1][(W_WIDTH / S_WIDTH) + 1];
	mutex _st_lock[(W_HEIGHT / S_HEIGHT) + 1][(W_WIDTH / S_WIDTH) + 1];

	void SLInsert(int id, int s_x, int s_y);
	void SLErase(int id, int s_x, int s_y);
};

