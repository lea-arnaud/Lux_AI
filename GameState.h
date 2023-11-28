#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <optional>

#include "Map.h"
#include "City.h"
#include "Bot.h"
#include "InfluenceMap.h"

struct GameStateDiff
{
  std::vector<std::unique_ptr<Bot>> deadBots;
  std::vector<Bot *> newBots;
  std::vector<tileindex_t> updatedRoads;
};

struct GameState
{
  size_t currentTurn = 0;

  Map map;
  std::vector<std::unique_ptr<City>> cities;
  std::vector<std::unique_ptr<Bot>> bots;

  // used to update influence maps
  std::vector<tileindex_t> resourcesIndex;
  std::vector<Bot*> citiesBot;

  size_t playerResearchPoints[2];

  InfluenceMap citiesInfluence;
  InfluenceMap resourcesInfluence;
  std::unordered_map<std::string, InfluenceMap> ennemyPath;

  // Used to choose if we can have more city or not
  float resourcesRemaining, resourcesNeeded, resourcesOwned;

  std::optional<const Bot*> getEntityAt(tileindex_t tile) const { auto [x, y] = map.getTilePosition(tile); return getEntityAt(x, y); }
  std::optional<const Bot*> getEntityAt(int x, int y) const;
  void computeInfluence(const GameStateDiff &gameStateDiff);
  bool shouldExpand();
};

#endif