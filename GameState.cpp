#include "GameState.h"

std::optional<const Bot*> GameState::getEntityAt(int x, int y) const
{
  auto it = std::ranges::find_if(bots, [=](const std::unique_ptr<Bot> &bot) { return bot->getX() == x && bot->getY() == y; });
  return it != bots.end() ? std::optional<const Bot*>(it->get()) : std::nullopt;
}
