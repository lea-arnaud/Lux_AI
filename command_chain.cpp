#include "command_chain.h"

#include <algorithm>

#include "game_rules.h"
#include "log.h"

Commander::Commander()
  : m_globalBlackboard(std::make_shared<Blackboard>())
{
    m_squads.push_back({}); // to be removed, the initial implementation uses one squad only
}

void Commander::updateHighLevelObjectives(GameState &state, const GameStateDiff &diff)
{
    Squad &firstSquad = m_squads[0];
    firstSquad.getAgents().clear();
    for (Bot &bot : state.bots) {
      if (bot.getTeam() == Player::ALLY)
        firstSquad.getAgents().push_back(&bot);
    }
}

std::vector<TurnOrder> Commander::getTurnOrders(const Map &map)
{
    static int turnNumber = 0;
    LOG("Turn " << ++turnNumber);
    static int nbAgents;

    std::vector<TurnOrder> orders;
    m_globalBlackboard->insertData(bbn::GLOBAL_MAP, &map);
    m_globalBlackboard->insertData(bbn::GLOBAL_ORDERS_LIST, &orders);

    // collect agents that can act right now
    std::vector<Bot*> availableAgents;
    for (Squad &squad : m_squads) {
        for (Bot *agent : squad.getAgents()) {
            nbAgents += 1;
            if(agent->getCooldown() < game_rules::MAX_ACT_COOLDOWN)
                availableAgents.push_back(agent);
        }
    }

    m_globalBlackboard->insertData(bbn::GLOBAL_AGENTS, &nbAgents);

    // fill in the orders list through agents behavior trees
    std::for_each(availableAgents.begin(), availableAgents.end(), [this](Bot *agent) {
        agent->getBlackboard().insertData(bbn::AGENT_SELF, agent);
        agent->getBlackboard().setParentBoard(m_globalBlackboard);
        agent->act();
    });

    // not critical, but keeping dandling pointers alive is never a good idea
    m_globalBlackboard->removeData(bbn::GLOBAL_ORDERS_LIST);

    return orders;
}

