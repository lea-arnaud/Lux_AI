#include "Map.h"

#include "log.h"

kit::DIRECTIONS Map::getDirection(tileindex_t from, tileindex_t to) const
{
  auto [x1, y1] = getTilePosition(from);
  auto [x2, y2] = getTilePosition(to);
  if (x2 == x1+1) return kit::DIRECTIONS::EAST;
  if (x2 == x1-1) return kit::DIRECTIONS::WEST;
  if (y2 == y1+1) return kit::DIRECTIONS::EAST;
  if (y2 == y1-1) return kit::DIRECTIONS::WEST;
  LOG("Invalid direction from tile " << getTilePosition(from) << " to " << getTilePosition(to));
  return kit::DIRECTIONS::CENTER;
}

std::pair<int, int> Map::getTilePosition(tileindex_t tile) const
{
  return std::make_pair(tile % m_width, tile / m_width);
}
