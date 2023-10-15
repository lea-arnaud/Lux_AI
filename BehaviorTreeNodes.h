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

namespace nodes
{

std::shared_ptr<Task> testIsNight();
std::shared_ptr<Task> testIsAgentFullOfResources();
std::shared_ptr<Task> testHasTeamEnoughAgents();
std::shared_ptr<Task> testHasTeamEnoughResearchPoint();

// debugging method, prints and returns success
std::shared_ptr<Task> taskLog(const std::string &text, TaskResult result = TaskResult::SUCCESS);
// utility method, adapts a function to a task
std::shared_ptr<Task> taskPlayAgentTurn(std::function<TurnOrder(Bot *bot)> &&orderSupplier);
// utility method, adapts a function to a task
std::shared_ptr<Task> taskPlayAgentTurn(std::function<TurnOrder(Blackboard &bb)> &&orderSupplier);

std::shared_ptr<Task> taskMoveTo(std::function<tileindex_t(Blackboard &)> &&goalFinder, const std::string &pathtype);
std::shared_ptr<Task> taskFetchResources();
std::shared_ptr<Task> taskBuildCity();

std::shared_ptr<Task> taskCityCreateBot();
std::shared_ptr<Task> taskCityResearch();

std::shared_ptr<Task> behaviorCity();
std::shared_ptr<Task> behaviorWorker();

}

#endif