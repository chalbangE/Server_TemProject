#include "SectorManager.h"

void SectorManager::SLInsert(int id, int s_x, int s_y)
{
	_st_lock[s_y][s_x].lock();
	sector_list[s_y][s_x].insert(id);
	_st_lock[s_y][s_x].unlock();
}

void SectorManager::SLErase(int id, int s_x, int s_y)
{
	_st_lock[s_y][s_x].lock();
	sector_list[s_y][s_x].erase(id);
	_st_lock[s_y][s_x].unlock();
}