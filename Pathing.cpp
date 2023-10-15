#include "Pathing.h"

#include "GameRules.h"

namespace pathing
{

//static constexpr float ADJACENT_CITIES_WEIGHT = 10.0f;
static constexpr float ADJACENT_CITIES_WEIGHT = 0.0f; // TODO restore with better pathfinding
static constexpr float RESOURCE_NB_WEIGHT = 1.0f;
static constexpr float DISTANCE_WEIGHT = -1.0f;

bool checkPathValidity(const std::vector<tileindex_t> &path, const GameState &gameState, size_t moveAheadCount)
{
  for (size_t i = 0; i < std::min(moveAheadCount, path.size()); i++) {
    if (gameState.map.tileAt(path[i]).getType() == TileType::ENEMY_CITY)
      return false; // a new city has been built
    if (gameState.map.tileAt(path[i]).getType() != TileType::ALLY_CITY) {
      std::optional<const Bot*> blockingBot = gameState.getEntityAt(path[i]);
      if (blockingBot.has_value() && (i == 0 || blockingBot.value()->getTeam() == Player::ENEMY))
        return false; // a friendly bot is right in front, or an enemy bot stepped on our path
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

tileindex_t getBestNightTimeLocation(const Bot *bot, const Map *map)
{
  // currently only considering cities
  // it would be better to give resource clusters a score too
  tileindex_t bestTile = -1;
  tileindex_t botTile = map->getTileIndex(*bot);
  size_t minDistance = std::numeric_limits<size_t>::max();
  for (tileindex_t i = 0; i < map->getMapSize(); i++) {
    if (map->tileAt(i).getType() != TileType::ALLY_CITY)
      continue;
    size_t dist = map->distanceBetween(i, botTile);
    if (dist < minDistance) {
      minDistance = dist;
      bestTile = i;
    }
  }
  return bestTile;
}

}