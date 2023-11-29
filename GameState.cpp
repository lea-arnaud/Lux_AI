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
        citiesInfluence.addTemplateAtIndex(index, cityTemplate);
      citiesInfluence.setValueAtIndex(index, -100.0f);
    });

  std::ranges::for_each(resourcesIndex,
    [&](tileindex_t index) {
      // TODO: prendre en compte research point et le type de ressource
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

      resourcesInfluence.addTemplateAtIndex(index, resourceTemplate, resourceScale);
      citiesInfluence.setValueAtIndex(index, -100.0f);
    });
}

//GOAP

void GameState::setVariable(const int var_id, const bool value)
{
    vars[var_id] = value;
}

bool GameState::getVariable(const int var_id) const
{
    return vars.at(var_id);
}


bool GameState::operator==(const GameState &other) const
{
    return (vars == other.vars);
}

//Return true if the current GameState meets the requirement for the goal
bool GameState::meetsGoal(const GameState &goal_state) const
{
    for (const auto &kv : goal_state.vars) {
        try {
            if (vars.at(kv.first) != kv.second) {
                return false;
            }
        } catch (const std::out_of_range &) {
            return false;
        }
    }
    return true;
}

//Gives how many variables differ from the current state to an other
int GameState::distanceTo(const GameState &goal_state) const
{
    int result = 0;

    for (const auto &kv : goal_state.vars) {
        auto itr = vars.find(kv.first);
        if (itr == end(vars) || itr->second != kv.second) {
            ++result;
        }
    }

    return result;
}
