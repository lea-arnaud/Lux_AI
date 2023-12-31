#ifndef PATHING_H
#define PATHING_H

#include <vector>

#include "GameState.h"

namespace pathing
{

bool checkPathValidity(const std::vector<tileindex_t> &path, const Map &map, const std::vector<tileindex_t> &botPositions, size_t moveAheadCount);
tileindex_t getResourceFetchingLocation(const Bot* bot, const GameState *gameState, float distanceWeight=-1.f);
tileindex_t getBestCityBuildingLocation(const tileindex_t botTile, const GameState *gameState);
tileindex_t getBestExpansionLocation(const tileindex_t botTile, const GameState *gameState);
tileindex_t getBestCityFeedingLocation(const tileindex_t botTile, const GameState *gameState);
tileindex_t getBestBlockingPathLocation(const tileindex_t botTile, const GameState *gameState);
tileindex_t getBestNightTimeLocation(const tileindex_t botTile, const GameState *gameState, const std::vector<tileindex_t> &occupiedTiles);

std::vector<tileindex_t> getManyResourceFetchingLocations(const tileindex_t botTile, const GameState *gameState, int n);
std::vector<tileindex_t> getManyCityBuildingLocations(const tileindex_t botTile, const GameState *gameState, int n);
std::vector<tileindex_t> getManyExpansionLocations(const tileindex_t botTile, const GameState *gameState, int n);
std::vector<tileindex_t> getManyBlockingPathLocations(const tileindex_t botTile, const GameState *gameState, int n);
}

#endif