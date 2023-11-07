#include "CommandChain.h"

#include <algorithm>

#include "GameRules.h"
#include "Log.h"
#include "Pathing.h"
#include "InfluenceMap.h"
#include "Types.h"
#include <memory>


Commander::Commander()
  : m_globalBlackboard(std::make_shared<Blackboard>())
  , m_gameState(nullptr)
{
    m_squads = std::vector<Squad>();
    //m_squads.push_back(Squad{}); // to be removed, the initial implementation uses one squad only
}

void Commander::updateHighLevelObjectives(GameState *state, const GameStateDiff &diff)
{
    m_gameState = state;
    rearrangeSquads();
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
    int nbCities = 0;

    std::vector<tileindex_t> agentsPosition;
    std::ranges::transform(m_gameState->bots, std::back_inserter(agentsPosition), [this](const Bot &bot) { return m_gameState->map.getTileIndex(bot); });

    m_globalBlackboard->insertData(bbn::GLOBAL_TURN, turnNumber);
    m_globalBlackboard->insertData(bbn::GLOBAL_GAME_STATE, m_gameState);
    m_globalBlackboard->insertData(bbn::GLOBAL_MAP, &m_gameState->map);
    m_globalBlackboard->insertData(bbn::GLOBAL_ORDERS_LIST, &orders);
    m_globalBlackboard->insertData(bbn::GLOBAL_TEAM_RESEARCH_POINT, m_gameState->playerResearchPoints[Player::ALLY]);
    m_globalBlackboard->insertData(bbn::GLOBAL_AGENTS_POSITION, &agentsPosition);

    // collect agents that can act right now
    std::vector<std::pair<Bot *, Archetype>> availableAgents{};
    for (Squad &squad : m_squads) {
        for (Bot *agent : squad.getAgents()) {
            if (agent->getType() == UNIT_TYPE::CITY) {
              nbCities++;
            } else {
                agentsPosition.push_back(m_gameState->map.getTileIndex(*agent));
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

    for (Bot &city : m_gameState->bots) {
        if (city.getType() == UNIT_TYPE::CITY) {
            nbCities++;
            std::pair c{ &city, CITIZEN };
            if (city.getCooldown() < game_rules::MAX_ACT_COOLDOWN)
                availableAgents.push_back(c);
        }
    }

    m_globalBlackboard->insertData(bbn::GLOBAL_AGENTS, nbAgents);
    m_globalBlackboard->insertData(bbn::GLOBAL_WORKERS, nbWorkers);
    m_globalBlackboard->insertData(bbn::GLOBAL_CARTS, nbCarts);
    m_globalBlackboard->insertData(bbn::GLOBAL_FRIENDLY_CITY_COUNT, nbCities);

    m_gameState->map.computeInfluence();

    // fill in the orders list through agents behavior trees
    std::for_each(availableAgents.begin(), availableAgents.end(), [&,this](std::pair<Bot *, Archetype> agent) {
        tileindex_t targetTile = 1;
        BotObjective::ObjectiveType mission = BotObjective::ObjectiveType::BUILD_CITY;
        switch (agent.second) {
        case Archetype::CITIZEN: targetTile = pathing::getBestExpansionLocation(agent.first, &m_gameState->map); break;
            //TODO fix external symbol error : case Archetype::SETTLER: targetTile = pathing::getBestCityBuildingLocation(agent.first, &m_gameState->map); break;
            case Archetype::FARMER: mission = BotObjective::ObjectiveType::FEED_CITY; targetTile = pathing::getResourceFetchingLocation(agent.first, &m_gameState->map); break;
            case Archetype::TROUBLEMAKER: mission = BotObjective::ObjectiveType::GO_BLOCK_PATH; break; //TODO implement pathing algorithm to block
                //case Archetype::ROADMAKER: mission = BotObjective::ObjectiveType::BUILD_CITY; break; //TODO implement ROADMAKER cart behaviour
            default: break;
        }
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

void Commander::rearrangeSquads()
{
    auto enemyStance = currentStrategy.getEnemyStance(*m_gameState);
    auto stanceToTake = currentStrategy.adaptToEnemy(enemyStance, *m_gameState);
    m_squads = currentStrategy.createSquads(stanceToTake, m_gameState);
}

std::array<std::vector<CityCluster>, Player::COUNT> Strategy::getCityClusters(const GameState &gameState)
{
    constexpr float maxClusterRadius = 5.f;
    std::array<std::vector<CityCluster>, Player::COUNT> clusters;

    for(const Bot &bot : gameState.bots) {
      if (bot.getType() != UNIT_TYPE::CITY)
        continue;
      const Bot &cityTile = bot;
      auto botFriendlyClusters = clusters[bot.getTeam()];

      auto distanceToCluster = [&](CityCluster cluster) { return std::abs(cluster.center_x - (float)cityTile.getX()) + std::abs(cluster.center_y - (float)cityTile.getY()); };
      auto nearestCluster = std::ranges::min_element(botFriendlyClusters, std::less{}, distanceToCluster);

      if(nearestCluster == botFriendlyClusters.end() || distanceToCluster(*nearestCluster) > maxClusterRadius) {
        botFriendlyClusters.emplace_back((float)bot.getX(), (float)bot.getY(), 1);
      } else {
        auto &c = *nearestCluster;
        c.center_x = (c.center_x * (float)c.cityTileCount + (float)bot.getX()) / ((float)c.cityTileCount + 1);
        c.center_x = (c.center_y * (float)c.cityTileCount + (float)bot.getY()) / ((float)c.cityTileCount + 1);
        c.cityTileCount++;
      }
    }

    return clusters;
}

std::vector<EnemySquadInfo> Strategy::getEnemyStance(const GameState &gameState)
{
    constexpr int maxSquadSpan = 6;

    std::vector<EnemySquadInfo> enemySquads;

    auto cityClusters = getCityClusters(gameState);

    { // remove small enemy clusters (ongoing expansions)
      auto &enemyClusters = cityClusters[Player::ENEMY];
      std::erase_if(enemyClusters, [](auto &cluster) { return cluster.cityTileCount < 3; });
    }

    for(const Bot &bot : gameState.bots) {
      if (bot.getType() == UNIT_TYPE::CITY || bot.getTeam() != Player::ENEMY)
        continue;

      auto distanceToCluster = [&](const CityCluster &cluster) { return std::abs(cluster.center_x - (float)bot.getX()) + std::abs(cluster.center_y - (float)bot.getY()); };

      // poll stats about the bot
      float distanceToAllyCity = cityClusters[Player::ALLY].empty() ? std::numeric_limits<float>::max() :
        distanceToCluster(*std::ranges::min_element(cityClusters[Player::ALLY], std::less{}, distanceToCluster));
      float distanceToEnemyCity = cityClusters[Player::ENEMY].empty() ? std::numeric_limits<float>::max() :
        distanceToCluster(*std::ranges::min_element(cityClusters[Player::ENEMY], std::less{}, distanceToCluster));
      int distanceToResources = getDistanceToClosestResource(bot.getX(), bot.getY(), gameState.map);

      // guess the bot's current objective
      Archetype botMission;
      if (distanceToResources < 5 && distanceToEnemyCity < 5)
        botMission = Archetype::CITIZEN;
      else if (distanceToEnemyCity < 5)
        botMission = Archetype::FARMER;
      else if (distanceToAllyCity < 5)
        botMission = Archetype::TROUBLEMAKER;
      else if (bot.getType() == UNIT_TYPE::CART)
        botMission = Archetype::ROADMAKER;
      else
        // it is quite hard to differenciate settlers and trouble makers when they come close to
        // resource clusters that we already captured, by default they'll be considered settlers
        botMission = Archetype::SETTLER;

      // find the nearest enemy squad with the same objective
      auto distanceToSquad = [&](const EnemySquadInfo &squad) { return squad.mission == botMission 
        ? std::abs(squad.pos_x - bot.getX()) + std::abs(squad.pos_y - bot.getY()) : std::numeric_limits<int>::max(); };
      auto nearestSimilarEnemySquad = std::ranges::min_element(enemySquads, std::less{}, distanceToSquad);
      
      // assign the bot to that squad, or create a new squad for them
      int additionalWorkerCount = bot.getType() == UNIT_TYPE::WORKER ? 1 : 0;
      int additionalCartCount = bot.getType() == UNIT_TYPE::CART ? 1 : 0;
      if(nearestSimilarEnemySquad == enemySquads.end() || distanceToSquad(*nearestSimilarEnemySquad) > maxSquadSpan) {
        enemySquads.emplace_back(additionalWorkerCount, additionalCartCount, bot.getX(), bot.getY(), botMission);
      } else {
        nearestSimilarEnemySquad->botNb += additionalWorkerCount;
        nearestSimilarEnemySquad->cartNb += additionalCartCount;
      }
    }

    return enemySquads;
}

//TODO : set and use positions in SquadRequirement
std::vector<SquadRequirement> Strategy::adaptToEnemy(const std::vector<EnemySquadInfo> &enemyStance, const GameState &gameState)
{
    std::vector<SquadRequirement> squadRequirements{};

    std::vector<EnemySquadInfo> info{ enemyStance };

    int availableBots = 0;
    int availableCarts = 0;
    for_each(gameState.bots.begin(), gameState.bots.end(), [&availableBots, &availableCarts](const Bot bot)
    {
        if (bot.getTeam() == Player::ALLY) {
            if (bot.getType() == UNIT_TYPE::CART) availableCarts++;
            else if (bot.getType() == UNIT_TYPE::WORKER) availableBots++;
        }
    });

    //TODO replace with gameState->cities ??
    auto clusters = getCityClusters(gameState);
    std::vector<CityCluster> allyCities = clusters[0];
    std::vector<CityCluster> enemyCities = clusters[1];

    //First citizen
    squadRequirements.push_back(SquadRequirement{ 1,0,0,10,10,Archetype::CITIZEN });
    availableBots--;

    //Sustain
    for_each(allyCities.begin(), allyCities.end(), [&squadRequirements, &availableBots](CityCluster cc) {
        if (availableBots > 0)
        {
            squadRequirements.push_back(SquadRequirement{ 1,0,1,(int)cc.center_x, (int)cc.center_y, Archetype::FARMER });
            availableBots--;
        }
    });

    std::map<Archetype, size_t> priorities{};
    priorities.emplace(std::pair<Archetype, size_t>(Archetype::CITIZEN, 2));
    priorities.emplace(std::pair<Archetype, size_t>(Archetype::FARMER, 2));
    priorities.emplace(std::pair<Archetype, size_t>(Archetype::KILLER, 2));
    priorities.emplace(std::pair<Archetype, size_t>(Archetype::ROADMAKER, 2));
    priorities.emplace(std::pair<Archetype, size_t>(Archetype::SETTLER, 2));
    priorities.emplace(std::pair<Archetype, size_t>(Archetype::TROUBLEMAKER, 2));


    //Retaliation
    while ((availableBots > 0 || availableCarts > 0) && !info.empty())
    {
        EnemySquadInfo enemySquad = info[0];
        switch (enemySquad.mission) {
        case ROADMAKER:
            squadRequirements.push_back(SquadRequirement{ 1,0,priorities[ROADMAKER],enemySquad.pos_x,enemySquad.pos_y,TROUBLEMAKER });
            priorities.emplace(ROADMAKER, priorities[ROADMAKER]+1);
            availableBots--;
            break; //ROADMAKER blocked by TROUBLEMAKER
        case SETTLER:
            squadRequirements.push_back(SquadRequirement{ 1,0,priorities[SETTLER],enemySquad.pos_x,enemySquad.pos_y,SETTLER });
            priorities.emplace(SETTLER, priorities[SETTLER]+1);
            availableBots--;
            break; //SETTLER blocked by SETTLER : don't go in their direction
        case KILLER:
            squadRequirements.push_back(SquadRequirement{ 1,0,priorities[SETTLER],enemySquad.pos_x,enemySquad.pos_y,SETTLER });
            priorities.emplace(SETTLER, priorities[SETTLER]+1);
            availableBots--;
            break; //KILLER blocked by SETTLER : get out of the city !
        default: break;
        }
        info.pop_back();
    }

    size_t selfPriority = 10;

    //Self-organization
    while ((availableBots > 0 || availableCarts > 0))
    {
        squadRequirements.push_back(SquadRequirement{ static_cast<size_t>(std::min(availableBots, 3)),static_cast<size_t>(std::min(availableCarts, 1)),selfPriority,0,0,FARMER });
        availableBots -= std::min(availableBots, 3);
        availableCarts--;
        selfPriority++;
        squadRequirements.push_back(SquadRequirement{ static_cast<size_t>(std::min(availableBots, 1)),0,selfPriority,0,0,CITIZEN });
        availableBots -= std::min(availableBots, 3);
        availableCarts--;
    }

    std::sort(squadRequirements.begin(), squadRequirements.end(), [](SquadRequirement sr1, SquadRequirement sr2)->bool { return sr1.priority < sr2.priority; });
    return squadRequirements;
}


std::vector<Squad> Strategy::createSquads(const std::vector<SquadRequirement> &squadRequirements, GameState* gameState) //not optimal, could do clustering to be more efficient
{
    std::vector<Squad> newSquads{};
    std::vector<Bot *> botsAssigned{};
    std::vector<std::pair<Bot *, size_t>> bestBots{};

    for_each(squadRequirements.begin(), squadRequirements.end(), [&gameState, &bestBots, &botsAssigned, &newSquads](const SquadRequirement &sr) 
    {
        //Get the sr.botNb closest bots for this SquadRequirement
        bestBots.clear();
        for_each(gameState->bots.begin(), gameState->bots.end(), [&botsAssigned, &bestBots, &sr](Bot &bot)
        {
            if (bot.getTeam() == Player::ALLY && (bot.getType() != UNIT_TYPE::CITY) && (std::find(botsAssigned.begin(), botsAssigned.end(), &bot) == botsAssigned.end())) {
                if (bestBots.size() < sr.botNb) bestBots.emplace_back(&bot, (size_t)(std::abs(sr.dest_x - bot.getX()) + std::abs(sr.dest_y - bot.getY())));
                else {
                    size_t dist = std::abs(sr.dest_x - bot.getX()) + std::abs(sr.dest_y - bot.getY());
                    size_t scoreDist = bestBots[0].second;
                    size_t index = 0;
                    for (size_t i = 1; i < sr.botNb; i++) {
                        if (bestBots[i].second < scoreDist) {
                            index = i;
                            scoreDist = bestBots[i].second;
                        }
                    }
                    if (dist < scoreDist) bestBots[index] = std::pair<Bot *, size_t>(&bot, dist);
                }
            }
        });
        std::vector<Bot *> nSquad{};
        std::ranges::transform(bestBots, std::back_inserter(botsAssigned), [](auto &v) { return v.first; });
        std::ranges::transform(bestBots, std::back_inserter(nSquad), [](auto &v) { return v.first; });
        newSquads.emplace_back(std::move(nSquad), sr.mission);
    });
    return newSquads;
}

int Strategy::getDistanceToClosestResource(int x, int y, const Map &map)
{
  // TODO this should be converted into an influence map for O(1) lookup
  int dist = std::numeric_limits<int>::max();
  for(int i = 0; i < map.getWidth(); i++) {
    for(int j = 0; j < map.getHeight(); j++) {
      if (map.tileAt(i, j).getType() != TileType::RESOURCE)
        continue;
      dist = std::min(dist, std::abs(x-i) + std::abs(y-j));
    }
  }
  return dist;
}
