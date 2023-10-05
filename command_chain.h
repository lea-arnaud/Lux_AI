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

class SquadBot
{
public:
  CompletionState getGoalState();

private:
  Bot    *m_bot;
  BotGoal m_goal;
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

  std::vector<SquadBot> &getBots() { return m_bots; }

private:
  std::vector<SquadBot> m_bots;
  std::unique_ptr<SquadObjective> m_objective;
};

class Commander
{
public:
  void updateHighLevelObjectives(const GameState &state);
  std::vector<TurnOrder> getTurnOrders();

private:
  std::vector<Squad> m_squads;
};

