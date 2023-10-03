#ifndef MAP_H
#define MAP_H

#include <vector>
#include "Tile.h"

class Map
{
	std::vector<Tile> m_tiles;
	int m_width, m_height;

public:
	Map() = default;
};

#endif