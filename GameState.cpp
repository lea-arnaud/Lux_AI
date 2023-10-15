#include "GameState.h"

std::optional<const Bot*> GameState::getEntityAt(int x, int y) const
{
  auto it = std::find_if(bots.begin(), bots.end(), [=](const Bot &bot) { return bot.getX() == x && bot.getY() == y; });
  return it == bots.end() ? std::optional<const Bot*>(&*it) : std::nullopt;
}
