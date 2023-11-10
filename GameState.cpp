#include "GameState.h"
#include <algorithm>

std::optional<const Bot*> GameState::getEntityAt(int x, int y) const
{
  auto it = std::ranges::find_if(bots, [=](const std::unique_ptr<Bot> &bot) { return bot->getX() == x && bot->getY() == y; });
  return it != bots.end() ? std::optional<const Bot*>(it->get()) : std::nullopt;
}

void GameState::computeInfluence(const GameStateDiff &gameStateDiff)
{
  std::for_each(citiesBot.begin(), citiesBot.end(),
    [&](Bot bot) {
      int index = static_cast<int>(map.getTileIndex(bot.getX(), bot.getY()));
      if (bot.getTeam() == playerId)
        citiesInfluence.addTemplateAtIndex(index, cityTemplate);
      citiesInfluence.setValueAtIndex(index, -100.0f);
    });

  std::for_each(resourcesIndex.begin(), resourcesIndex.end(),
    [&](tileindex_t index) {
      // TODO: prendre en compte research point et le type de ressource
      resourcesInfluence.addTemplateAtIndex(static_cast<int>(index), ressourceTemplate);
      citiesInfluence.setValueAtIndex(static_cast<int>(index), -100.0f);
    });
}
