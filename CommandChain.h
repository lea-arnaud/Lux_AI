#ifndef COMMAND_CHAIN_H
#define COMMAND_CHAIN_H

#include "Bot.h"
#include "GameState.h"
#include "TurnOrder.h"
#include "BehaviorTreeNames.h"
#include "Types.h"

struct BotObjective
{
  enum class ObjectiveType
  {
    GO_BLOCK_PATH,
    BUILD_CITY,
    FEED_CITY,
  } type;

  tileindex_t targetTile;
};

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
  std::vector<TurnOrder> getTurnOrders(GameState &gameState);

private:
  std::vector<Squad> m_squads;
  std::shared_ptr<Blackboard> m_globalBlackboard;
};

#endif