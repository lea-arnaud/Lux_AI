#include "command_chain.h"

static constexpr float MAX_ACT_COOLDOWN = 1.f; // units must have cooldown<1 to act

Commander::Commander()
{
    m_squads.push_back({}); // to be removed, the initial implementation uses one squad only
}

void Commander::updateHighLevelObjectives(const GameState &state, const GameStateDiff &diff)
{
    Squad &firstSquad = m_squads[0];
    firstSquad.getAgents().clear();
    for (const Bot &bot : state.bots)
        firstSquad.getAgents().push_back(SquadAgent(&bot));
}

std::vector<TurnOrder> Commander::getTurnOrders(const Map &map)
{
    std::vector<SquadAgent*> availableAgents;
    for (Squad &squad : m_squads) {
        for (SquadAgent &agents : squad.getAgents()) {
            if(agents.getBot().getCooldown() < MAX_ACT_COOLDOWN)
                availableAgents.push_back(&agents);
        }
    }

    // TODO collect agents that should pathfind here
    // TODO do pathfinding here

    std::vector<TurnOrder> orders;

    // TODO implement squad/bot objectives
    for (SquadAgent *agent : availableAgents) {
        TurnOrder order{};
        order.bot = &agent->getBot();
        if (agent->getBot().getType() == UNIT_TYPE::CITY) {
            order.type = TurnOrder::RESEARCH;
        } else {
            order.type = TurnOrder::MOVE;
            order.targetTile = map.getTileNeighbour(map.getTileIndex(agent->getBot()), kit::DIRECTIONS::EAST);
        }
        orders.push_back(order);
    }

    return orders;
}

