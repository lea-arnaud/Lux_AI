#ifndef CITY_H
#define CITY_H

class City
{
	int m_id;
	int m_team;
	float m_fuel;
	float m_lightUpkeep;

public:
	City(int id, int team, float fuel, float lightUpkeep) : m_id{ id }, m_team{ team }, m_fuel{ fuel }, m_lightUpkeep{ lightUpkeep } {}
};

#endif