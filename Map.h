#ifndef MAP_H
#define MAP_H

#include <vector>
#include <utility>

#include "lux/kit.hpp"
#include "Tile.h"
#include "Bot.h"

using tileindex_t = uint16_t;

class Map
{
private:
	std::vector<Tile> m_tiles;
	int m_width, m_height;

public:
	Map() = default;

	tileindex_t getTileIndex(int x, int y) const { return x + y * m_height; }
	tileindex_t getTileIndex(const Bot &bot) const { return getTileIndex(bot.getX(), bot.getY()); }
	std::pair<int, int> getTilePosition(tileindex_t tile) const;
	kit::DIRECTIONS getDirection(tileindex_t from, tileindex_t to) const;
};

#endif