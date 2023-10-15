#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <optional>

#include "Map.h"
#include "City.h"
#include "Bot.h"

struct GameState
{
  Map map;
  std::vector<City> cities;
  std::vector<Bot> bots;
  size_t playerResearchPoints[2];

  std::optional<const Bot*> getEntityAt(tileindex_t tile) const { auto [x, y] = map.getTilePosition(tile); return getEntityAt(x, y); }
  std::optional<const Bot*> getEntityAt(int x, int y) const; // TODO add a time frame, to get prioritized planning to work
};

struct GameStateDiff
{
  std::vector<Bot> deadBots;
  //std::vector<tileindex_t> exhaustedResourceTiles; // to be implemented (when needed)
  //std::vector<tileindex_t> deadCityTiles;

  std::vector<Bot*> newBots;
  //std::vector<tileindex_t> newCityTiles;

  std::vector<tileindex_t> updatedRoads;
};

#endif