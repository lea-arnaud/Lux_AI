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

std::vector<tileindex_t> Map::getValidNeighbours(tileindex_t source) const
{
    static const std::vector<kit::DIRECTIONS> neighbourDirections = { kit::DIRECTIONS::NORTH, kit::DIRECTIONS::EAST, kit::DIRECTIONS::SOUTH, kit::DIRECTIONS::WEST };
    std::vector<tileindex_t> neighbours{};

    for (kit::DIRECTIONS direction : neighbourDirections) {
        if (!isValidNeighbour(source, direction)) continue;

        if (tileindex_t neighbourIndex = getTileNeighbour(source, direction)) {
            Tile tile = tileAt(neighbourIndex);
            if (tile.getType() != TileType::ENEMY_CITY) neighbours.push_back(neighbourIndex);
        }
    }

    return neighbours;
}

bool Map::isValidNeighbour(tileindex_t source, kit::DIRECTIONS direction) const
{
    std::pair<int, int> position = getTilePosition(source);

    switch (direction) {
    case kit::DIRECTIONS::EAST:  return position.first < m_width-1;
    case kit::DIRECTIONS::WEST:  return position.first > 0;
    case kit::DIRECTIONS::SOUTH: return position.second < m_height-1;
    case kit::DIRECTIONS::NORTH: return position.second > 0;
    default: throw std::runtime_error("Got unexpected direction " + std::to_string((int)direction));
    }
}

std::pair<int, int> Map::getTilePosition(tileindex_t tile) const
{
    return std::make_pair(tile % m_width, tile / m_width);
}
