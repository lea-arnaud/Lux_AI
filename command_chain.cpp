#include "command_chain.h"

#include <algorithm>

#include "game_rules.h"
#include "log.h"

static std::shared_ptr<BasicBehavior> WORKER_BEHAVIOR;
static std::shared_ptr<BasicBehavior> CART_BEHAVIOR;
static std::shared_ptr<BasicBehavior> CITY_BEHAVIOR;

Commander::Commander()
{
    m_squads.push_back({}); // to be removed, the initial implementation uses one squad only
}

void Commander::updateHighLevelObjectives(const GameState &state, const GameStateDiff &diff)
{
    Squad &firstSquad = m_squads[0];
    firstSquad.getAgents().clear();
    for (const Bot &bot : state.bots) {
        auto agentBehavior = (
          bot.getType() == UNIT_TYPE::WORKER ? WORKER_BEHAVIOR :
          bot.getType() == UNIT_TYPE::CART ? CART_BEHAVIOR :
          bot.getType() == UNIT_TYPE::CITY ? CITY_BEHAVIOR :
          WORKER_BEHAVIOR/*unreachable*/);
        // TODO keep track of agent blackboard states between turns
        auto agentBlackboard = std::make_shared<Blackboard>();

        SquadAgent agent{ &bot, agentBlackboard, agentBehavior };

        agentBlackboard->insertData(bbn::AGENT_SELF, &agent);
        firstSquad.getAgents().push_back(std::move(agent));
    }
}

std::vector<TurnOrder> Commander::getTurnOrders(const Map &map)
{
    static int turnNumber = 0;
    LOG("Turn " << ++turnNumber);

    std::vector<TurnOrder> orders;
    m_globalBlackboard.insertData(bbn::GLOBAL_MAP, &map);
    m_globalBlackboard.insertData(bbn::GLOBAL_ORDERS_LIST, &orders);

    // collect agents that can act right now
    std::vector<SquadAgent*> availableAgents;
    for (Squad &squad : m_squads) {
        for (SquadAgent &agents : squad.getAgents()) {
            if(agents.getBot().getCooldown() < game_rules::MAX_ACT_COOLDOWN)
                availableAgents.push_back(&agents);
        }
    }

    // TODO restore when the behavior trees are restored
    //std::for_each(availableAgents.begin(), availableAgents.end(),
    //  [](SquadAgent *agent) { agent->act(); });

    // not critical, but keeping dandling pointers alive is never a good idea
    m_globalBlackboard.removeData(bbn::GLOBAL_ORDERS_LIST);

    return orders;
}

