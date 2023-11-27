#include "TurnOrder.h"

#include "lux/annotate.hpp"
#include "lux/kit.hpp"

std::string TurnOrder::getAsString(const Map &map) const
{
  switch (type) {
  case TurnOrder::MOVE:          return kit::move(bot->getId(), map.getDirection(map.getTileIndex(*bot), targetTile));
  case TurnOrder::BUILD_CITY:    return kit::buildCity(bot->getId());
  case TurnOrder::RESEARCH:      return kit::research(bot->getX(), bot->getY());
  case TurnOrder::DO_NOTHING:    return "";
  case TurnOrder::CREATE_WORKER: return kit::buildWorker(bot->getX(), bot->getY());
  case TurnOrder::CREATE_CART:   return kit::buildCart(bot->getX(), bot->getY());
  default: throw std::runtime_error("Unimplemented turn order " + std::to_string(type));
  }
}

std::vector<std::string> lux::Annotate::s_logs;
