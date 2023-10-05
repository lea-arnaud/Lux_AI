#pragma once

#include "Map.h"
#include "City.h"
#include "Bot.h"

struct GameState
{
  Map map;
  std::vector<City> cities;
  std::vector<Bot> bots;
};