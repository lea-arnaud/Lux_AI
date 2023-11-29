#ifndef BEHAVIORTREENODES_H
#define BEHAVIORTREENODES_H

#include <vector>

#include "BehaviorTree.h"
#include "BehaviorTreeNames.h"
#include "GameRules.h"
#include "Types.h"
#include "TurnOrder.h"
#include "Log.h"
#include "AStar.h"
#include "GameState.h"

namespace nodes
{

// common tests (a stateless, consequenceless task that returns FAIL/SUCCESS)
std::shared_ptr<Task> testIsNight();
std::shared_ptr<Task> testIsDawnOrNight();
std::shared_ptr<Task> testIsAgentFullOfResources();
std::shared_ptr<Task> testHasTeamEnoughResearchPoint();

// debugging method, prints and returns success
std::shared_ptr<Task> taskLog(const std::string &text, TaskResult result = TaskResult::SUCCESS);
// utility method, adapts a function to a task
std::shared_ptr<Task> taskPlayAgentTurn(std::function<TurnOrder(Bot *bot)> &&orderSupplier);
// utility method, adapts a function to a task
std::shared_ptr<Task> taskPlayAgentTurn(std::function<TurnOrder(Blackboard &bb)> &&orderSupplier);
// a supplier that can be passed to taskMoveTo, that supplies the goal set to the
// agent by its squad into the agent's blackboard "AGENT_OBJECTIVE" entry
std::function<tileindex_t(Blackboard &bb)> goalSupplierFromAgentObjective();

// types used by taskMoveTo to create complex pathfinding behaviors
// their simpler versions can be used when only some attributes are
// needed and retrieving those from the blackboard is an unwanted overhead
using GoalSupplier = std::function<tileindex_t(Blackboard &)>;
using SimpleGoalSupplier = std::function<tileindex_t(const Bot *, GameState *)>;
using PathFlagsSupplier = std::function<pathflags_t(Blackboard &)>;
using GoalValidityChecker = std::function<bool(Blackboard &)>;
using SimpleGoalValidityChecker = std::function<bool(const Bot *, const Map *, tileindex_t)>;

// adapt simple functions to ones with more capabilities for when you don't need theese capabilities
GoalSupplier        adaptGoalSupplier(SimpleGoalSupplier &&simpleSupplier);
GoalValidityChecker adaptGoalValidityChecker(SimpleGoalValidityChecker &&simpleChecker);
PathFlagsSupplier   adaptFlagsSupplier(pathflags_t flags);

// common workers/bots tasks
std::shared_ptr<Task> taskMoveTo(GoalSupplier &&goalSupplier, GoalValidityChecker &&goalValidityChecker, PathFlagsSupplier &&pathFlags, const std::string &pathtype);
std::shared_ptr<Task> taskMoveTo(SimpleGoalSupplier &&goalSupplier, SimpleGoalValidityChecker &&goalValidityChecker, pathflags_t pathFlags, const std::string &pathtype);
std::shared_ptr<Task> taskFetchResources(float distanceWeight=-1.f);
std::shared_ptr<Task> taskBuildCity();
std::shared_ptr<Task> taskFeedCity();
std::shared_ptr<Task> taskMoveToCreateRoad();

// city tasks
std::shared_ptr<Task> taskCityCreateBot();
std::shared_ptr<Task> taskCityResearch();

// final behaviors (the only methods meant to be exposed from this file)
std::shared_ptr<Task> behaviorCity();
std::shared_ptr<Task> behaviorWorker();
std::shared_ptr<Task> behaviorCart();

}

#endif