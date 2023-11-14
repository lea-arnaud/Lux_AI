#ifndef PATHING_H
#define PATHING_H

#include "GameState.h"
#include <vector>

namespace pathing
{

bool checkPathValidity(const std::vector<tileindex_t> &path, const Map &map, const std::vector<tileindex_t> &botPositions, size_t moveAheadCount);
tileindex_t getResourceFetchingLocation(const Bot *bot, const GameState *gameState);
tileindex_t getBestCityBuildingLocation(const Bot *bot, const GameState *gameState);
tileindex_t getBestExpansionLocation(const Bot *bot, const GameState *gameState);
tileindex_t getBestCityFeedingLocation(const Bot* bot, const GameState *gameState);
tileindex_t getBestNightTimeLocation(const Bot *bot, const GameState *gameState, const std::vector<tileindex_t> &occupiedTiles);

std::vector<tileindex_t> getResourceFetchingLocation2(const Bot *bot, const GameState *gameState, int n);

}

#endif