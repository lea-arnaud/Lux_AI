#ifndef CITY_H
#define CITY_H

#include <string>

#include "Types.h"

class City
{
	std::string m_id;
	player_t m_team;
	float m_fuel;
	float m_lightUpkeep;

public:
	City(const std::string &id, player_t team, float fuel, float lightUpkeep) 
	  : m_id{ id }, m_team{ team }, m_fuel{ fuel }, m_lightUpkeep{ lightUpkeep } {}

	const std::string &getId() const { return m_id; }
};

#endif