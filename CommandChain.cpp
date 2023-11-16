#include "CommandChain.h"

#include <algorithm>
#include <memory>

#include "GameRules.h"
#include "Log.h"
#include "Pathing.h"
#include "InfluenceMap.h"
#include "Types.h"
#include "lux/annotate.hpp"
#include "IAParams.h"


Commander::Commander()
  : m_globalBlackboard(std::make_shared<Blackboard>())
  , m_gameState(nullptr)
{
}

void Commander::updateHighLevelObjectives(GameState *state, const GameStateDiff &diff)
{
    m_gameState = state;
    rearrangeSquads(diff);
}


std::vector<TurnOrder> Commander::getTurnOrders()
{
    static size_t turnNumber = 0;
    std::vector<TurnOrder> orders;

    turnNumber++;

    int nbAgents = 0;
    int nbWorkers = 0;
    int nbCarts = 0;
    int nbCities = 0;

    std::vector<tileindex_t> agentsPositions;
    std::vector<tileindex_t> nonCityPositions;
    std::ranges::transform(m_gameState->bots, std::back_inserter(agentsPositions),
      [this](const auto &bot) { return m_gameState->map.getTileIndex(*bot); });
    std::ranges::for_each(m_gameState->bots,
      [&](const auto &bot) { if(bot->getType() != UNIT_TYPE::CITY) nonCityPositions.push_back(m_gameState->map.getTileIndex(*bot)); });

    // collect agents that can act right now
    std::vector<std::pair<Bot *, Archetype>> availableAgents{};
    for (Squad &squad : m_squads) {
        for (Bot *agent : squad.getAgents()) {
            nbAgents += 1;
            if (agent->getType() == UNIT_TYPE::WORKER)
                nbWorkers += 1;
            else if (agent->getType() == UNIT_TYPE::CART)
                nbCarts += 1;
            if(agent->getCooldown() < game_rules::MAX_ACT_COOLDOWN)
                availableAgents.emplace_back(agent, squad.getArchetype());
        }
    }

    for (auto &city : m_gameState->bots) {
        if (city->getType() == UNIT_TYPE::CITY && city->getTeam() == Player::ALLY) {
            nbCities++;
            if (city->getCooldown() < game_rules::MAX_ACT_COOLDOWN)
                availableAgents.emplace_back(city.get(), CITIZEN);
        }
    }

    m_globalBlackboard->insertData(bbn::GLOBAL_TURN, turnNumber);
    m_globalBlackboard->insertData(bbn::GLOBAL_GAME_STATE, m_gameState);
    m_globalBlackboard->insertData(bbn::GLOBAL_MAP, &m_gameState->map);
    m_globalBlackboard->insertData(bbn::GLOBAL_ORDERS_LIST, &orders);
    m_globalBlackboard->insertData(bbn::GLOBAL_TEAM_RESEARCH_POINT, m_gameState->playerResearchPoints[Player::ALLY]);
    m_globalBlackboard->insertData(bbn::GLOBAL_AGENTS_POSITION, &agentsPositions);
    m_globalBlackboard->insertData(bbn::GLOBAL_NONCITY_POSITION, &nonCityPositions);
    m_globalBlackboard->insertData(bbn::GLOBAL_AGENTS, nbAgents);
    m_globalBlackboard->insertData(bbn::GLOBAL_WORKERS, nbWorkers);
    m_globalBlackboard->insertData(bbn::GLOBAL_CARTS, nbCarts);
    m_globalBlackboard->insertData(bbn::GLOBAL_FRIENDLY_CITY_COUNT, nbCities);

    // fill in the orders list through agents behavior trees
    std::ranges::for_each(availableAgents, [&,this](const std::pair<Bot *, Archetype> &agentAndArchetype) {
        auto &[agent, archetype] = agentAndArchetype;
        tileindex_t targetTile = 1;
        BotObjective::ObjectiveType mission = BotObjective::ObjectiveType::BUILD_CITY;
        switch (archetype) {
        case Archetype::CITIZEN: targetTile = pathing::getBestExpansionLocation(agent, m_gameState); break;
        case Archetype::SETTLER: targetTile = pathing::getBestCityBuildingLocation(agent, m_gameState); break;
        case Archetype::FARMER: mission = BotObjective::ObjectiveType::FEED_CITY; targetTile = pathing::getBestCityFeedingLocation(agent, m_gameState); break;
        case Archetype::TROUBLEMAKER: mission = BotObjective::ObjectiveType::GO_BLOCK_PATH; break; //TODO implement pathing algorithm to block
        case Archetype::ROADMAKER: mission = BotObjective::ObjectiveType::MAKE_ROAD; break; //TODO implement pathing algorithm to resource
        default: break;
        }
        
        BotObjective objective{ mission, targetTile };
        agent->getBlackboard().insertData(bbn::AGENT_SELF, agent);
        agent->getBlackboard().insertData(bbn::AGENT_OBJECTIVE, objective);
        agent->getBlackboard().setParentBoard(m_globalBlackboard);
        agent->act();
    });

    // not critical, but keeping dandling pointers alive is never a good idea
    m_globalBlackboard->removeData(bbn::GLOBAL_ORDERS_LIST);

    return orders;
}

bool Commander::shouldUpdateSquads(const GameStateDiff &diff, const std::vector<EnemySquadInfo> &newEnemyStance)
{
  if (std::ranges::any_of(diff.deadBots, [](auto &deadBot) { return deadBot->getTeam() == Player::ALLY; }))
    return true;
  if (std::ranges::any_of(diff.newBots, [](auto &newBot) { return newBot->getTeam() == Player::ALLY; }))
    return true;
  return false;
}

void Commander::rearrangeSquads(const GameStateDiff &diff)
{
    auto enemyStance = currentStrategy.getEnemyStance(*m_gameState);
    if(m_previousEnemyStance.empty() || shouldUpdateSquads(diff, enemyStance)) {
      lux::Annotate::sidetext("! Updating squads");
      auto stanceToTake = currentStrategy.adaptToEnemy(enemyStance, *m_gameState);
      m_squads = currentStrategy.createSquads(stanceToTake, m_gameState);
      m_previousEnemyStance = std::move(enemyStance);
    }
}

std::array<std::vector<CityCluster>, Player::COUNT> Strategy::getCityClusters(const GameState &gameState)
{
    constexpr float maxClusterRadius = 5.f;
    std::array<std::vector<CityCluster>, Player::COUNT> clusters;

    for(const std::unique_ptr<Bot> &bot : gameState.bots) {
      if (bot->getType() != UNIT_TYPE::CITY)
        continue;
      const Bot &cityTile = *bot;
      auto &botFriendlyClusters = clusters[cityTile.getTeam()];

      auto distanceToCluster = [&](CityCluster cluster) { return std::abs(cluster.center_x - (float)cityTile.getX()) + std::abs(cluster.center_y - (float)cityTile.getY()); };
      auto nearestCluster = std::ranges::min_element(botFriendlyClusters, std::less{}, distanceToCluster);

      if(nearestCluster == botFriendlyClusters.end() || distanceToCluster(*nearestCluster) > maxClusterRadius) {
        botFriendlyClusters.emplace_back((float)cityTile.getX(), (float)cityTile.getY(), 1);
      } else {
        auto &c = *nearestCluster;
        c.center_x = (c.center_x * (float)c.cityTileCount + (float)cityTile.getX()) / ((float)c.cityTileCount + 1);
        c.center_x = (c.center_y * (float)c.cityTileCount + (float)cityTile.getY()) / ((float)c.cityTileCount + 1);
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

    std::vector<std::string> seenIds{}; 

    for(const std::unique_ptr<Bot> &bot : gameState.bots) {
      // We don't look at enemies nor cities
      if (bot->getType() == UNIT_TYPE::CITY || bot->getTeam() != Player::ENEMY)
        continue;

      // We don't look at already assigned bots
      if (std::find(seenIds.begin(), seenIds.end(), bot->getId()) != seenIds.end())
        continue;

      auto distanceToCluster = [&](const CityCluster &cluster) { return std::abs(cluster.center_x - (float)bot->getX()) + std::abs(cluster.center_y - (float)bot->getY()); };

      // get bot path
      auto path = gameState.ennemyPath.at(bot->getId());

      EnemySquadInfo enemySquadInfo{ bot->getType() == UNIT_TYPE::WORKER ? (unsigned int)1 : (unsigned int)0, bot->getType() == UNIT_TYPE::CART ? (unsigned int)1 : (unsigned int)0 ,path,Archetype::CITIZEN };

      // compare with not already assigned bots
      for(const std::unique_ptr<Bot> &bot2 : gameState.bots)
      {
          // We don't look at enemies nor cities
          if (bot->getType() == UNIT_TYPE::CITY || bot->getTeam() != Player::ENEMY)
              continue;
          // We don't look at already assigned bots
          if (std::find(seenIds.begin(), seenIds.end(), bot->getId()) != seenIds.end())
              continue;
          if (!gameState.ennemyPath.contains(bot2->getId())) continue;
          auto path2 = gameState.ennemyPath.at(bot2->getId());
          // We check of paths are similar -> TODO propagate maps to avoid not registering bots side by side as same squad
          if (path.getSimilarity(path2, params::similarityTolerance) >= params::similarPercentage) {
            seenIds.push_back(bot2->getId());
            if (bot->getType() == UNIT_TYPE::WORKER)
                enemySquadInfo.botNb++;
            else
                enemySquadInfo.cartNb++;
          }
      }

      // guess the squad's current objective
      // TODO propagate path
      // enemyCities is an InfluenceMap created to facilitate operations with path
      InfluenceMap enemyCities{gameState.citiesInfluence};
      enemyCities.clear();
      for_each(cityClusters[1].begin(), cityClusters[1].end(), [&enemyCities](CityCluster cc) {enemyCities.addTemplateAtIndex(enemyCities.getIndex(cc.center_x, cc.center_y),clusterTemplate); });
      
      // if bot's path covers a resource point AND an enemy city, he's either expanding the city or fueling it (same movement pattern, in fact)
      if (path.covers(gameState.resourcesInfluence, params::resourceCoverageNeeded) && path.covers(enemyCities, params::cityCoverageNeeded)) 
      {
          enemySquadInfo.mission = Archetype::FARMER; // Or CITIZEN, really
          InfluenceMap startCheck{ path };
          startCheck.multiplyMap(enemyCities);
          // start is the enemy city
          auto [start_x, start_y] = startCheck.getCoord(startCheck.getHighestPoint());
          enemySquadInfo.start_x = start_x;
          enemySquadInfo.start_y = start_y;
          InfluenceMap endCheck{ path };
          endCheck.multiplyMap(gameState.resourcesInfluence);
          // end is the resource point
          auto [end_x, end_y] = endCheck.getCoord(endCheck.getHighestPoint());
          enemySquadInfo.dest_x = end_x;
          enemySquadInfo.dest_y = end_y;
      }
      else // if we see a bot approaching one of our cities in "kind of a straight line" (depending on pathStep), he's hostile :
          // if they are many, they probably want to kill a city and/or units => KILLER
          // if they are less, they probably just want to block a path or something => TROUBLEMAKER
      {
        bool movingTowardsAllyCity = false;
        CityCluster targetedCity{};
        // we seek if a city is targeted
        for (CityCluster cc : cityClusters[0])
        {
            if (path.approachesPoint(cc.center_x, cc.center_y, params::ennemyPathingTurn, params::pathStep))
            {
                movingTowardsAllyCity = true;
                targetedCity = cc;
                break;
            }
        }
        if (movingTowardsAllyCity) {
            if (enemySquadInfo.botNb + enemySquadInfo.cartNb >= 3)
                enemySquadInfo.mission = Archetype::KILLER;
            else
                enemySquadInfo.mission = Archetype::TROUBLEMAKER;
            enemySquadInfo.dest_x = targetedCity.center_x;
            enemySquadInfo.dest_y = targetedCity.center_y;

        } else 
            //TODO more than this, please : especially on settler's info (destination prediction needed)
            if (enemySquadInfo.botNb == 0 && enemySquadInfo.cartNb != 0)
                enemySquadInfo.mission = Archetype::ROADMAKER;
            else
                enemySquadInfo.mission = Archetype::SETTLER;
      }

      enemySquads.push_back(enemySquadInfo);
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
    std::ranges::for_each(gameState.bots, [&availableBots, &availableCarts](const std::unique_ptr<Bot> &bot) {
        if (bot->getTeam() != Player::ALLY) return;
        if (bot->getType() == UNIT_TYPE::CART) availableCarts++;
        else if (bot->getType() == UNIT_TYPE::WORKER) availableBots++;
    });

    //TODO replace with gameState->cities ??
    const auto &clusters = getCityClusters(gameState);
    const std::vector<CityCluster> &allyCities = clusters[Player::ALLY];
    const std::vector<CityCluster> &enemyCities = clusters[Player::ENEMY];

    //First citizen
    squadRequirements.emplace_back(1,0,0,Archetype::CITIZEN);
    availableBots--;

    //Sustain
    for(size_t i = 0; i < allyCities.size() && availableBots > 0; i++) {
        auto &cc = allyCities[i];
        SquadRequirement sr{ 1,0,1,Archetype::FARMER };
        sr.dest_x = cc.center_x; sr.dest_y = cc.center_y;
        squadRequirements.emplace_back(sr);
        availableBots--;
    }

    std::map<Archetype, size_t> priorities{};
    priorities.emplace(Archetype::CITIZEN, 2);
    priorities.emplace(Archetype::FARMER, 2);
    priorities.emplace(Archetype::KILLER, 2);
    priorities.emplace(Archetype::ROADMAKER, 2);
    priorities.emplace(Archetype::SETTLER, 2);
    priorities.emplace(Archetype::TROUBLEMAKER, 2);


    //Retaliation
    while ((availableBots > 0 || availableCarts > 0) && !info.empty()) {
        const EnemySquadInfo &enemySquad = info.back();
        SquadRequirement sr{ 0,0,0,SETTLER };
        switch (enemySquad.mission) {
        case ROADMAKER:
            sr.botNb = 1;
            sr.cartNb = 0;
            sr.priority = priorities[ROADMAKER];
            sr.mission = TROUBLEMAKER;
            sr.setDestination(enemySquad.path.getRandomValuedPoint());
            squadRequirements.emplace_back(sr);
            priorities.emplace(ROADMAKER, priorities[ROADMAKER]+1);
            availableBots--;
            break; //ROADMAKER blocked by TROUBLEMAKER
        case SETTLER:
            sr.botNb = 1;
            sr.cartNb = 0;
            sr.priority = priorities[SETTLER];
            sr.mission = SETTLER;
            // TODO define destination in createSquads to use bots directly (can't get best location without base position
            squadRequirements.emplace_back(sr);
            priorities.emplace(SETTLER, priorities[SETTLER]+1);
            availableBots--;
            break; //SETTLER blocked by SETTLER : don't go in their direction
        case KILLER:
            sr.botNb = 1;
            sr.cartNb = 0;
            sr.priority = priorities[SETTLER];
            sr.mission = SETTLER;
            squadRequirements.emplace_back(sr);
            priorities.emplace(SETTLER, priorities[SETTLER]+1);
            availableBots--;
            break; //KILLER blocked by SETTLER : get out of the city !
        default:
            break;
        }
        info.pop_back();
    }

    size_t selfPriority = 10;

    //Self-organization
    while (availableBots > 0 || availableCarts > 0) {
        SquadRequirement srFarmer{ static_cast<size_t>(std::min(availableBots, 3)),static_cast<size_t>(std::min(availableCarts, 1)),selfPriority,FARMER };
        squadRequirements.emplace_back(srFarmer);
        availableBots -= std::min(availableBots, 3);
        availableCarts--;
        selfPriority++;
        SquadRequirement srCitizen{ static_cast<size_t>(std::min(availableBots, 1)),0,selfPriority,CITIZEN };
        squadRequirements.emplace_back(srCitizen);
        availableBots -= std::min(availableBots, 3);
        availableCarts--;
    }

    std::ranges::sort(squadRequirements, std::less{}, [](auto &sr) { return sr.priority; });
    return squadRequirements;
}


std::vector<Squad> Strategy::createSquads(const std::vector<SquadRequirement> &squadRequirements, GameState* gameState) //not optimal, could do clustering to be more efficient
{
    std::vector<Squad> newSquads{};
    std::vector<Bot *> botsAssigned{};
    std::vector<std::pair<Bot *, size_t>> bestBots{};

    for(const SquadRequirement &sr : squadRequirements) {
        //Get the sr.botNb closest bots for this SquadRequirement
        bestBots.clear();
        for (const std::unique_ptr<Bot> &bot : gameState->bots) {
          if (bot->getTeam() != Player::ALLY || (bot->getType() == UNIT_TYPE::CITY) || std::ranges::find(botsAssigned, bot.get()) != botsAssigned.end())
              continue;
          if (bestBots.size() < sr.botNb) {
              bestBots.emplace_back(bot.get(), (size_t)(std::abs((int)sr.dest_x - bot->getX()) + std::abs((int)sr.dest_y - bot->getY())));
          } else {
              size_t dist = std::abs((int)sr.dest_x - bot->getX()) + std::abs((int)sr.dest_y - bot->getY());
              size_t scoreDist = bestBots[0].second;
              size_t index = 0;
              for (size_t i = 1; i < sr.botNb; i++) {
                  if (bestBots[i].second < scoreDist) {
                      index = i;
                      scoreDist = bestBots[i].second;
                  }
              }
              if (dist < scoreDist) bestBots[index] = { bot.get(), dist };
          }
        }
        std::vector<Bot *> nSquad{};
        std::ranges::transform(bestBots, std::back_inserter(botsAssigned), [](auto &v) { return v.first; });
        std::ranges::transform(bestBots, std::back_inserter(nSquad), [](auto &v) { return v.first; });
        newSquads.emplace_back(std::move(nSquad), sr.mission);
    }
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
