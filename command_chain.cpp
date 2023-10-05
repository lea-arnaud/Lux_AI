#include "command_chain.h"

void Commander::updateHighLevelObjectives(const GameState &state)
{
}

std::vector<TurnOrder> Commander::getTurnOrders()
{
  std::vector<SquadBot*> bots;
  for (Squad &squad : m_squads)
    for (SquadBot &bot : squad.getBots())
      bots.push_back(&bot);

  // TODO collect agents that should pathfind here
  // TODO do pathfinding here

  std::vector<TurnOrder> orders;
  // TODO collect bots individual orders here

  return orders;
}

