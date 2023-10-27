#include "CommandChain.h"

#include <algorithm>

#include "GameRules.h"
#include "Log.h"
#include "Pathing.h"

Commander::Commander()
  : m_globalBlackboard(std::make_shared<Blackboard>())
{
    m_squads = std::vector<Squad>();
    //m_squads.push_back(Squad{}); // to be removed, the initial implementation uses one squad only
}

void Commander::updateHighLevelObjectives(GameState *state, const GameStateDiff &diff)
{
    m_gameState = state;
    rearrangeSquads();
    if (m_squads.size() == 0) return;
    Squad &firstSquad = m_squads[0];
    firstSquad.getAgents().clear();
    for (Bot &bot : state->bots) {
      if (bot.getTeam() == Player::ALLY)
        firstSquad.getAgents().push_back(&bot);
    }
}


std::vector<TurnOrder> Commander::getTurnOrders()
{
    static size_t turnNumber = 0;
    std::vector<TurnOrder> orders;

    LOG("Turn " << turnNumber);
    turnNumber++;

    int nbAgents = 0;
    int nbWorkers = 0;
    int nbCarts = 0;
    GameState *gameState{ m_gameState };

    size_t friendlyCityCount = std::count_if(gameState->bots.begin(), gameState->bots.end(), [](Bot &bot) { return bot.getTeam() == Player::ALLY && bot.getType() == UNIT_TYPE::CITY; });
    std::vector<tileindex_t> agentsPosition;
    std::ranges::transform(gameState->bots, std::back_inserter(agentsPosition), [&gameState](Bot &bot) { return gameState->map.getTileIndex(bot); });

    m_globalBlackboard->insertData(bbn::GLOBAL_TURN, turnNumber);
    m_globalBlackboard->insertData(bbn::GLOBAL_GAME_STATE, m_gameState);
    m_globalBlackboard->insertData(bbn::GLOBAL_MAP, &m_gameState->map);
    m_globalBlackboard->insertData(bbn::GLOBAL_ORDERS_LIST, &orders);
    m_globalBlackboard->insertData(bbn::GLOBAL_TEAM_RESEARCH_POINT, m_gameState->playerResearchPoints[Player::ALLY]);
    m_globalBlackboard->insertData(bbn::GLOBAL_FRIENDLY_CITY_COUNT, friendlyCityCount);
    m_globalBlackboard->insertData(bbn::GLOBAL_AGENTS_POSITION, &agentsPosition);

    // collect agents that can act right now
    std::vector<std::pair<Bot*,Archetype>> availableAgents;
    for (Squad &squad : m_squads) {
        for (Bot *agent : squad.getAgents()) {
            if (agent->getType() == UNIT_TYPE::WORKER) {
                nbAgents += 1;
                nbWorkers += 1;
            }
            else if (agent->getType() == UNIT_TYPE::CART) {
                nbAgents += 1;
                nbCarts += 1;
            }
            if(agent->getCooldown() < game_rules::MAX_ACT_COOLDOWN)
                availableAgents.push_back(std::pair<Bot*, Archetype>(agent, squad.getArchetype()));
        }
    }

    m_globalBlackboard->insertData(bbn::GLOBAL_AGENTS, nbAgents);
    m_globalBlackboard->insertData(bbn::GLOBAL_WORKERS, nbWorkers);
    m_globalBlackboard->insertData(bbn::GLOBAL_CARTS, nbCarts);


    // fill in the orders list through agents behavior trees
    std::for_each(availableAgents.begin(), availableAgents.end(), [&,this](std::pair<Bot *, Archetype> agent) {
        tileindex_t targetTile = 0;
        BotObjective::ObjectiveType mission = BotObjective::ObjectiveType::BUILD_CITY;
        switch (agent.second) {
            case Archetype::CITIZEN:    //TODO implement CITIZEN/SETTLER difference in pathing::getBestCityBuildingLocation algorithm
            case Archetype::SETTLER: targetTile = pathing::getBestCityBuildingLocation(agent.first, &m_gameState->map); break;
            case Archetype::FARMER: mission = BotObjective::ObjectiveType::FEED_CITY; targetTile = pathing::getResourceFetchingLocation(agent.first, &m_gameState->map); break;
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

std::vector<EnemySquadInfo> Strategy::getEnemyStance()
{
    std::vector<EnemySquadInfo> enemySquads{};
    for (int arch = Archetype::CITIZEN; arch < Archetype::TROUBLEMAKER; arch++) {
        enemySquads.push_back(EnemySquadInfo{ 0,0,5,5,Archetype::ROADMAKER });
    }
    return enemySquads;
}

std::vector<SquadRequirement> Strategy::adaptToEnemy(std::vector<EnemySquadInfo> enemyStance)
{
    //sorted by priority in the end
    return std::vector<SquadRequirement>{SquadRequirement{ 1,0,1,5,5, Archetype::CITIZEN }};
}

std::vector<Squad> Strategy::createSquads(std::vector<SquadRequirement> squadRequirements, GameState* gameState) //not optimal, could do clustering to be more efficient
{
    std::vector<Squad> newSquads{};
    std::vector<Bot *> botsUnassigned{};
    LOG("Squad : ");
    for_each(gameState->bots.begin(), gameState->bots.end(), [&botsUnassigned](Bot bot) {botsUnassigned.push_back(&bot); });

    for_each(squadRequirements.begin(), squadRequirements.end(), [&botsUnassigned, &newSquads](SquadRequirement sr) 
    {
        std::vector<std::pair<Bot*, size_t>> bestBots{sr.botNb};

        //Get the sr.botNb closest bots for this SquadRequirement
        for_each(botsUnassigned.begin(), botsUnassigned.end(), [&bestBots, &sr](Bot* bot)
        {
            if (bestBots.size() < sr.botNb) bestBots.push_back(std::pair<Bot*, size_t>(bot, (size_t)(std::abs(sr.dest_x - bot->getX()) + std::abs(sr.dest_y - bot->getY()))));
            else 
            {
                size_t dist = (size_t)(std::abs(sr.dest_x - bot->getX()) + std::abs(sr.dest_y - bot->getY()));
                size_t scoreDist = bestBots[0].second;
                size_t index = 0;
                for (size_t i = 1; i < sr.botNb; i++) 
                {
                    if (bestBots[i].second < scoreDist) 
                    {
                        index = i;
                        scoreDist = bestBots[i].second;
                    }
                }
                if (dist < scoreDist) bestBots[index] = std::pair<Bot*, size_t>(bot, dist);
            }
        });
        std::vector<Bot *> nSquad{};
        for_each(bestBots.begin(), bestBots.end(), [&nSquad](std::pair<Bot*, size_t> v)
        {
            //LOG(v.first->getId() << " : " << v.first->getX() << ";" << v.first->getY());
            nSquad.push_back(v.first);
        });
        newSquads.push_back(Squad(nSquad, sr.mission));
    });
    return newSquads;
}