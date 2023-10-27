#ifndef PATHING_H
#define PATHING_H

#include "GameState.h"

namespace pathing
{

bool checkPathValidity(const std::vector<tileindex_t> &path, const GameState &gameState, size_t moveAheadCount);
tileindex_t getResourceFetchingLocation(const Bot *bot, const Map *map);
tileindex_t getBestCityBuildingLocation(const Bot *bot, Map *map);
tileindex_t getBestNightTimeLocation(const Bot *bot, const Map *map);

}

#endif