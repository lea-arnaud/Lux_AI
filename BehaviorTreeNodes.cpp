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
  constexpr size_t dawnDuration = 5;
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

std::shared_ptr<Task> testHasTeamReachedAgentCapacity()
{
  return std::make_shared<Test>([](Blackboard& bb) {
    return bb.getData<int>(bbn::GLOBAL_AGENTS) >= bb.getData<size_t>(bbn::GLOBAL_FRIENDLY_CITY_COUNT);
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

std::shared_ptr<Task> taskMoveTo(std::function<tileindex_t(Blackboard &)> &&goalFinder, const std::string &pathtype)
{
  return
    std::make_shared<Sequence>(
      std::make_shared<Selector>(
        // Path caching is way more complicated than anticipated, for now computing an A* every turn will have to do
        //std::make_shared<Test>([=](Blackboard &bb) {
        //  return bb.hasData(bbn::AGENT_PATHFINDING_TYPE) &&
        //    pathtype == bb.getData<std::string>(bbn::AGENT_PATHFINDING_TYPE);
        //}),
        std::make_shared<SimpleAction>([=](Blackboard &bb) {
          //LOG("Action interupted: from " << (bb.hasData(bbn::AGENT_PATHFINDING_TYPE) ? bb.getData<std::string>(bbn::AGENT_PATHFINDING_TYPE) : "none") << " to " << pathtype);
          bb.removeData(bbn::AGENT_PATHFINDING_GOAL);
          bb.removeData(bbn::AGENT_PATHFINDING_PATH);
          bb.insertData(bbn::AGENT_PATHFINDING_TYPE, pathtype);
        })
      ),
      // create a path if there is not already a valid one
      std::make_shared<Selector>(
        std::make_shared<Test>([&](Blackboard &bb) {
          if(!bb.hasData(bbn::AGENT_PATHFINDING_PATH)) return false;
          const GameState *gameState = bb.getData<GameState*>(bbn::GLOBAL_GAME_STATE);
          const std::vector<tileindex_t> &path = bb.getData<std::vector<tileindex_t>>(bbn::AGENT_PATHFINDING_PATH);
          constexpr size_t minimumValidTilesAhead = 3;
          return pathing::checkPathValidity(path, *gameState, minimumValidTilesAhead);
        }),
        std::make_shared<Sequence>(
          // create a goal if there is not already one
          std::make_shared<Selector>(
            std::make_shared<Test>([](Blackboard &bb) { return bb.hasData(bbn::AGENT_PATHFINDING_GOAL); }),
            std::make_shared<ComplexAction>([goalFinder = std::move(goalFinder)](Blackboard &bb) {
              tileindex_t target = goalFinder(bb);
              const Map *map = bb.getData<Map *>(bbn::GLOBAL_MAP);
              if (!map->isValidTileIndex(target)) {
                LOG("A unit could not find a valid target tile for " << bb.getData<std::string>(bbn::AGENT_PATHFINDING_TYPE));
                return TaskResult::PENDING;
              }
              bb.insertData(bbn::AGENT_PATHFINDING_GOAL, target);
              return TaskResult::SUCCESS;
            })
          ),
          // create a valid path to the cached goal
          std::make_shared<ComplexAction>([&](Blackboard &bb) {
            Bot* bot = bb.getData<Bot*>(bbn::AGENT_SELF);
            const Map *map = bb.getData<Map*>(bbn::GLOBAL_MAP);
            tileindex_t goalIndex = bb.getData<tileindex_t>(bbn::AGENT_PATHFINDING_GOAL);
            std::vector<tileindex_t> path = aStar(*map, *bot, goalIndex);
            if (path.empty()) {
              LOG("A unit could not find a valid path to its target tile " << map->getTilePosition(goalIndex) << " from " << bot->getX() << "," << bot->getY());
              return TaskResult::FAILURE; // probably due to the goal not being valid/reachable
            }
            path.pop_back();
            bb.insertData(bbn::AGENT_PATHFINDING_PATH, std::move(path));
            return TaskResult::SUCCESS;
          })
        )
      ),
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

std::shared_ptr<Task> taskMoveTo(std::function<tileindex_t(const Bot *, const Map *)> &&goalFinder, const std::string &pathtype)
{
  return taskMoveTo([goalFinder = std::move(goalFinder)](Blackboard &bb) -> tileindex_t {
    const Bot *bot = bb.getData<Bot*>(bbn::AGENT_SELF);
    const Map *map = bb.getData<Map*>(bbn::GLOBAL_MAP);
    return goalFinder(bot, map);
  }, pathtype);
}

std::shared_ptr<Task> taskFetchResources()
{
  return std::make_shared<Selector>(
    testIsAgentFullOfResources(),
    taskLog("Not enough resources", TaskResult::FAILURE),
    std::make_shared<Sequence>(
      taskMoveTo(pathing::getResourceFetchingLocation, "resource-fetching-site"),
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
  return std::make_shared<Sequence>(
    taskFetchResources(),
    taskLog("Enough resources, moving to city construction tile"),
    taskMoveTo(goalSupplierFromAgentObjective(), "city-construction-site"),
    taskPlayAgentTurn([](Bot *bot) { return TurnOrder{ TurnOrder::BUILD_CITY, bot }; })
  );
}

std::shared_ptr<Task> taskFeedCity()
{
  return std::make_shared<Sequence>(
    taskFetchResources(),
    taskLog("Enough resources, moving to supplying city tile"),
    taskMoveTo(goalSupplierFromAgentObjective(), "city-supplying-site")
  );
}

std::shared_ptr<Task> taskCityCreateBot()
{
  return std::make_shared<Selector>(
    testHasTeamEnoughAgents(),
    testHasTeamReachedAgentCapacity(),
    taskLog("There are not enough bot, creating worker", TaskResult::FAILURE),
    taskPlayAgentTurn([](Bot *bot) { return TurnOrder{ TurnOrder::CREATE_WORKER, bot }; })
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
    taskCityCreateBot(),
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

  void addStrategy(BotObjective::ObjectiveType objectiveType, std::shared_ptr<Task> strategy)
  {
    m_strategies.emplace(objectiveType, strategy);
  }
};

std::shared_ptr<Task> behaviorWorker()
{
  auto taskPlaySquadProvidedObjective = std::make_shared<BotObjectiveAlternative>();
  taskPlaySquadProvidedObjective->addStrategy(BotObjective::ObjectiveType::BUILD_CITY, taskBuildCity());
  taskPlaySquadProvidedObjective->addStrategy(BotObjective::ObjectiveType::FEED_CITY, taskFeedCity());
  taskPlaySquadProvidedObjective->addStrategy(BotObjective::ObjectiveType::GO_BLOCK_PATH, taskMoveTo(goalSupplierFromAgentObjective(), "go-block-path-strategy"));

  return
    std::make_shared<Alternative>(
      testIsDawnOrNight(),
      taskMoveTo(pathing::getBestNightTimeLocation, "closest-city"),
      std::make_shared<Sequence>(
        taskPlaySquadProvidedObjective,
        taskLog("Agent had nothing to do")
      )
    );
}

}