#include "CommandChain.h"

#include <algorithm>
#include <memory>
#include <queue>
#include <unordered_set>

#include "GameRules.h"
#include "Log.h"
#include "Pathing.h"
#include "InfluenceMap.h"
#include "Types.h"
#include "lux/annotate.hpp"
#include "IAParams.h"
#include "BehaviorTreeNames.h"


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
      [&](const auto &bot) { if(bot->getType() != UnitType::CITY) nonCityPositions.push_back(m_gameState->map.getTileIndex(*bot)); });

    // collect agents that can act right now
    std::vector<std::pair<Bot *, Archetype>> availableAgents{};
    for (Squad &squad : m_squads) {
        for (Bot *agent : squad.getAgents()) {
            nbAgents += 1;
            if (agent->getType() == UnitType::WORKER)
                nbWorkers += 1;
            else if (agent->getType() == UnitType::CART)
                nbCarts += 1;
            if(agent->getCooldown() < game_rules::MAX_ACT_COOLDOWN)
                availableAgents.emplace_back(agent, squad.getArchetype());
        }
    }

    for (auto &city : m_gameState->bots) {
        if (city->getType() == UnitType::CITY && city->getTeam() == Player::ALLY) {
            nbCities++;
            if (city->getCooldown() < game_rules::MAX_ACT_COOLDOWN)
                availableAgents.emplace_back(city.get(), Archetype::CITIZEN);
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
  // update squads if...
  // a friendly bot died
  if (std::ranges::any_of(diff.deadBots, [](auto &deadBot) { return deadBot->getTeam() == Player::ALLY; }))
    return true;
  // a new friendly bot was created
  if (std::ranges::any_of(diff.newBots, [](auto &newBot) { return newBot->getTeam() == Player::ALLY; }))
    return true;
  // a squad cannot fullfill its objective
  if (std::ranges::any_of(m_squads, [&](auto &squad) -> bool {
    switch(squad.getArchetype()) {
    case Archetype::FARMER:  return m_gameState->map.tileAt(squad.getTargetTile()).getType() != TileType::ALLY_CITY;
    case Archetype::SETTLER: return m_gameState->map.tileAt(squad.getTargetTile()).getType() != TileType::EMPTY;
    case Archetype::CITIZEN: return m_gameState->map.tileAt(squad.getTargetTile()).getType() != TileType::EMPTY;
    default: break;
    }
    return false;
  }))
    return true;
  
  // otherwise don't update squads
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
    struct RawCityCluster {
      float centerX;
      float centerY;
      size_t cityTileCount;
    };

    constexpr float maxClusterRadius = 5.f;
    std::array<std::vector<RawCityCluster>, Player::COUNT> clusters;

    for(const std::unique_ptr<Bot> &bot : gameState.bots) {
      if (bot->getType() != UnitType::CITY)
        continue;
      const Bot &cityTile = *bot;
      auto &botFriendlyClusters = clusters[cityTile.getTeam()];

      auto distanceToCluster = [&](RawCityCluster cluster) { return std::abs(cluster.centerX - (float)cityTile.getX()) + std::abs(cluster.centerY - (float)cityTile.getY()); };
      auto nearestCluster = std::ranges::min_element(botFriendlyClusters, std::less{}, distanceToCluster);

      if(nearestCluster == botFriendlyClusters.end() || distanceToCluster(*nearestCluster) > maxClusterRadius) {
        botFriendlyClusters.emplace_back((float)cityTile.getX(), (float)cityTile.getY(), 1);
      } else {
        auto &c = *nearestCluster;
        c.centerX = (c.centerX * (float)c.cityTileCount + (float)cityTile.getX()) / ((float)c.cityTileCount + 1);
        c.centerY = (c.centerY * (float)c.cityTileCount + (float)cityTile.getY()) / ((float)c.cityTileCount + 1);
        c.cityTileCount++;
      }
    }

    std::array<std::vector<CityCluster>, Player::COUNT> finalClusters;
    for(size_t i = 0; i < Player::COUNT; i++) {
      finalClusters[i].reserve(clusters[i].size());
      std::ranges::transform(clusters[i], std::back_inserter(finalClusters[i]), [](auto &rawCluster) {
        return CityCluster{ static_cast<int>(rawCluster.centerX), static_cast<int>(rawCluster.centerY), rawCluster.cityTileCount };
      });
    }
    return finalClusters;
}

std::vector<EnemySquadInfo> Strategy::getEnemyStance(const GameState &gameState)
{
    std::vector<EnemySquadInfo> enemySquads;

    auto cityClusters = getCityClusters(gameState);

    { // remove small enemy clusters (ongoing expansions)
      auto &enemyClusters = cityClusters[Player::ENEMY];
      std::erase_if(enemyClusters, [](auto &cluster) { return cluster.cityTileCount < 3; });
    }

    std::vector<std::string> seenIds{}; 

    for(const std::unique_ptr<Bot> &bot : gameState.bots) {
      // We don't look at enemies nor cities
      if (bot->getType() == UnitType::CITY || bot->getTeam() != Player::ENEMY)
        continue;

      // We don't look at already assigned bots
      if (std::ranges::find(seenIds, bot->getId()) != seenIds.end())
        continue;

      // get bot path
      auto &path = gameState.ennemyPath.at(bot->getId());

      EnemySquadInfo enemySquadInfo{ bot->getType() == UnitType::WORKER ? (unsigned int)1 : (unsigned int)0, bot->getType() == UnitType::CART ? (unsigned int)1 : (unsigned int)0 ,path,Archetype::CITIZEN };

      // compare with not already assigned bots
      for(const std::unique_ptr<Bot> &bot2 : gameState.bots)
      {
          // We don't look at enemies nor cities
          if (bot->getType() == UnitType::CITY || bot->getTeam() != Player::ENEMY)
              continue;
          // We don't look at already assigned bots
          if (std::ranges::find(seenIds, bot2->getId()) != seenIds.end())
              continue;
          if (!gameState.ennemyPath.contains(bot2->getId())) continue;
          auto &path2 = gameState.ennemyPath.at(bot2->getId());
          // We check of paths are similar -> TODO propagate maps to avoid not registering bots side by side as same squad
          if (path.getSimilarity(path2, params::similarityTolerance) >= params::similarPercentage) {
            seenIds.push_back(bot2->getId());
            if (bot->getType() == UnitType::WORKER)
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
      std::ranges::for_each(cityClusters[Player::ENEMY], [&enemyCities](CityCluster cc) {
        enemyCities.addTemplateAtIndex(enemyCities.getIndex(cc.center_x, cc.center_y), influence_templates::ENEMY_CITY_CLUSTER_PROXIMITY);
      });
      
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

    size_t availableBots = 0;
    size_t availableCarts = 0;
    std::ranges::for_each(gameState.bots, [&availableBots, &availableCarts](const std::unique_ptr<Bot> &bot) {
        if (bot->getTeam() != Player::ALLY) return;
        if (bot->getType() == UnitType::CART) availableCarts++;
        else if (bot->getType() == UnitType::WORKER) availableBots++;
    });

    const auto &clusters = getCityClusters(gameState);
    const std::vector<CityCluster> &allyCities = clusters[Player::ALLY];
    const std::vector<CityCluster> &enemyCities = clusters[Player::ENEMY];

    // explore
    // (after all requirements have been fullfilled, remaining bots will be assigned
    // to settlers that target a tile near them)
    availableBots -= std::min(availableBots, 2ull);

    // sustain
    for(size_t i = 0; i < allyCities.size() && availableBots > 2; i++) {
        auto &cc = allyCities[i];
        SquadRequirement sr{ 1,0,10,Archetype::FARMER, gameState.map.getTileIndex(cc.center_x, cc.center_y) };
        squadRequirements.emplace_back(sr);
        availableBots--;
    }

    std::map<Archetype, size_t> priorities{}; // FUTURE should be a constexpr std::array<priority_t, 6>
    priorities.emplace(Archetype::CITIZEN, 2);
    priorities.emplace(Archetype::FARMER, 2);
    priorities.emplace(Archetype::KILLER, 2);
    priorities.emplace(Archetype::ROADMAKER, 2);
    priorities.emplace(Archetype::SETTLER, 2);
    priorities.emplace(Archetype::TROUBLEMAKER, 2);

    //Retaliation
    for (size_t i = 0; i < enemyStance.size() && (availableBots > 0 || availableCarts > 0); i++) {
        SquadRequirement sr{};
        const EnemySquadInfo &enemySquad = enemyStance[i];
        switch (enemySquad.mission) {
        case Archetype::ROADMAKER: {
            sr.botNb = 1;
            sr.cartNb = 0;
            sr.priority = priorities[Archetype::ROADMAKER];
            sr.mission = Archetype::TROUBLEMAKER;
            auto [targetX, targetY] = enemySquad.path.getRandomValuedPoint();
            sr.missionTarget = gameState.map.getTileIndex(targetX, targetY);
            squadRequirements.emplace_back(sr);
            priorities.emplace(Archetype::ROADMAKER, priorities[Archetype::ROADMAKER]+1);
            availableBots--;
        } break; // ROADMAKER blocked by TROUBLEMAKER
        //case SETTLER:
        //    sr.botNb = 1;
        //    sr.cartNb = 0;
        //    sr.priority = priorities[SETTLER];
        //    sr.mission = SETTLER;
        //    // TODO does it realy make sense to create a squad that can go anywhere when we detect an enemy settler?
        //    // its quite hard to separate the code that dictates what squads to create to the one that actually creates them...
        //    sr.missionTarget = SquadRequirement::ANY_TARGET;
        //    squadRequirements.emplace_back(sr);
        //    priorities.emplace(SETTLER, priorities[SETTLER]+1);
        //    availableBots--;
        //    break; // SETTLER blocked by SETTLER : don't go in their direction
        default:
            //LOG("Cannot react to enemy squad with type " << (int)enemySquad.mission);
            break;
        }
        availableBots -= sr.botNb;
    }

    std::ranges::sort(squadRequirements, std::less{}, [](auto &sr) { return sr.priority; });
    return squadRequirements;
}


std::vector<Squad> Strategy::createSquads(const std::vector<SquadRequirement> &squadRequirements, GameState *gameState)
{
    std::vector<Squad> newSquads;
    std::unordered_set<Bot *> unassignedBots;

    for(auto &bot : gameState->bots) {
      if (bot->getTeam() != Player::ALLY || (bot->getType() == UnitType::CITY))
        continue;
      unassignedBots.insert(bot.get());
    }

    for(const SquadRequirement &sr : squadRequirements) {
        //Get the sr.botNb closest bots for this SquadRequirement
        std::priority_queue<std::pair<Bot *, size_t>> bestBots;
        for (Bot *bot : unassignedBots) {
            tileindex_t botLocation = gameState->map.getTileIndex(*bot);
            size_t botDistanceToMissionTarget = gameState->map.distanceBetween(botLocation, sr.missionTarget);
            bestBots.emplace(bot, botDistanceToMissionTarget);
        }
        std::vector<Bot *> nSquad;
        for(size_t remainingWorkers = sr.botNb, remainingCarts = sr.cartNb; (remainingWorkers > 0 || remainingCarts > 0) && !bestBots.empty(); ) {
            Bot *availableBot = bestBots.top().first;
            if(availableBot->getType() == UnitType::CART && remainingCarts > 0) {
                nSquad.push_back(availableBot);
                unassignedBots.erase(availableBot);
                remainingCarts--;
            } else if(availableBot->getType() == UnitType::WORKER && remainingWorkers > 0) {
                nSquad.push_back(availableBot);
                unassignedBots.erase(availableBot);
                remainingWorkers--;
            }
        }
        newSquads.emplace_back(std::move(nSquad), sr.mission, sr.missionTarget);
    }

    // assign remaining bots as settlers/farmers
    size_t lateAssign = 0;
    for(Bot *remainingBot : unassignedBots) {
        lateAssign++;
        if(lateAssign % 3 == 0) { // 1/3 farmers
            newSquads.emplace_back(std::vector<Bot*>{ remainingBot }, Archetype::FARMER, pathing::getBestCityFeedingLocation(remainingBot, gameState));
        } else { // 2/3 settlers
            newSquads.emplace_back(std::vector<Bot*>{ remainingBot }, Archetype::SETTLER, pathing::getBestCityBuildingLocation(remainingBot, gameState));
        }
    }

    return newSquads;
}

