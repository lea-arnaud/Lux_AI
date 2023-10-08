#pragma once

#include "Map.h"
#include "City.h"
#include "Bot.h"

struct GameState
{
  Map map;
  std::vector<City> cities;
  std::vector<Bot> bots;
  size_t playerResearchPoints[2];
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