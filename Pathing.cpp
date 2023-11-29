#include "Pathing.h"

#include <algorithm>
#include <limits>

#include "Benchmarking.h"
#include "GameRules.h"
#include "Log.h"
#include "InfluenceMap.h"
#include "lux/annotate.hpp"

namespace pathing
{

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

tileindex_t getResourceFetchingLocation(const Bot* bot, const GameState *gameState, float distanceWeight)
{
  static constexpr float RESOURCE_NB_WEIGHT = +1.f;

  const int neededResources = game_rules::WORKER_CARRY_CAPACITY - (bot->getCoalAmount() + bot->getWoodAmount() + bot->getUraniumAmount());

  const Map *map = &gameState->map;
  size_t currentResearchPoints = gameState->playerResearchPoints[Player::ALLY];

  tileindex_t botTile = map->getTileIndex(*bot);

  tileindex_t bestTile = -1;
  float bestTileScore = std::numeric_limits<float>::lowest();
  for (tileindex_t i = 0; i < map->getMapSize(); i++) {
    if(map->tileAt(i).getType() == TileType::ALLY_CITY || map->tileAt(i).getType() == TileType::ENEMY_CITY)
      continue;
    int neighborResources = 0;
    std::vector<tileindex_t> neighbors = map->getValidNeighbours(i, PathFlags::NONE);
    for (tileindex_t j : neighbors) {
      if (map->tileAt(j).getType() == TileType::RESOURCE) {
        switch (map->tileAt(j).getResourceType()) {
        case kit::ResourceType::wood:
          neighborResources += std::min((int)game_rules::COLLECT_RATE_WOOD, map->tileAt(j).getResourceAmount());
          break;
        case kit::ResourceType::coal:
          if(currentResearchPoints >= game_rules::MIN_RESEARCH_COAL)
            neighborResources += std::min((int)game_rules::COLLECT_RATE_COAL, map->tileAt(j).getResourceAmount());
          break;
        case kit::ResourceType::uranium:
          if(currentResearchPoints >= game_rules::MIN_RESEARCH_URANIUM)
            neighborResources += std::min((int)game_rules::COLLECT_RATE_URANIUM, map->tileAt(j).getResourceAmount());
          break;
        }
      }
    }
    if (neighborResources == 0)
      continue;
    float tileScore = 
      RESOURCE_NB_WEIGHT * std::min(neededResources, neighborResources) +
      distanceWeight * map->distanceBetween(botTile, i);
    if (bestTileScore < tileScore) {
      bestTile = i;
      bestTileScore = tileScore;
    }
  }
  return bestTile;
}

tileindex_t getBestCityBuildingLocation(const tileindex_t botTile, const GameState *gameState)
{
  MULTIBENCHMARK_LAPBEGIN(getBestCityBuildingLocation);
  static constexpr float DISTANCE_WEIGHT = -1.f;
  static constexpr float ADJACENT_CITIES_WEIGHT = +1.f;

  tileindex_t bestTile = -1;
  float bestTileScore = std::numeric_limits<float>::lowest();
  const Map *map = &gameState->map;
  for (tileindex_t i = 0; i < map->getMapSize(); i++)
  {
    if(map->tileAt(i).getType() != TileType::EMPTY) continue;
    std::pair<int, int> coords = map->getTilePosition(i);
    size_t neighborCities = 0;
    std::vector<tileindex_t> neighbors = map->getValidNeighbours(i, PathFlags::NONE);
    for (tileindex_t j : neighbors)
    {
      if (map->tileAt(j).getType() == TileType::ALLY_CITY)
        neighborCities++;
    }
    auto botCoords = gameState->map.getTilePosition(botTile);
    float tileScore = 
      ADJACENT_CITIES_WEIGHT * neighborCities +
      DISTANCE_WEIGHT * (abs(botCoords.first - coords.first) + abs(botCoords.second - coords.second));
    if (bestTileScore < tileScore)
    {
      bestTile = i;
      bestTileScore = tileScore;
    }
  }
  MULTIBENCHMARK_LAPEND(getBestCityBuildingLocation);
  return bestTile;
}

tileindex_t getBestExpansionLocation(const tileindex_t botTile, const GameState *gameState)
{
  InfluenceMap workingMap{ gameState->citiesInfluence };
  workingMap.addTemplateAtIndex(botTile, influence_templates::AGENT_PROXIMITY);
  return static_cast<tileindex_t>(workingMap.getHighestPoint());
}

tileindex_t getBestCityFeedingLocation(const tileindex_t botTile, const GameState *gameState)
{
  MULTIBENCHMARK_LAPBEGIN(getBestCityFeedingLocation);
  static constexpr float DISTANCE_WEIGHT = -1.f;

  tileindex_t bestTile = -1;
  float bestTileScore = std::numeric_limits<float>::lowest();
  const Map *map = &gameState->map;
  for (tileindex_t i = 0; i < map->getMapSize(); i++) {
    if(map->tileAt(i).getType() != TileType::ALLY_CITY) continue;
    float tileScore = DISTANCE_WEIGHT * map->distanceBetween(i, botTile);
    if (bestTileScore < tileScore) {
      bestTile = i;
      bestTileScore = tileScore;
    }
  }
  MULTIBENCHMARK_LAPEND(getBestCityFeedingLocation);
  return bestTile;
}

tileindex_t getBestBlockingPathLocation(const tileindex_t botTile, const GameState* gameState)
{
  // TODO: check if the highestpoint can be ennemy city
  InfluenceMap workingMap{ gameState->map.getWidth(), gameState->map.getHeight() };

  for (auto const& [_, val] : gameState->ennemyPath)
    workingMap.addMap(val);

  workingMap.addTemplateAtIndex(botTile, influence_templates::AGENT_PROXIMITY);

  return workingMap.getHighestPoint();
}

tileindex_t getBestNightTimeLocation(const tileindex_t botTile, const GameState *gameState, const std::vector<tileindex_t> &occupiedTiles)
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

  const Map *map = &gameState->map;

  tileindex_t bestTile = botTile;
  float bestScore = std::numeric_limits<float>::lowest();
  for (tileindex_t i = 0; i < map->getMapSize(); i++) {
    bool hasAdjacentResources = map->hasAdjacentResources(i);
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

std::vector<tileindex_t> getManyResourceFetchingLocations(const tileindex_t botTile, const GameState *gameState, int n)
{
  static constexpr float DISTANCE_WEIGHT = 1.0f;

  InfluenceMap workingMap{ gameState->resourcesInfluence };
  workingMap.addTemplateAtIndex(botTile, influence_templates::AGENT_PROXIMITY, DISTANCE_WEIGHT);
  workingMap.addTemplateAtIndex(workingMap.getHighestPoint(), influence_templates::ENEMY_CITY_CLUSTER_PROXIMITY, 2.0f);

  return workingMap.getNHighestPoints(n);
}

std::vector<tileindex_t> getManyCityBuildingLocations(const tileindex_t botTile, const GameState *gameState, int n)
{
    static constexpr float DISTANCE_WEIGHT = -1.f;
    static constexpr float ADJACENT_CITIES_WEIGHT = -0.5f;
    static constexpr float ADJACENT_RESOURCES_WEIGHT = +1.f;

    std::vector<std::pair<tileindex_t, float>> tiles{};
    const Map *map = &gameState->map;
    for (tileindex_t i = 0; i < map->getMapSize(); i++) {
        if (map->tileAt(i).getType() != TileType::EMPTY) continue;
        std::pair<int, int> coords = map->getTilePosition(i);
        size_t neighborCities = 0;
        float resourceScore = 0.f;
        std::vector<tileindex_t> neighbors = map->getValidNeighbours(i, 0);
        for (tileindex_t j : neighbors) {
            if (map->tileAt(j).getType() == TileType::ALLY_CITY)
                neighborCities++;
            if (map->tileAt(j).getType() == TileType::RESOURCE)
            {
                kit::ResourceType rt = map->tileAt(j).getResourceType();
                float rs = static_cast<float>(map->tileAt(j).getResourceAmount());
                switch (rt) {
                case kit::ResourceType::wood:
                    rs *= game_rules::FUEL_VALUE_WOOD;
                    break;
                case kit::ResourceType::coal:
                    rs *= gameState->playerResearchPoints[Player::ALLY] > 50 ? game_rules::FUEL_VALUE_COAL : 0;
                    break;
                case kit::ResourceType::uranium:
                    rs *= gameState->playerResearchPoints[Player::ALLY] > 200 ? game_rules::FUEL_VALUE_URANIUM : 0;
                    break;
                default:
                    break;
                }
                resourceScore += rs;
            }
        }
        auto botCoords = gameState->map.getTilePosition(botTile);
        float tileScore =
            ADJACENT_CITIES_WEIGHT * neighborCities +
            DISTANCE_WEIGHT * (abs(botCoords.first - coords.first) + abs(botCoords.second - coords.second)) +
            ADJACENT_RESOURCES_WEIGHT * resourceScore;
        tiles.push_back(std::pair<tileindex_t, float>(i, tileScore));
    }
    std::ranges::sort(tiles, [](std::pair<tileindex_t, float> p1, std::pair<tileindex_t, float> p2) {return p1.second > p2.second; });
    std::vector<tileindex_t> bestTiles{};
    for (int i = 0; i < n; i++)
    {
        bestTiles.push_back(tiles[i].first);
    }
    return bestTiles;
}

std::vector<tileindex_t> getManyExpansionLocations(const tileindex_t botTile, const GameState *gameState, int n)
{
    static constexpr float DISTANCE_WEIGHT = 3.0f;

    InfluenceMap workingMap{ gameState->citiesInfluence };
    workingMap.addTemplateAtIndex(botTile, influence_templates::AGENT_PROXIMITY, DISTANCE_WEIGHT);
    workingMap.addTemplateAtIndex(workingMap.getHighestPoint(), influence_templates::RESOURCE_PROXIMITY, 2.0f);

    return workingMap.getNHighestPoints(n);
}

std::vector<tileindex_t> getManyBlockingPathLocations(const tileindex_t botTile, const GameState* gameState, int n)
{
  // TODO: check if the highestpoint can be ennemy city
  InfluenceMap workingMap{ gameState->map.getWidth(), gameState->map.getHeight() };

  for (auto const& [_, val] : gameState->ennemyPath)
    workingMap.addMap(val);

  workingMap.addTemplateAtIndex(botTile, influence_templates::AGENT_PROXIMITY);

  return workingMap.getNHighestPoints(n);
}

}
