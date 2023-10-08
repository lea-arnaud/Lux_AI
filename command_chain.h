#include "Bot.h"

#include "game_state.h"
#include "turn_order.h"

enum CompletionState
{
  DONE,
  DOING,
  FAILED,
};

struct BotGoal
{
  enum
  {
    GO_TO,
    PICK_RESOURCE,
    BUILD_CITY,
    MAKE_WORKER,
    MAKE_CART,
  } type;

  union
  {
    tileindex_t targetTile;
  };
};

class SquadAgent
{
public:
  SquadAgent(const Bot *bot) : m_bot(bot) {}

  CompletionState getGoalState();
  const Bot &getBot() const { return *m_bot; }

private:
  const Bot *m_bot;
  BotGoal    m_goal;
};

struct SquadObjective
{
  enum
  {
    CONQUER_RESOURCE_SPOT
  } type;

  union
  {
    // TODO
  };
};

class Squad
{
public:
  CompletionState getObjectiveState();

  std::vector<SquadAgent> &getAgents() { return m_agents; }

private:
  std::vector<SquadAgent> m_agents;
  std::unique_ptr<SquadObjective> m_objective;
};

class Commander
{
public:
  Commander();
  void updateHighLevelObjectives(const GameState &state, const GameStateDiff &diff);
  std::vector<TurnOrder> getTurnOrders(const Map &map);

private:
  std::vector<Squad> m_squads;
};

