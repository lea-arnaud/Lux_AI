#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <optional>

#include "Map.h"
#include "City.h"
#include "Bot.h"
#include "InfluenceMap.h"

struct GameStateDiff
{
  std::vector<Bot> deadBots;
  //std::vector<tileindex_t> exhaustedResourceTiles; // to be implemented (when needed)
  //std::vector<tileindex_t> dead
  // Tiles;

  std::vector<Bot *> newBots;
  //std::vector<tileindex_t> newCityTiles;

  std::vector<tileindex_t> updatedRoads;
};

struct GameState
{
  int playerId;

  Map map;
  std::vector<City> cities;
  std::vector<Bot> bots;

  // used to update influence maps
  std::vector<tileindex_t> resourcesIndex;
  std::vector<Bot> citiesBot;

  size_t playerResearchPoints[2];

  InfluenceMap citiesInfluence;
  InfluenceMap resourcesInfluence;
  std::unordered_map<std::string, InfluenceMap> ennemyPath;

  std::optional<const Bot*> getEntityAt(tileindex_t tile) const { auto [x, y] = map.getTilePosition(tile); return getEntityAt(x, y); }
  std::optional<const Bot*> getEntityAt(int x, int y) const;
  void computeInfluence(const GameStateDiff &gameStateDiff);
};

#endif