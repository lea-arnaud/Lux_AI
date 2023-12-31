#ifndef MAP_H
#define MAP_H

#include <vector>
#include <utility>

#include "lux/kit.hpp"
#include "Tile.h"
#include "Bot.h"
#include "InfluenceMap.h"

using pathflags_t = uint8_t;

namespace PathFlags
{
static constexpr pathflags_t
  NONE = 0,
  CANNOT_MOVE_THROUGH_FRIENDLY_CITIES = 1 << 0,
  MUST_BE_NIGHT_SURVIVABLE_TILE    = 1 << 1;
}

class Map
{
private:
	std::vector<Tile> m_tiles;
	int m_width{}, m_height{};
	InfluenceMap m_resourcesAdjencies;

public:
	Map() = default;

	void setSize(int width, int height)
	{
		m_width = width;
		m_height = height;
		m_tiles.resize(width * height);
	}

	void rebuildResourceAdjencies();

	int getWidth() const { return m_width; }
	int getHeight() const { return m_height; }
	size_t getMapSize() const { return m_tiles.size(); }
	bool hasAdjacentResources(tileindex_t index) const { return m_resourcesAdjencies.getValue(index) > 0; }
	tileindex_t getTileIndex(int x, int y) const { return x + y * m_width; }
	tileindex_t getTileIndex(const Bot &bot) const { return getTileIndex(bot.getX(), bot.getY()); }
	Tile &tileAt(tileindex_t index) { return m_tiles[index]; }
	Tile &tileAt(int x, int y) { return tileAt(getTileIndex(x, y)); }
	const Tile &tileAt(tileindex_t index) const { return m_tiles[index]; }
	const Tile &tileAt(int x, int y) const { return tileAt(getTileIndex(x, y)); }
	bool isValidTileIndex(tileindex_t index) const { return index <= m_width*m_height; }
	size_t distanceBetween(tileindex_t t1, tileindex_t t2) const;
	std::pair<int, int> getTilePosition(tileindex_t tile) const;
	kit::DIRECTIONS getDirection(tileindex_t from, tileindex_t to) const;
	tileindex_t getTileNeighbour(tileindex_t source, kit::DIRECTIONS direction) const;
	std::vector<tileindex_t> getValidNeighbours(tileindex_t source, pathflags_t flags) const;
	bool isValidNeighbour(tileindex_t source, kit::DIRECTIONS direction) const;


};

#endif