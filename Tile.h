#ifndef TILE_H
#define TILE_H

#include "lux/kit.hpp"

enum class TileType
{
  EMPTY,
  RESOURCE,
  ALLY_CITY,
  ENEMY_CITY,
};

class Tile
{
	float m_road = 0;
	kit::ResourceType m_resource{};
	TileType m_type = TileType::EMPTY;
	int m_resourceNb = 0;

public:
	static const float MAX_ROAD;

	void setType(TileType type, kit::ResourceType resource = kit::ResourceType::coal) { m_type = type; m_resource = resource; }
	float getRoadAmount() const { return m_road; }
	void setRoadAmount(float amount) { m_road = amount; }
	void setResourceAmount(int amount) { m_resourceNb = amount; }
	int getResourceAmount() const { return m_resourceNb; }
	TileType getType() const { return m_type; };
	kit::ResourceType getResourceType() const { return m_resource; }
};

#endif