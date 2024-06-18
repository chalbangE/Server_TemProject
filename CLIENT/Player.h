#pragma once
#include "stdafx.h"

class Player
{

public:
	int				id;
	short			hp;
	short			max_hp;
	int				exp;
	int				level;
	short			x, y;
	char			dir;
	char			name[NAME_SIZE];
};

