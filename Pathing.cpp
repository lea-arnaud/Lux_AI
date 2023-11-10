#include "Pathing.h"

#include <algorithm>
#include <limits>

#include "GameRules.h"
#include "Log.h"
#include "InfluenceMap.h"
#include "lux/annotate.hpp"

namespace pathing
{

// TODO move these constants in the functions that uses them, or into structures names after the function
// currently it is hard to know exactly what their purpose are
//static constexpr float ADJACENT_CITIES_WEIGHT = 10.0f;
static constexpr float ADJACENT_CITIES_WEIGHT = 0.0f; // TODO restore with better pathfinding
static constexpr float RESOURCE_NB_WEIGHT = 1.0f;
static constexpr float DISTANCE_WEIGHT = -1.0f;

bool checkPathValidity(const std::vector<tileindex_t> &path, const Map &map, const std::vector<tileindex_t> &botPositions, size_t moveAheadCount)
{
  for (size_t i = 0; i < std::min(moveAheadCount, path.size()); i++) {
    tileindex_t nextTileIdx = path[path.size() - i - 1];
    const Tile &nextTile = map.tileAt(nextTileIdx);
    if (nextTile.getType() == TileType::ENEMY_CITY)
      return false; // a new city has been built
    if (nextTile.getType() != TileType::ALLY_CITY) {
      if (std::ranges::find(botPositions, nextTileIdx) != botPositions.end())
        return false; // a bot is right in front
    }
  }
  return true;
}

tileindex_t getResourceFetchingLocation(const Bot *bot, const Map *map)
{
  const int neededResources = game_rules::WORKER_CARRY_CAPACITY - (bot->getCoalAmount() + bot->getWoodAmount() + bot->getUraniumAmount());

  tileindex_t bestTile = -1;
  float bestTileScore = std::numeric_limits<float>::lowest();
  for (tileindex_t i = 0; i < map->getMapSize(); i++)
  {
    if(map->tileAt(i).getType() == TileType::ALLY_CITY)
      continue;
    std::pair<int, int> coords = map->getTilePosition(i);
    size_t neighborResources = 0;
    std::vector<tileindex_t> neighbors = map->getValidNeighbours(i);
    for (tileindex_t j : neighbors)
    {
      if (map->tileAt(j).getType() == TileType::RESOURCE)
      {
        switch (map->tileAt(j).getResourceType())
        {
          // FIX consider research points
        case kit::ResourceType::wood: neighborResources += std::min(neededResources, std::min((int)game_rules::COLLECT_RATE_WOOD, map->tileAt(j).getResourceAmount())); break;
        case kit::ResourceType::coal: neighborResources += std::min(neededResources, std::min((int)game_rules::COLLECT_RATE_COAL, map->tileAt(j).getResourceAmount())); break;
        case kit::ResourceType::uranium: neighborResources += std::min(neededResources, std::min((int)game_rules::COLLECT_RATE_URANIUM, map->tileAt(j).getResourceAmount())); break;
        }
      }
    }
    float tileScore = 
      RESOURCE_NB_WEIGHT * neighborResources +
      DISTANCE_WEIGHT * (abs(bot->getX() - coords.first) + abs(bot->getY() - coords.second));
    if (bestTileScore < tileScore)
    {
      bestTile = i;
      bestTileScore = tileScore;
    }
  }
  return bestTile;
}

tileindex_t getBestCityBuildingLocation(const Bot *bot, const Map *map)
{
  tileindex_t bestTile = -1;
  float bestTileScore = std::numeric_limits<float>::lowest();
  for (tileindex_t i = 0; i < map->getMapSize(); i++)
  {
    if(map->tileAt(i).getType() != TileType::EMPTY) continue;
    std::pair<int, int> coords = map->getTilePosition(i);
    size_t neighborCities = 0;
    std::vector<tileindex_t> neighbors = map->getValidNeighbours(i);
    for (tileindex_t j : neighbors)
    {
      if (map->tileAt(j).getType() == TileType::ALLY_CITY)
        neighborCities++;
    }
    float tileScore = 
      ADJACENT_CITIES_WEIGHT * neighborCities +
      DISTANCE_WEIGHT * (abs(bot->getX() - coords.first) + abs(bot->getY() - coords.second));
    if (bestTileScore < tileScore)
    {
      bestTile = i;
      bestTileScore = tileScore;
    }
  }
  return bestTile;
}

tileindex_t getBestExpansionLocation(const Bot* bot, const Map* map)
{
#if 1
  InfluenceMap workingMap{ map->getCitiesMap() };
  //workingMap.addTemplateAtIndex(map->getTileIndex(*bot), agentTemplate);
  return static_cast<tileindex_t>(workingMap.getHighestPoint());
#else
  return getBestCityBuildingLocation(bot, map);
#endif
}

tileindex_t getBestCityFeedingLocation(const Bot* bot, const Map* map)
{
  tileindex_t bestTile = -1;
  float bestTileScore = std::numeric_limits<float>::lowest();
  for (tileindex_t i = 0; i < map->getMapSize(); i++) {
    if(map->tileAt(i).getType() != TileType::ALLY_CITY) continue;
    float tileScore = DISTANCE_WEIGHT * map->distanceBetween(i, map->getTileIndex(*bot));
    if (bestTileScore < tileScore) {
      bestTile = i;
      bestTileScore = tileScore;
    }
  }
  return bestTile;
}

tileindex_t getBestNightTimeLocation(const Bot *bot, const Map *map, const std::vector<tileindex_t> &occupiedTiles)
{
  /*
   * At night, agents try to reach the nearest city to avoid dying
   * - city tiles with adjacent resources are better since they will
   *   collect resources if at least one agent is in them.
   * - it is better to spread agents, as more city tiles will be able
   *   to collect at the same time
   * - resource tiles are also valid, but are targeted only if there
   *   is no good city to go to
   */

  constexpr float distanceWeight = -.2f;
  constexpr float isCityFactor = +.5f;
  constexpr float unreachableFactor = -1000.f;
  constexpr float hasAdjacentResourcesFactor = +5.f;
  constexpr float isTileOccupiedFactor = -8.f;

  tileindex_t botTile = map->getTileIndex(*bot);
  tileindex_t bestTile = botTile;
  float bestScore = std::numeric_limits<float>::lowest();
  for (tileindex_t i = 0; i < map->getMapSize(); i++) {
    bool hasAdjacentResources = map->tileAt(i).getType() == TileType::RESOURCE || 
      std::ranges::any_of(map->getValidNeighbours(i), [&](tileindex_t n) { return map->tileAt(n).getType() == TileType::RESOURCE; });
    bool isCity = map->tileAt(i).getType() == TileType::ALLY_CITY;
    if (!isCity && !hasAdjacentResources) continue;
    size_t dist = map->distanceBetween(i, botTile);
    bool isTileOccupied = std::ranges::count(occupiedTiles, i) - (i == botTile) > 0;
    float tileScore = 0
      + isCityFactor * isCity
      + distanceWeight * (float)dist
      + unreachableFactor * (dist > game_rules::NIGHT_DURATION)
      + hasAdjacentResourcesFactor * hasAdjacentResources
      + isTileOccupiedFactor * isTileOccupied;

    if (tileScore > bestScore) {
      bestScore = tileScore;
      bestTile = i;
    }
  }

  return bestTile;
}

}
