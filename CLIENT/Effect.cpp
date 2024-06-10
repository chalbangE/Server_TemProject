#include "Effect.h"

Effect::Effect(char a, int xx, int yy) : type(a), x(xx), y(yy) {
	motion = 0;

	switch (a)
	{
	case ET_P_ATTACK:
	case ET_OP_ATTACK:
	case ET_NPC_ATTACK: m_max = 4; break;
	case ET_P_DEATH:
	case ET_NPC_DEATH:
	case ET_OP_DEATH: m_max = 6; break;
	default:
		cout << "Effect(char a) - 존재하지 않는 Effect type 입니다!" << endl;
		break;
	}
}

void Effect::EF_Motion_Plus()
{
	++motion;
	if (m_max * 3 <= motion)
		motion = -1;
}
