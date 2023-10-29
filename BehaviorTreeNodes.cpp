#include "BehaviorTreeNodes.h"

#include "Pathing.h"
#include "CommandChain.h"

namespace nodes
{


std::shared_ptr<Task> testIsNight() {
  return std::make_shared<Test>([](Blackboard &bb) {
    size_t turn = bb.getData<size_t>(bbn::GLOBAL_TURN);
    return (turn % game_rules::DAYNIGHT_CYCLE_DURATION) >= game_rules::DAY_DURATION;
  });
}

std::shared_ptr<Task> testIsDawnOrNight() {
  constexpr size_t dawnDuration = 3;
  return std::make_shared<Test>([](Blackboard &bb) {
    size_t turn = bb.getData<size_t>(bbn::GLOBAL_TURN);
    return (turn % game_rules::DAYNIGHT_CYCLE_DURATION) >= game_rules::DAY_DURATION - dawnDuration;
  });
}

std::shared_ptr<Task> testIsAgentFullOfResources()
{
  return std::make_shared<Test>([](Blackboard &bb) {
    Bot *bot = bb.getData<Bot *>(bbn::AGENT_SELF);
    return bot->getCoalAmount() + bot->getWoodAmount() + bot->getUraniumAmount() >= game_rules::WORKER_CARRY_CAPACITY;
  });
}

std::shared_ptr<Task> testHasTeamEnoughAgents()
{
  return std::make_shared<Test>([](Blackboard& bb) {
    return bb.getData<int>(bbn::GLOBAL_AGENTS) >= 8;
  });
}
std::shared_ptr<Task> testHasTeamEnoughWorkers()
{
    return std::make_shared<Test>([](Blackboard &bb) {
        return bb.getData<int>(bbn::GLOBAL_AGENTS) >= 5;
    });
}
std::shared_ptr<Task> testHasTeamEnoughCarts()
{
    return std::make_shared<Test>([](Blackboard &bb) {
        return bb.getData<int>(bbn::GLOBAL_AGENTS) >= 1;
    });
}


std::shared_ptr<Task> testHasTeamReachedAgentCapacity()
{
  return std::make_shared<Test>([](Blackboard& bb) {
    return bb.getData<int>(bbn::GLOBAL_AGENTS) >= bb.getData<int>(bbn::GLOBAL_CITY_COUNT);
  });
}

std::shared_ptr<Task> testHasTeamEnoughResearchPoint()
{
  return std::make_shared<Test>([](Blackboard& bb) {
    return bb.getData<size_t>(bbn::GLOBAL_TEAM_RESEARCH_POINT) >= game_rules::MIN_RESEARCH_URANIUM;
  });
}

std::shared_ptr<Task> taskLog(const std::string &text, TaskResult result)
{
  return std::make_shared<WithResult>(result, std::make_shared<SimpleAction>([text](Blackboard &bb) {
    Bot *bot = bb.getData<Bot *>(bbn::AGENT_SELF);
    LOG(bot->getId() << ": " << text);
  }));
}

std::shared_ptr<Task> taskLog(const std::string &text, const std::shared_ptr<Task> &wrapped) {
  return std::make_shared<Sequence>(taskLog(text), wrapped);
}

std::shared_ptr<Task> taskPlayAgentTurn(std::function<TurnOrder(Bot *bot)> &&orderSupplier)
{
  return std::make_shared<WithResult>(TaskResult::PENDING,
    std::make_shared<SimpleAction>([orderSupplier = std::move(orderSupplier)](Blackboard &bb) {
      Bot *bot = bb.getData<Bot*>(bbn::AGENT_SELF);
      bb.getData<std::vector<TurnOrder>*>(bbn::GLOBAL_ORDERS_LIST)->push_back(orderSupplier(bot));
    })
  );
}

std::shared_ptr<Task> taskPlayAgentTurn(std::function<TurnOrder(Blackboard &bb)> &&orderSupplier)
{
  return std::make_shared<WithResult>(TaskResult::PENDING,
    std::make_shared<SimpleAction>([orderSupplier = std::move(orderSupplier)](Blackboard &bb) {
      bb.getData<std::vector<TurnOrder>*>(bbn::GLOBAL_ORDERS_LIST)->push_back(orderSupplier(bb));
    })
  );
}

std::shared_ptr<Task> taskMoveTo(
  std::function<tileindex_t(Blackboard &)> &&goalFinder,
  std::function<bool(Blackboard &)> &&goalValidityChecker,
  pathflags_t pathFlags,
  const std::string &pathtype)
{
  auto followPathTask =
    std::make_shared<Selector>(
      std::make_shared<Test>([](Blackboard &bb) {
        // if the path has ended, short-circuit
        return bb.getData<std::vector<tileindex_t>>(bbn::AGENT_PATHFINDING_PATH).empty();
      }),
      taskPlayAgentTurn([](Blackboard &bb) {
        auto &path = bb.getData<std::vector<tileindex_t>>(bbn::AGENT_PATHFINDING_PATH);
        auto occupiedTiles = bb.getData<std::vector<tileindex_t>*>(bbn::GLOBAL_AGENTS_POSITION);
        const Bot *bot = bb.getData<Bot *>(bbn::AGENT_SELF);
        tileindex_t nextTile = path.back();
        occupiedTiles->push_back(nextTile);
        return TurnOrder{ TurnOrder::MOVE, bot, nextTile };
      })
    );

  auto computePathTask = 
    std::make_shared<ComplexAction>([pathFlags](Blackboard &bb) {
      const Bot* bot = bb.getData<Bot*>(bbn::AGENT_SELF);
      const Map *map = bb.getData<Map*>(bbn::GLOBAL_MAP);
      tileindex_t goalIndex = bb.getData<tileindex_t>(bbn::AGENT_PATHFINDING_GOAL);
      auto occupiedTiles = bb.getData<std::vector<tileindex_t>*>(bbn::GLOBAL_AGENTS_POSITION);

      std::vector<tileindex_t> path = aStar(*map, *bot, goalIndex, *occupiedTiles, pathFlags);

      if (path.empty()) {
        LOG(bot->getId() << ": could not find a valid path to its target tile " << map->getTilePosition(goalIndex) << " from " << bot->getX() << "," << bot->getY());
        return TaskResult::FAILURE; // probably due to the goal not being valid/reachable
      }
      path.pop_back();
      bb.insertData(bbn::AGENT_PATHFINDING_PATH, std::move(path));
      return TaskResult::SUCCESS;
    });

  auto isGoalValidTest =
    std::make_shared<Test>([goalValidityChecker = std::move(goalValidityChecker)](Blackboard &bb) {
      return bb.hasData(bbn::AGENT_PATHFINDING_GOAL) && goalValidityChecker(bb);
    });

  auto computeGoalTask = 
    std::make_shared<ComplexAction>([goalFinder = std::move(goalFinder)](Blackboard &bb) {
      const Bot *bot = bb.getData<Bot *>(bbn::AGENT_SELF);
      const Map *map = bb.getData<Map *>(bbn::GLOBAL_MAP);
      tileindex_t target = goalFinder(bb);
      if (!map->isValidTileIndex(target)) {
        LOG(bot->getId() << ": could not find a valid target tile for " << bb.getData<std::string>(bbn::AGENT_PATHFINDING_TYPE));
        return TaskResult::PENDING;
      }
      bb.insertData(bbn::AGENT_PATHFINDING_GOAL, target);
      return TaskResult::SUCCESS;
    });

  auto isPathValidTest =
    std::make_shared<Test>([&](Blackboard &bb) {
      if(!bb.hasData(bbn::AGENT_PATHFINDING_PATH)) return false;
      const GameState *gameState = bb.getData<GameState*>(bbn::GLOBAL_GAME_STATE);
      const std::vector<tileindex_t> &path = bb.getData<std::vector<tileindex_t>>(bbn::AGENT_PATHFINDING_PATH);
      const std::vector<tileindex_t> &botPositions = *bb.getData<std::vector<tileindex_t>*>(bbn::GLOBAL_AGENTS_POSITION);
      constexpr size_t minimumValidTilesAhead = 3;
      return pathing::checkPathValidity(path, gameState->map, botPositions, minimumValidTilesAhead);
    });

  auto clearPathcacheTask =
    std::make_shared<SimpleAction>([=](Blackboard &bb) {
      bb.removeData(bbn::AGENT_PATHFINDING_GOAL);
      bb.removeData(bbn::AGENT_PATHFINDING_PATH);
      bb.insertData(bbn::AGENT_PATHFINDING_TYPE, pathtype);
    });

  auto objectiveChangedTest =
    std::make_shared<Test>([=](Blackboard &bb) {
      const Bot *bot = bb.getData<Bot *>(bbn::AGENT_SELF);
      bool changed = !bb.hasData(bbn::AGENT_PATHFINDING_TYPE) || pathtype != bb.getData<std::string>(bbn::AGENT_PATHFINDING_TYPE);
      if(changed) LOG(bot->getId() << ": action interupted from " << (bb.hasData(bbn::AGENT_PATHFINDING_TYPE) ? bb.getData<std::string>(bbn::AGENT_PATHFINDING_TYPE) : "none") << " to " << pathtype);
      return !changed;
    });

  auto taskRemoveCurrentTileFromPathCache =
    std::make_shared<SimpleAction>([](Blackboard &bb) {
      const Bot *bot = bb.getData<Bot *>(bbn::AGENT_SELF);
      const Map *map = bb.getData<Map *>(bbn::GLOBAL_MAP);
      if (!bb.hasData(bbn::AGENT_PATHFINDING_PATH)) return;
      auto &path = bb.getData<std::vector<tileindex_t>>(bbn::AGENT_PATHFINDING_PATH);
      if (path.empty()) return;
      tileindex_t nextTile = path.back();
      if (map->getTileIndex(*bot) == nextTile)
        path.pop_back();
      //else
      //  LOG(bot->getId() << ": did not move on the previous turn, probably due to a collision");
    });

  return
    std::make_shared<Sequence>(
      // first, remove the tile we're at if we moved the previous turn
      taskRemoveCurrentTileFromPathCache,

      // then, fully clear the path cache if the bot's objective changed
      std::make_shared<Selector>(
        objectiveChangedTest,
        clearPathcacheTask
      ),

      std::make_shared<Alternative>(
        isGoalValidTest,
        // if the goal is still valid...
        std::make_shared<Alternative>(
          isPathValidTest,
          // if the path is still valid simply follow it
          followPathTask,
          // otherwise compute a new path and follow that one
          std::make_shared<Sequence>(
            computePathTask,
            followPathTask
          )
        ),
        // ... if the goal is no longer valid...
        std::make_shared<Sequence>(
          // compute a new goal, path and follow it
          //taskLog("goal invalid"),
          computeGoalTask,
          computePathTask,
          followPathTask
        )
      )
    );
}

std::shared_ptr<Task> taskMoveTo(SimpleGoalSupplier &&goalSupplier, SimpleGoalValidityChecker &&goalValidityChecker, pathflags_t pathFlags, const std::string &pathtype)
{
  return taskMoveTo(
    [goalFinder = std::move(goalSupplier)](Blackboard &bb) -> tileindex_t {
      const Bot *bot = bb.getData<Bot*>(bbn::AGENT_SELF);
      const Map *map = bb.getData<Map*>(bbn::GLOBAL_MAP);
      return goalFinder(bot, map);
    },
    [goalValidityChecker = std::move(goalValidityChecker)](Blackboard &bb) -> bool {
      const Bot *bot = bb.getData<Bot*>(bbn::AGENT_SELF);
      const Map *map = bb.getData<Map*>(bbn::GLOBAL_MAP);
      tileindex_t goal = bb.getData<tileindex_t>(bbn::AGENT_PATHFINDING_GOAL);
      return goalValidityChecker(bot, map, goal);
    },
    pathFlags,
    pathtype);
}

std::shared_ptr<Task> taskFetchResources()
{
  auto testIsValidResourceFetchingLocation = [](const Bot *bot, const Map *map, tileindex_t goal) {
    if (map->tileAt(goal).getType() == TileType::RESOURCE) return true;
    auto neighbours = map->getValidNeighbours(goal);
    return std::ranges::any_of(neighbours, [=](tileindex_t t) {
      return map->tileAt(t).getType() == TileType::RESOURCE;
    });
  };

  return std::make_shared<Selector>(
    testIsAgentFullOfResources(),
    std::make_shared<Sequence>(
      taskMoveTo(pathing::getResourceFetchingLocation, testIsValidResourceFetchingLocation, PathFlags::CAN_MOVE_THROUGH_FRIENDLY_CITIES, "resource-fetching-site"),
      taskLog("Collecting resources"),
      taskPlayAgentTurn([](Bot *bot) { return TurnOrder{ TurnOrder::COLLECT_RESOURCES, bot }; })
    )
  );
}

std::function<tileindex_t(Blackboard &bb)> goalSupplierFromAgentObjective()
{
  return [](Blackboard &bb) {
    return bb.getData<BotObjective>(bbn::AGENT_OBJECTIVE).targetTile;
  };
}

std::shared_ptr<Task> taskBuildCity()
{
  auto testIsPathGoalValidConstructionTile = [](Blackboard &bb) {
    const Map *map = bb.getData<Map*>(bbn::GLOBAL_MAP);
    tileindex_t goal = bb.getData<tileindex_t>(bbn::AGENT_PATHFINDING_GOAL);
    return map->tileAt(goal).getType() == TileType::EMPTY;
  };

  return std::make_shared<Sequence>(
    taskFetchResources(),
    //taskLog("Enough resources, moving to city construction tile"),
    taskMoveTo(goalSupplierFromAgentObjective(), testIsPathGoalValidConstructionTile, PathFlags::NONE, "city-construction-site"),
    taskPlayAgentTurn([](Bot *bot) { return TurnOrder{ TurnOrder::BUILD_CITY, bot }; })
  );
}

std::shared_ptr<Task> taskFeedCity()
{
  auto testIsGoalValidFriendlyCityTile = [](Blackboard &bb) {
    const Map *map = bb.getData<Map *>(bbn::GLOBAL_MAP);
    tileindex_t goal = bb.getData<tileindex_t>(bbn::AGENT_PATHFINDING_GOAL);
    return map->tileAt(goal).getType() == TileType::ALLY_CITY;
  };

  return std::make_shared<Sequence>(
    taskFetchResources(),
    //taskLog("Enough resources, moving to supplying city tile"),
    taskMoveTo(
      goalSupplierFromAgentObjective(),
      testIsGoalValidFriendlyCityTile,
      PathFlags::CAN_MOVE_THROUGH_FRIENDLY_CITIES, // currently there is no garanty that the right city is fed
      "city-supplying-site")
  );
}

std::shared_ptr<Task> taskMoveToBlockTile() {
  auto isValidBlockingTile = [](Blackboard &bb) {
    const Map *map = bb.getData<Map *>(bbn::GLOBAL_MAP);
    tileindex_t goal = bb.getData<tileindex_t>(bbn::AGENT_PATHFINDING_GOAL);
    return map->tileAt(goal).getType() == TileType::EMPTY;
  };

  return taskMoveTo(
    goalSupplierFromAgentObjective(),
    isValidBlockingTile,
    PathFlags::CAN_MOVE_THROUGH_FRIENDLY_CITIES,
    "go-block-tile-strategy");
}

std::shared_ptr<Task> taskMoveToClosestFriendlyCity() {
  auto testIsGoalValidFriendlyCityTile = [](const Bot *bot, const Map *map, tileindex_t goal) {
    return map->tileAt(goal).getType() == TileType::ALLY_CITY;
  };

  return taskMoveTo(
    pathing::getBestNightTimeLocation, 
    testIsGoalValidFriendlyCityTile,
    PathFlags::CAN_MOVE_THROUGH_FRIENDLY_CITIES,
    "closest-city");
}

// The city create a worker if there is less than 7 workers
std::shared_ptr<Task> taskCityCreateWorker()
{
  return std::make_shared<Selector>(
    testHasTeamEnoughWorkers(),
    testHasTeamReachedAgentCapacity(),
    taskLog("There are not enough bot, creating worker", TaskResult::FAILURE),
    taskPlayAgentTurn([](Blackboard &bb) {
      const Bot *bot = bb.getData<Bot *>(bbn::AGENT_SELF);
      bb.updateData(bbn::GLOBAL_AGENTS, bb.getData<int>(bbn::GLOBAL_AGENTS) + 1);
      bb.updateData(bbn::GLOBAL_WORKERS, bb.getData<int>(bbn::GLOBAL_WORKERS) + 1);
      return TurnOrder{ TurnOrder::CREATE_WORKER, bot };
    })
  );
}

// Create a cart if there is less than 1 cart
std::shared_ptr<Task> taskCityCreateCart()
{
    return std::make_shared<Selector>(
      testHasTeamEnoughCarts(),
      testHasTeamReachedAgentCapacity(),
      taskLog("Creating cart", TaskResult::FAILURE),
      taskPlayAgentTurn([](Blackboard &bb) {
        const Bot *bot = bb.getData<Bot *>(bbn::AGENT_SELF);
        bb.updateData(bbn::GLOBAL_AGENTS, bb.getData<int>(bbn::GLOBAL_AGENTS) + 1);
        bb.updateData(bbn::GLOBAL_WORKERS, bb.getData<int>(bbn::GLOBAL_WORKERS) + 1);
        return TurnOrder{ TurnOrder::CREATE_CART, bot };
    })
    );
}



std::shared_ptr<Task> taskCityResearch()
{
  return std::make_shared<Selector>(
    testHasTeamEnoughResearchPoint(),
    taskLog("There are not enough research point, starting research", TaskResult::FAILURE),
    taskPlayAgentTurn([](Bot *bot) { return TurnOrder{ TurnOrder::RESEARCH, bot }; })
  );
}

std::shared_ptr<Task> behaviorCity()
{
  return std::make_shared<Sequence>(
    taskCityCreateWorker(),
    taskCityCreateCart(),
    taskCityResearch()
  );
}

class BotObjectiveAlternative : public Task
{
private:
  std::unordered_map<BotObjective::ObjectiveType, std::shared_ptr<Task>> m_strategies;
public:
  TaskResult run(const std::shared_ptr<Blackboard> &blackboard) override
  {
    BotObjective &objective = blackboard->getData<BotObjective>(bbn::AGENT_OBJECTIVE);
    if (!m_strategies.contains(objective.type)) throw std::runtime_error("Unimplemented bot objective: " + std::to_string((int)objective.type));
    return m_strategies[objective.type]->run(blackboard);
  }

  void addStrategy(BotObjective::ObjectiveType objectiveType, const std::shared_ptr<Task>& strategy)
  {
    m_strategies.emplace(objectiveType, strategy);
  }
};

std::shared_ptr<Task> behaviorWorker()
{
  auto taskPlaySquadProvidedObjective = std::make_shared<BotObjectiveAlternative>();
  taskPlaySquadProvidedObjective->addStrategy(BotObjective::ObjectiveType::BUILD_CITY, taskBuildCity());
  taskPlaySquadProvidedObjective->addStrategy(BotObjective::ObjectiveType::FEED_CITY, taskFeedCity());
  taskPlaySquadProvidedObjective->addStrategy(BotObjective::ObjectiveType::GO_BLOCK_PATH, taskMoveToBlockTile());

  return
    std::make_shared<Alternative>(
      testIsDawnOrNight(),
      taskMoveToClosestFriendlyCity(),
      std::make_shared<Sequence>(
        taskPlaySquadProvidedObjective,
        taskLog("Agent had nothing to do!")
      )
    );
}

}