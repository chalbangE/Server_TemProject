#include "MapManager.h"

MapManager::MapManager()
{
	Init_Map();
}

void MapManager::Init_Map()
{
	for (int y = 0; y < W_HEIGHT / S_HEIGHT; ++y) {
		for (int x = 0; x < W_WIDTH / S_WIDTH; ++x) {
			m_lock[y][x].lock();
			for (int i = 0; i < (S_WIDTH * S_HEIGHT * 0.02); ++i) {
				int m_y = rand() % S_HEIGHT;
				int m_x = rand() % S_WIDTH;

				map[(y * S_HEIGHT) + m_y][(x * S_WIDTH) + m_x] = (rand() % 2) + 1;
			}
			m_lock[y][x].unlock();
		}
	}
	 
	cout << "Init Map ¿Ï·á" << endl;
}

void MapManager::Change_Map(int x, int y, char what)
{
	m_lock[y / S_HEIGHT][x / S_WIDTH].lock();
	map[y][x] = what;
	m_lock[y / S_HEIGHT][x / S_WIDTH].unlock();
}
