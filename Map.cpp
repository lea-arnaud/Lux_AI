#include "Map.h"

#include "log.h"

kit::DIRECTIONS Map::getDirection(tileindex_t from, tileindex_t to) const
{
    auto [x1, y1] = getTilePosition(from);
    auto [x2, y2] = getTilePosition(to);
    if (x2 > x1) return kit::DIRECTIONS::EAST;
    if (x2 < x1) return kit::DIRECTIONS::WEST;
    if (y2 > y1) return kit::DIRECTIONS::SOUTH;
    if (y2 < y1) return kit::DIRECTIONS::NORTH;
    return kit::DIRECTIONS::CENTER;
}

tileindex_t Map::getTileNeighbour(tileindex_t source, kit::DIRECTIONS direction) const
{
    switch(direction) {
    case kit::DIRECTIONS::EAST:  return source + 1;
    case kit::DIRECTIONS::WEST:  return source - 1;
    case kit::DIRECTIONS::SOUTH: return source + m_width;
    case kit::DIRECTIONS::NORTH: return source - m_width;
    default: throw std::runtime_error("Got unexpected direction " + std::to_string((int)direction));
    }
}

std::pair<int, int> Map::getTilePosition(tileindex_t tile) const
{
    return std::make_pair(tile % m_width, tile / m_width);
}
