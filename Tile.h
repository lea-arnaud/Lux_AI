#ifndef TILE_H
#define TILE_H

#include "lux/kit.hpp"

class Tile
{
	float m_road;
	kit::ResourceType m_resource;
	int m_resourceNb;

public:
	Tile(float road, kit::ResourceType resource, int resourceNb) : m_road{ road }, m_resource{ resource }, m_resourceNb{ resourceNb } {}
};

#endif