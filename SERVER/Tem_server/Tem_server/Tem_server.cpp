#include "stdafx.h"
#include "GameManager.h"

using namespace std;

constexpr int VIEW_RANGE = 15;

GameManager GM;

int main()
{
	GM.Worker_thread();
}
