#include "CommandChain.h"

#include <algorithm>

#include "GameRules.h"
#include "Log.h"

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


std::vector<TurnOrder> Commander::getTurnOrders(GameState &gameState)
{
    static size_t turnNumber = 0;
    std::vector<TurnOrder> orders;

    turnNumber++;

    LOG("Turn " << turnNumber);

    int nbAgents = 0;
    size_t friendlyCityCount = std::count_if(gameState.bots.begin(), gameState.bots.end(), [](Bot &bot) { return bot.getTeam() == Player::ALLY && bot.getType() == UNIT_TYPE::CITY; });

    m_globalBlackboard->insertData(bbn::GLOBAL_TURN, turnNumber);
    m_globalBlackboard->insertData(bbn::GLOBAL_GAME_STATE, &gameState);
    m_globalBlackboard->insertData(bbn::GLOBAL_MAP, &gameState.map);
    m_globalBlackboard->insertData(bbn::GLOBAL_ORDERS_LIST, &orders);
    m_globalBlackboard->insertData(bbn::GLOBAL_TEAM_RESEARCH_POINT, gameState.playerResearchPoints[Player::ALLY]);
    m_globalBlackboard->insertData(bbn::GLOBAL_FRIENDLY_CITY_COUNT, friendlyCityCount);

    // collect agents that can act right now
    std::vector<Bot*> availableAgents;
    for (Squad &squad : m_squads) {
        for (Bot *agent : squad.getAgents()) {
            if (agent->getType() != UNIT_TYPE::CITY)
                nbAgents += 1;
            if(agent->getCooldown() < game_rules::MAX_ACT_COOLDOWN)
                availableAgents.push_back(agent);
        }
    }

    m_globalBlackboard->insertData(bbn::GLOBAL_AGENTS, nbAgents);

    // fill in the orders list through agents behavior trees
    std::for_each(availableAgents.begin(), availableAgents.end(), [this](Bot *agent) {
        BotObjective objective{ BotObjective::ObjectiveType::BUILD_CITY, 0/*TODO find the best construction tile*/ };
        agent->getBlackboard().insertData(bbn::AGENT_SELF, agent);
        agent->getBlackboard().insertData(bbn::AGENT_OBJECTIVE, objective);
        agent->getBlackboard().setParentBoard(m_globalBlackboard);
        agent->act();
    });

    // not critical, but keeping dandling pointers alive is never a good idea
    m_globalBlackboard->removeData(bbn::GLOBAL_ORDERS_LIST);

    return orders;
}

