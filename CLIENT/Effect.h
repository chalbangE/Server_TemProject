#pragma once
#include "stdafx.h"

enum EFFECT_TYPE {
	ET_P_ATTACK, ET_OP_ATTACK, ET_NPC_ATTACK, ET_P_DEATH, ET_OP_DEATH, ET_NPC_DEATH
};

class Effect
{
public:
	char type;
	char motion;
	char m_max;
	int x;
	int y;

	Effect(char a, int xx, int yy);

	void EF_Motion_Plus();
};