#include "Bot.h"

#include "game_state.h"
#include "turn_order.h"
#include "BehaviorTree.h"
#include "BehaviorTreeNames.h"

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
  explicit SquadAgent(const Bot *bot, const std::shared_ptr<Blackboard> &blackboard, const std::shared_ptr<BasicBehavior> &behaviorTree)
    : m_bot(bot)
    , m_blackboard(blackboard)
    , m_behaviorTree(behaviorTree)
  {}

  const Bot &getBot() const { return *m_bot; }
  Blackboard &getBlackboard() { return *m_blackboard; }
  void act() { m_behaviorTree->run(m_blackboard); }

private:
  const Bot                     *m_bot;
  std::shared_ptr<Blackboard>    m_blackboard;
  std::shared_ptr<BasicBehavior> m_behaviorTree;
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
  Blackboard m_globalBlackboard;
};

