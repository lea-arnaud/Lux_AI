#include "CommandChain.h"

#include <algorithm>

#include "GameRules.h"
#include "Log.h"
#include "Pathing.h"

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

    LOG("Turn " << turnNumber);
    turnNumber++;

    int nbAgents = 0;
    int nbWorkers = 0;
    int nbCarts = 0;
    int nbCities = 0;

    std::vector<tileindex_t> agentsPosition;

    m_globalBlackboard->insertData(bbn::GLOBAL_TURN, turnNumber);
    m_globalBlackboard->insertData(bbn::GLOBAL_GAME_STATE, &gameState);
    m_globalBlackboard->insertData(bbn::GLOBAL_MAP, &gameState.map);
    m_globalBlackboard->insertData(bbn::GLOBAL_ORDERS_LIST, &orders);
    m_globalBlackboard->insertData(bbn::GLOBAL_TEAM_RESEARCH_POINT, gameState.playerResearchPoints[Player::ALLY]);
    m_globalBlackboard->insertData(bbn::GLOBAL_AGENTS_POSITION, &agentsPosition);

    // collect agents that can act right now
    std::vector<std::pair<Bot*,Archetype>> availableAgents;
    for (Squad &squad : m_squads) {
        for (Bot *agent : squad.getAgents()) {
            if (agent->getType() == UNIT_TYPE::CITY) {
              nbCities++;
            } else {
                agentsPosition.push_back(gameState.map.getTileIndex(*agent));
                nbAgents += 1;
                if (agent->getType() == UNIT_TYPE::WORKER)
                    nbWorkers += 1;
                else if (agent->getType() == UNIT_TYPE::CART)
                    nbCarts += 1;
            }
            if(agent->getCooldown() < game_rules::MAX_ACT_COOLDOWN)
                availableAgents.emplace_back(agent, squad.getArchetype());
        }
    }

    m_globalBlackboard->insertData(bbn::GLOBAL_AGENTS, nbAgents);
    m_globalBlackboard->insertData(bbn::GLOBAL_WORKERS, nbWorkers);
    m_globalBlackboard->insertData(bbn::GLOBAL_CARTS, nbCarts);
    m_globalBlackboard->insertData(bbn::GLOBAL_FRIENDLY_CITY_COUNT, nbCities);


    // fill in the orders list through agents behavior trees
    std::for_each(availableAgents.begin(), availableAgents.end(), [&,this](std::pair<Bot *, Archetype> agent) {
        tileindex_t targetTile = 0;
        BotObjective::ObjectiveType mission = BotObjective::ObjectiveType::BUILD_CITY;
        switch (agent.second) {
            case Archetype::CITIZEN:    //TODO implement CITIZEN/SETTLER difference in pathing::getBestCityBuildingLocation algorithm
            case Archetype::SETTLER: targetTile = pathing::getBestCityBuildingLocation(agent.first, &gameState.map); break;
            case Archetype::FARMER: mission = BotObjective::ObjectiveType::FEED_CITY; targetTile = pathing::getResourceFetchingLocation(agent.first, &gameState.map); break;
            case Archetype::TROUBLEMAKER: mission = BotObjective::ObjectiveType::GO_BLOCK_PATH; break; //TODO implement pathing algorithm to block
                //case Archetype::ROADMAKER: mission = BotObjective::ObjectiveType::BUILD_CITY; break; //TODO implement ROADMAKER cart behaviour
            default: break;
        };
        BotObjective objective{ mission, targetTile };
        (agent.first)->getBlackboard().insertData(bbn::AGENT_SELF, agent.first);
        (agent.first)->getBlackboard().insertData(bbn::AGENT_OBJECTIVE, objective);
        (agent.first)->getBlackboard().setParentBoard(m_globalBlackboard);
        (agent.first)->act();
    });

    // not critical, but keeping dandling pointers alive is never a good idea
    m_globalBlackboard->removeData(bbn::GLOBAL_ORDERS_LIST);

    return orders;
}

std::map<Archetype, size_t> Strategy::getEnemyStance()
{
    std::map<Archetype, size_t> enemySquads;
    for (int arch = Archetype::CITIZEN; arch < Archetype::TROUBLEMAKER; arch++) {
        enemySquads.emplace(std::pair<Archetype, size_t>((Archetype)arch, 0));
    }
    return enemySquads;
}