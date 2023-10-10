#ifndef BEHAVIORTREENODES_H
#define BEHAVIORTREENODES_H

#include <vector>

#include "BehaviorTree.h"
#include "BehaviorTreeNames.h"
#include "game_rules.h"
#include "types.h"
#include "turn_order.h"

namespace nodes
{

inline std::shared_ptr<Task> testIsNight() {
  return std::make_shared<Test>([](Blackboard &bb) {
    size_t turn = bb.getData<size_t>(bbn::GLOBAL_TURN);
    return (turn % game_rules::DAYNIGHT_CYCLE_DURATION) >= game_rules::DAY_DURATION;
  });
}

inline std::shared_ptr<Task> taskMoveTo(std::function<tileindex_t(Blackboard &)> &&goalFinder)
{
  // FIX add the path following part
  // I'm not sure how to incorporate the part responsible for testing if the current path is still valid or not
  return 
    std::make_shared<Sequence>(
      std::make_shared<Selector>(
        std::make_shared<Test>([](Blackboard &bb) { return bb.hasData(bbn::AGENT_PATHFINDING_GOAL); }),
        std::make_shared<SimpleAction>([goalFinder = std::move(goalFinder)](Blackboard &bb) {
           bb.insertData(bbn::AGENT_PATHFINDING_GOAL, goalFinder(bb));
        })
      )
    );
}

inline std::shared_ptr<Task> taskFetchResource()
{
  auto testHasEnoughResources = std::make_shared<Test>([](Blackboard &bb) {
    Bot *bot = bb.getData<Bot *>(bbn::AGENT_SELF);
    return bot->getCoalAmount() + bot->getUraniumAmount() + bot->getUraniumAmount() >= game_rules::WORKER_CARRY_CAPACITY;
  });

  return std::make_shared<Selector>(
    testHasEnoughResources
    // TODO
  );
}

inline std::shared_ptr<Task> taskPlayAgentTurn(std::function<TurnOrder(Bot *bot)> &&orderSupplier)
{
  return std::make_shared<WithResult>(TaskResult::PENDING,
    std::make_shared<SimpleAction>([orderSupplier = std::move(orderSupplier)](Blackboard &bb) {
      Bot *bot = bb.getData<Bot*>(bbn::AGENT_SELF);
      bb.getData<std::vector<TurnOrder>>(bbn::GLOBAL_ORDERS_LIST).push_back(orderSupplier(bot));
    })
  );
}

inline std::shared_ptr<Task> taskBuildCity()
{
  auto getBestCityBuildingPlace = [](Blackboard &) -> tileindex_t { return 0; }; // to be implemented

  return std::make_shared<Sequence>(
    taskFetchResource(),
    taskMoveTo(getBestCityBuildingPlace),
    taskPlayAgentTurn([](Bot *bot) { return TurnOrder{ TurnOrder::BUILD_CITY, bot }; })
  );
}

}

#endif