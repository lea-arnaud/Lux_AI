#ifndef BOT_H
#define BOT_H

#include <string>

enum UNIT_TYPE {
	WORKER = 0,
	CART,
	CITY
};

class Bot
{
	std::string m_id;
	int m_x, m_y;
	int m_cooldown;
	int m_wood, m_coal, m_uranium;
	UNIT_TYPE m_type;
	int m_team;

public:
	Bot(std::string id, int x, int y, int cooldown, int wood, int coal, int uranium, UNIT_TYPE type, int team) : 
	m_id{ id }, m_x{ x }, m_y{ y }, m_cooldown{ cooldown }, m_wood{ wood }, m_coal{ coal }, m_uranium{ uranium }, m_type{ type }, m_team{ team } {}

	const std::string &getId() const { return m_id; }
	int getX() const { return m_x; }
	int getY() const { return m_y; }
};

#endif