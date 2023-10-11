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

inline std::shared_ptr<Task> taskPlayAgentTurn(std::function<TurnOrder(Bot *bot)> &&orderSupplier)
{
  return std::make_shared<WithResult>(TaskResult::PENDING,
    std::make_shared<SimpleAction>([orderSupplier = std::move(orderSupplier)](Blackboard &bb) {
      Bot *bot = bb.getData<Bot*>(bbn::AGENT_SELF);
      bb.getData<std::vector<TurnOrder>>(bbn::GLOBAL_ORDERS_LIST).push_back(orderSupplier(bot));
    })
  );
}

inline std::shared_ptr<Task> taskPlayAgentTurn(std::function<TurnOrder(Blackboard &bb)> &&orderSupplier)
{
  return std::make_shared<WithResult>(TaskResult::PENDING,
    std::make_shared<SimpleAction>([orderSupplier = std::move(orderSupplier)](Blackboard &bb) {
      bb.getData<std::vector<TurnOrder>>(bbn::GLOBAL_ORDERS_LIST).push_back(orderSupplier(bb));
    })
  );
}

inline std::shared_ptr<Task> taskMoveTo(std::function<tileindex_t(Blackboard &)> &&goalFinder)
{
  auto pathfind = []() -> std::vector<tileindex_t> { return {}; }; // TODO link with pathfinding algorithm (replace by an actual function)
  auto checkPathValidity = [](Blackboard &bb) -> bool { return true; }; // TODO implement path validity check

  return 
    std::make_shared<Sequence>(
      std::make_shared<Selector>(
        std::make_shared<Test>([&](Blackboard &bb) {
          return bb.hasData(bbn::AGENT_PATHFINDING_PATH) && checkPathValidity(bb); // bb.getData<std::vector<tileindex_t>>(bbn::AGENT_PATHFINDING_PATH)
        }),
        std::make_shared<Sequence>(
          std::make_shared<Selector>(
            std::make_shared<Test>([](Blackboard &bb) { return bb.hasData(bbn::AGENT_PATHFINDING_GOAL); }),
            std::make_shared<SimpleAction>([goalFinder = std::move(goalFinder)](Blackboard &bb) {
               bb.insertData(bbn::AGENT_PATHFINDING_GOAL, goalFinder(bb));
            })
          ),
          std::make_shared<SimpleAction>([&](Blackboard &bb) {
            bb.insertData(bbn::AGENT_PATHFINDING_PATH, pathfind());
          })
        )
      ),
      std::make_shared<Selector>(
        std::make_shared<Test>([](Blackboard &bb) { return bb.getData<std::vector<tileindex_t>>(bbn::AGENT_PATHFINDING_PATH).empty(); }),
        taskPlayAgentTurn([](Blackboard &bb) {
          std::vector<tileindex_t> &path = bb.getData<std::vector<tileindex_t>>(bbn::AGENT_PATHFINDING_PATH);
          Bot *bot = bb.getData<Bot *>(bbn::AGENT_SELF);

          tileindex_t nextTile = path.back();
          path.pop_back();
          return TurnOrder{ TurnOrder::MOVE, bot, nextTile };
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

  auto getResourceFetchingLocation = [](Blackboard &bb) -> tileindex_t { return 0; }; // TODO implement resource research

  return std::make_shared<Selector>(
    testHasEnoughResources,
    taskMoveTo(getResourceFetchingLocation),
    taskPlayAgentTurn([](Bot *bot) { return TurnOrder{ TurnOrder::COLLECT_RESOURCES, bot }; })
  );
}

inline std::shared_ptr<Task> taskBuildCity()
{
  auto getBestCityBuildingPlace = [](Blackboard &) -> tileindex_t { return 0; }; // TODO implement building location research

  return std::make_shared<Sequence>(
    taskFetchResource(),
    taskMoveTo(getBestCityBuildingPlace),
    taskPlayAgentTurn([](Bot *bot) { return TurnOrder{ TurnOrder::BUILD_CITY, bot }; })
  );
}

}

#endif