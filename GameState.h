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

  std::optional<const Bot*> getEntityAt(tileindex_t tile) const { auto [x, y] = map.getTilePosition(tile); return getEntityAt(x, y); }
  std::optional<const Bot*> getEntityAt(int x, int y) const;
  void computeInfluence(const GameStateDiff &gameStateDiff);

  // For GOAP planner

  float priority_; // useful if this is a goal state, to distinguish from other possible goals
  std::string name_; // the human-readable name of the state
  std::map<int, bool> vars_; // the variables that in aggregate describe a worldstate

  void setVariable(const int var_id, const bool value);
  bool getVariable(const int var_id) const;
  bool meetsGoal(const GameState &goal_state) const;
  int distanceTo(const GameState &goal_state) const;
  bool operator==(const GameState &other) const;
};

#endif