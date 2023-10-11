#ifndef BEHAVIORTREENODES_H
#define BEHAVIORTREENODES_H

#include <vector>

#include "BehaviorTree.h"
#include "BehaviorTreeNames.h"
#include "game_rules.h"
#include "types.h"
#include "turn_order.h"
#include "log.h"
#include "astar.h"

namespace nodes
{
    static constexpr float ADJACENT_CITIES_WEIGHT = 10.0f;
    static constexpr float ADEQUATE_RESOURCE_WEIGHT = 1.0f;
    static constexpr float DISTANCE_WEIGHT = 1.0f;

inline std::shared_ptr<Task> testIsNight() {
  return std::make_shared<Test>([](Blackboard &bb) {
    size_t turn = bb.getData<size_t>(bbn::GLOBAL_TURN);
    return (turn % game_rules::DAYNIGHT_CYCLE_DURATION) >= game_rules::DAY_DURATION;
  });
}

inline std::shared_ptr<Task> testIsAgentFullOfResources()
{
  return std::make_shared<Test>([](Blackboard &bb) {
    Bot *bot = bb.getData<Bot *>(bbn::AGENT_SELF);
    return bot->getCoalAmount() + bot->getUraniumAmount() + bot->getUraniumAmount() >= game_rules::WORKER_CARRY_CAPACITY;
  });
}

// debugging method, prints and returns success
inline std::shared_ptr<Task> taskLog(const std::string &text)
{
  return std::make_shared<SimpleAction>([text](Blackboard &) { LOG(text); });
}

// utility method, adapts a function to a task
inline std::shared_ptr<Task> taskPlayAgentTurn(std::function<TurnOrder(Bot *bot)> &&orderSupplier)
{
  return std::make_shared<WithResult>(TaskResult::PENDING,
    std::make_shared<SimpleAction>([orderSupplier = std::move(orderSupplier)](Blackboard &bb) {
      Bot *bot = bb.getData<Bot*>(bbn::AGENT_SELF);
      bb.getData<std::vector<TurnOrder>*>(bbn::GLOBAL_ORDERS_LIST)->push_back(orderSupplier(bot));
    })
  );
}

// utility method, adapts a function to a task
inline std::shared_ptr<Task> taskPlayAgentTurn(std::function<TurnOrder(Blackboard &bb)> &&orderSupplier)
{
  return std::make_shared<WithResult>(TaskResult::PENDING,
    std::make_shared<SimpleAction>([orderSupplier = std::move(orderSupplier)](Blackboard &bb) {
      bb.getData<std::vector<TurnOrder>*>(bbn::GLOBAL_ORDERS_LIST)->push_back(orderSupplier(bb));
    })
  );
}

inline std::shared_ptr<Task> taskMoveTo(std::function<tileindex_t(Blackboard &)> &&goalFinder)
{
  auto checkPathValidity = [](Blackboard &bb) -> bool { return true; }; // TODO implement path validity check

  return
    std::make_shared<Sequence>(
      // create a path if there is not already a valid one
      std::make_shared<Selector>(
        std::make_shared<Test>([&](Blackboard &bb) {
          return bb.hasData(bbn::AGENT_PATHFINDING_PATH) && checkPathValidity(bb); // bb.getData<std::vector<tileindex_t>>(bbn::AGENT_PATHFINDING_PATH)
        }),
        std::make_shared<Sequence>(
          // create a goal if there is not already one
          std::make_shared<Selector>(
            std::make_shared<Test>([](Blackboard &bb) { return bb.hasData(bbn::AGENT_PATHFINDING_GOAL); }),
            std::make_shared<SimpleAction>([goalFinder = std::move(goalFinder)](Blackboard &bb) {
               bb.insertData(bbn::AGENT_PATHFINDING_GOAL, goalFinder(bb));
            })
          ),
          // create a valid path to the cached goal
          std::make_shared<ComplexAction>([&](Blackboard &bb) {
            Bot* bot = bb.getData<Bot*>(bbn::AGENT_SELF);
            Map *map = bb.getData<Map*>(bbn::GLOBAL_MAP);
            tileindex_t goalIndex = bb.getData<tileindex_t>(bbn::AGENT_PATHFINDING_GOAL);
            std::vector<tileindex_t> path = aStar(*map, *bot, goalIndex);
            if (path.empty()) {
              LOG("A unit could not find a valid path to its target tile");
              return TaskResult::FAILURE; // probably due to the goal not being valid/reachable
            }
            bb.insertData(bbn::AGENT_PATHFINDING_PATH, std::move(path));
            return TaskResult::SUCCESS;
          })
        )
      ),
      taskLog("Proceding to follow path"),
      // follow the path if we aren't already there
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

inline std::shared_ptr<Task> taskFetchResources()
{
  auto testHasEnoughResources = std::make_shared<Test>([](Blackboard &bb) {
    Bot *bot = bb.getData<Bot *>(bbn::AGENT_SELF);
    return bot->getCoalAmount() * game_rules::COAL_VALUE + bot->getWoodAmount() * game_rules::WOOD_VALUE + bot->getUraniumAmount() * game_rules::URANIUM_VALUE >= game_rules::WORKER_CARRY_CAPACITY;
  });

  auto getResourceFetchingLocation = [](Blackboard &bb) -> tileindex_t 
      {
          Bot* bot = bb.getData<Bot*>(bbn::AGENT_SELF);
          const int neededResources = game_rules::WORKER_CARRY_CAPACITY - (bot->getCoalAmount() * game_rules::COAL_VALUE + bot->getWoodAmount() * game_rules::WOOD_VALUE + bot->getUraniumAmount() * game_rules::URANIUM_VALUE);
          kit::ResourceType preferredResource = kit::ResourceType::wood;
          if (neededResources % game_rules::COAL_VALUE == 0)
              preferredResource = kit::ResourceType::coal;
          if (neededResources % game_rules::URANIUM_VALUE == 0)
              preferredResource = kit::ResourceType::uranium;

          tileindex_t tile = 0;
          float tileScore = 0;

          Map map = bb.getData<Map>(bbn::GLOBAL_MAP);
          for (size_t i = 0; i < map.getMapSize(); i++)
          {
              if (map.tileAt(i).getType() != TileType::RESOURCE || i == map.getTileIndex(*bot)) continue;
              std::pair<int,int> coords = map.getTilePosition(i);
              if (tileScore < (map.tileAt(i).getResourceType() == preferredResource ? ADEQUATE_RESOURCE_WEIGHT : 0) + DISTANCE_WEIGHT / (abs(bot->getX() - coords.first) + abs(bot->getY() - coords.second)))
              {
                  tile = i;
                  tileScore = (map.tileAt(i).getResourceType() == preferredResource ? ADEQUATE_RESOURCE_WEIGHT : 0) + DISTANCE_WEIGHT / (abs(bot->getX() - coords.first) + abs(bot->getY() - coords.second));
              }
          }

          return tile; 
      };

  return std::make_shared<Selector>(
    testIsAgentFullOfResources(),
    taskMoveTo(getResourceFetchingLocation),
    taskPlayAgentTurn([](Bot *bot) { return TurnOrder{ TurnOrder::COLLECT_RESOURCES, bot }; })
  );
}

inline std::shared_ptr<Task> taskBuildCity()
{
  auto getBestCityBuildingPlace = [](Blackboard &bb) -> tileindex_t 
      {
          Bot* bot = bb.getData<Bot*>(bbn::AGENT_SELF);
          Map map = bb.getData<Map>(bbn::GLOBAL_MAP);

          tileindex_t tile = 0;
          float tileScore = 0;
          for (size_t i = 0; i < map.getMapSize(); i++)
          {
              std::pair<int, int> coords = map.getTilePosition(i);
              size_t neighborCities = 0;
              std::vector<tileindex_t> neighbors = map.getValidNeighbours(i);
              for (size_t j = 0; j < neighbors.size(); j++)
              {
                  if (map.tileAt(neighbors[i]).getType() == TileType::ALLY_CITY)
                      neighborCities++;
              }
              if (tileScore < neighborCities * ADJACENT_CITIES_WEIGHT + DISTANCE_WEIGHT / (abs(bot->getX() - coords.first) + abs(bot->getY() - coords.second)))
              {
                  tile = i;
                  tileScore = neighborCities * ADJACENT_CITIES_WEIGHT + DISTANCE_WEIGHT / (abs(bot->getX() - coords.first) + abs(bot->getY() - coords.second));
              }
          }
          return tile; 
      };

  return std::make_shared<Sequence>(
    taskFetchResources(),
    taskMoveTo(getBestCityBuildingPlace),
    taskPlayAgentTurn([](Bot *bot) { return TurnOrder{ TurnOrder::BUILD_CITY, bot }; })
  );
}

inline std::shared_ptr<Task> behaviorWorker()
{
  // TODO implement day-night behavior difference
  return std::make_shared<Selector>(
    taskBuildCity(),
    taskPlayAgentTurn([](Bot *bot) {
      LOG("Bot " << bot->getId() << " had nothing to do!");
      return TurnOrder{ TurnOrder::DO_NOTHING, bot };
    })
  );
}

inline std::shared_ptr<Task> behaviorCity()
{
  // TODO implement city behaviors
  return std::make_shared<SimpleAction>([](Blackboard&) {});
}

}

#endif