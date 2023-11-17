#ifndef TURN_ORDER_H
#define TURN_ORDER_H

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
    CREATE_WORKER,
    CREATE_CART,
    DO_NOTHING, // a do-nothing order may as well not be generated
    COLLECT_RESOURCES = DO_NOTHING,
  } type;

  const Bot *bot;

  union
  {
    // only active when order is MOVE
    struct
    {
      tileindex_t targetTile;
    };
  };

  std::string getAsString(const Map &map) const;
};

#endif