#include "GameState.h"
#include <algorithm>
#include "GameRules.h"

std::optional<const Bot*> GameState::getEntityAt(int x, int y) const
{
  auto it = std::ranges::find_if(bots, [=](const std::unique_ptr<Bot> &bot) { return bot->getX() == x && bot->getY() == y; });
  return it != bots.end() ? std::optional<const Bot*>(it->get()) : std::nullopt;
}

void GameState::computeInfluence(const GameStateDiff &gameStateDiff)
{
  std::ranges::for_each(citiesBot,
    [&](const Bot *bot) {
      tileindex_t index = map.getTileIndex(bot->getX(), bot->getY());
      if (bot->getTeam() == Player::ALLY)
        citiesInfluence.addTemplateAtIndex(index, influence_templates::CITY_ADJENCY);
      citiesInfluence.setValueAtIndex(index, -100.0f);
    });

  std::ranges::for_each(resourcesIndex,
    [&](tileindex_t index) {
      float resourceScale = 0.0f;
      Tile tile = map.tileAt(index);
      
      switch (tile.getResourceType()) {
      case kit::ResourceType::wood:
        if (playerResearchPoints[Player::ALLY] < game_rules::MIN_RESEARCH_WOOD) break;
        resourceScale = static_cast<float>(tile.getResourceAmount() * game_rules::COLLECT_RATE_WOOD * game_rules::FUEL_VALUE_WOOD);
        break;
      case kit::ResourceType::coal:
        if (playerResearchPoints[Player::ALLY] < game_rules::MIN_RESEARCH_COAL) break;
        resourceScale = static_cast<float>(tile.getResourceAmount() * game_rules::COLLECT_RATE_COAL * game_rules::FUEL_VALUE_COAL);
        break;
      case kit::ResourceType::uranium:
        if (playerResearchPoints[Player::ALLY] < game_rules::MIN_RESEARCH_URANIUM) break;
        resourceScale = static_cast<float>(tile.getResourceAmount() * game_rules::COLLECT_RATE_URANIUM * game_rules::FUEL_VALUE_URANIUM);
        break;
      }

      if (resourceScale != 0) {
        resourcesInfluence.addTemplateAtIndex(index, influence_templates::RESOURCE_PROXIMITY, resourceScale);
      }
      citiesInfluence.setValueAtIndex(index, -100.0f);
    });
}

bool GameState::shouldExpand()
{
  // TODO: should take the agent into consideration and the ennemy
  constexpr float TOTAL_RESOURCES_PERCENT = 0.75;

  // calculate the resources needed to survive till the end
  size_t nightDuration = (360 - currentTurn) / 4; // 1/4 tu temps est la nuit
  float resourceNeededToSurvive = resourcesNeeded * nightDuration;

  if (resourceNeededToSurvive > TOTAL_RESOURCES_PERCENT * (resourcesRemaining + resourcesOwned)) 
    return false;

  return true;
}
