#ifndef COMMAND_CHAIN_H
#define COMMAND_CHAIN_H

#include "Bot.h"
#include "game_state.h"
#include "turn_order.h"
#include "BehaviorTreeNames.h"
#include "types.h"

class Squad
{
public:
  std::vector<Bot*> &getAgents() { return m_agents; }

private:
  std::vector<Bot*> m_agents;
};

class Commander
{
public:
  Commander();
  void updateHighLevelObjectives(GameState &state, const GameStateDiff &diff);
  std::vector<TurnOrder> getTurnOrders(const GameState &gameState);

private:
  std::vector<Squad> m_squads;
  std::shared_ptr<Blackboard> m_globalBlackboard;
};

#endif