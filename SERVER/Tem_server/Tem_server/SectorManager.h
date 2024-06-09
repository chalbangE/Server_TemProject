#pragma once
#include "stdafx.h"
#include "SESSION.h"

class SectorManager
{
public:
	unordered_set<SESSION*> sector_list[(W_HEIGHT / S_HEIGHT) + 1][(W_WIDTH / S_WIDTH) + 1];
	mutex _st_lock[(W_HEIGHT / S_HEIGHT) + 1][(W_WIDTH / S_WIDTH) + 1];

	void SLInsert(SESSION* ins);
	void SLErase(SESSION* era);
};

