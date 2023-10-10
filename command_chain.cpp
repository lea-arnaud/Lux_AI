#include "command_chain.h"

#include <algorithm>

#include "game_rules.h"
#include "log.h"

Commander::Commander()
{
    m_squads.push_back({}); // to be removed, the initial implementation uses one squad only
}

void Commander::updateHighLevelObjectives(GameState &state, const GameStateDiff &diff)
{
    Squad &firstSquad = m_squads[0];
    firstSquad.getAgents().resize(state.bots.size());
    std::transform(state.bots.begin(), state.bots.end(), firstSquad.getAgents().begin(), [](auto &bot) { return &bot; });
}

std::vector<TurnOrder> Commander::getTurnOrders(const Map &map)
{
    static int turnNumber = 0;
    LOG("Turn " << ++turnNumber);

    std::vector<TurnOrder> orders;
    m_globalBlackboard.insertData(bbn::GLOBAL_MAP, &map);
    m_globalBlackboard.insertData(bbn::GLOBAL_ORDERS_LIST, &orders);

    // collect agents that can act right now
    std::vector<Bot*> availableAgents;
    for (Squad &squad : m_squads) {
        for (Bot *agent : squad.getAgents()) {
            if(agent->getCooldown() < game_rules::MAX_ACT_COOLDOWN)
                availableAgents.push_back(agent);
        }
    }

    // TODO restore when the behavior trees are restored
    //std::for_each(availableAgents.begin(), availableAgents.end(),
    //  [](Bot *agent) { agent->act(); });

    // not critical, but keeping dandling pointers alive is never a good idea
    m_globalBlackboard.removeData(bbn::GLOBAL_ORDERS_LIST);

    return orders;
}

