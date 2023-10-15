#include "Map.h"

#include "Log.h"

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

std::vector<tileindex_t> Map::getValidNeighbours(tileindex_t source) const
{
    static const std::vector<kit::DIRECTIONS> neighbourDirections = { kit::DIRECTIONS::NORTH, kit::DIRECTIONS::EAST, kit::DIRECTIONS::SOUTH, kit::DIRECTIONS::WEST }; // FIX should not be a method member, should be an array outside
    std::vector<tileindex_t> neighbours{};

    for (kit::DIRECTIONS direction : neighbourDirections) {
        if (!isValidNeighbour(source, direction)) continue;

        tileindex_t neighbourIndex = getTileNeighbour(source, direction);
        const Tile &tile = tileAt(neighbourIndex);
        if (tile.getType() != TileType::ENEMY_CITY) neighbours.push_back(neighbourIndex);
    }

    return neighbours;
}

bool Map::isValidNeighbour(tileindex_t source, kit::DIRECTIONS direction) const
{
    auto [x,y] = getTilePosition(source);

    switch (direction) {
    case kit::DIRECTIONS::EAST:  return x < m_width-1;
    case kit::DIRECTIONS::WEST:  return x > 0;
    case kit::DIRECTIONS::SOUTH: return y < m_height-1;
    case kit::DIRECTIONS::NORTH: return y > 0;
    default: throw std::runtime_error("Got unexpected direction " + std::to_string((int)direction));
    }
}

size_t Map::distanceBetween(tileindex_t t1, tileindex_t t2) const
{
    // manhattan distance
    auto [x1,y1] = getTilePosition(t1);
    auto [x2,y2] = getTilePosition(t2);
    return std::abs(x1-x2) + std::abs(y1-y2);
}

std::pair<int, int> Map::getTilePosition(tileindex_t tile) const
{
    return std::make_pair(tile % m_width, tile / m_width);
}
