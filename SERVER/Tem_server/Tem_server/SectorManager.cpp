#include "SectorManager.h"

void SectorManager::SLInsert(SESSION* ins)
{
	int s_x = ins->x / S_WIDTH;
	int s_y = ins->y / S_HEIGHT;

	_st_lock[s_y][s_x].lock();
	sector_list[s_y][s_x].insert(ins);
	_st_lock[s_y][s_x].unlock();
}

void SectorManager::SLErase(SESSION* era)
{
	int s_x = era->x / S_WIDTH;
	int s_y = era->y / S_HEIGHT;

	_st_lock[s_y][s_x].lock();
	sector_list[s_y][s_x].erase(era);
	_st_lock[s_y][s_x].unlock();
}