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

std::shared_ptr<Task> testIsNight();
std::shared_ptr<Task> testIsDawnOrNight();
std::shared_ptr<Task> testIsAgentFullOfResources();
std::shared_ptr<Task> testHasTeamEnoughAgents();
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

using GoalSupplier = std::function<tileindex_t(Blackboard &)>;
using SimpleGoalSupplier = std::function<tileindex_t(const Bot *, const GameState *)>;
using GoalValidityChecker = std::function<bool(Blackboard &)>;
using SimpleGoalValidityChecker = std::function<bool(const Bot *, const Map *, tileindex_t)>;

std::shared_ptr<Task> taskMoveTo(GoalSupplier &&goalSupplier, GoalValidityChecker &&goalValidityChecker, pathflags_t pathFlags, const std::string &pathtype);
std::shared_ptr<Task> taskMoveTo(SimpleGoalSupplier &&goalSupplier, SimpleGoalValidityChecker &&goalValidityChecker, pathflags_t pathFlags, const std::string &pathtype);
std::shared_ptr<Task> taskFetchResources(float distanceWeight=-1.f);
std::shared_ptr<Task> taskBuildCity();
std::shared_ptr<Task> taskFeedCity();

std::shared_ptr<Task> taskCityCreateBot();
std::shared_ptr<Task> taskCityResearch();

std::shared_ptr<Task> behaviorCity();
std::shared_ptr<Task> behaviorWorker();
std::shared_ptr<Task> behaviorCart();

}

#endif