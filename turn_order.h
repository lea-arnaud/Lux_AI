#pragma once

#include <string>

#include "Map.h"
#include "Bot.h"

struct TurnOrder
{
  enum
  {
    MOVE,
    BUILD_CITY,
    RESEARCH,
    CREATE_BOT,
    CREATE_CART,
    DO_NOTHING, // a do-nothing order may as well not be generated
  } type;

  Bot *bot;

  union
  {
    tileindex_t targetTile;
  };

  std::string getAsString(const Map &map);
};