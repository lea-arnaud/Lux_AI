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
#include "Benchmarking.h"
#include "Statistics.h"


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
    std::vector<Bot *> availableCities{};

    for (auto &city : m_gameState->bots) {
        if (city->getType() == UnitType::CITY && city->getTeam() == Player::ALLY) {
            nbCities++;
            if (city->getCooldown() < game_rules::MAX_ACT_COOLDOWN)
                availableCities.emplace_back(city.get());
        }
    }

    lux::Annotate::sidetext("Number of available cities : " + std::to_string(availableCities.size()));

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
    m_globalBlackboard->insertData(bbn::GLOBAL_CITY_COUNT, (int)m_gameState->citiesBot.size());
    m_globalBlackboard->insertData(bbn::GLOBAL_FRIENDLY_CITY_COUNT, nbCities);

    std::ranges::for_each(availableCities, [&, this](Bot *city) {
        BotObjective objective{ BotObjective::ObjectiveType::BUILD_CITY, 0 };
        city->getBlackboard().insertData(bbn::AGENT_SELF, city);
        city->getBlackboard().insertData(bbn::AGENT_OBJECTIVE, objective);
        city->getBlackboard().setParentBoard(m_globalBlackboard);
        city->act();
    });

    if (params::trainingMode)
        statistics::gameStats.printGameStats(m_globalBlackboard);

    std::ranges::for_each(m_squads, [&, this](Squad &squad)
    {
        std::vector<tileindex_t> targetTiles{};
        BotObjective::ObjectiveType mission = BotObjective::ObjectiveType::BUILD_CITY;
        switch (squad.getArchetype()) {
        case Archetype::CITIZEN: 
            targetTiles = pathing::getManyExpansionLocations(squad.getAgents()[0], m_gameState, squad.getAgents().size()); 
            break;
        case Archetype::SETTLER: 
            targetTiles = pathing::getManyCityBuildingLocations(squad.getAgents()[0], m_gameState, squad.getAgents().size()); 
            break;
        case Archetype::FARMER:
            mission = BotObjective::ObjectiveType::FEED_CITY;
            targetTiles = pathing::getManyResourceFetchingLocations(squad.getAgents()[0], m_gameState, squad.getAgents().size());
            break;
        case Archetype::TROUBLEMAKER:
            targetTiles = { squad.getTargetTile() }; // TODO give more tiles to block ? Our troublemakers are loners though...
            mission = BotObjective::ObjectiveType::GO_BLOCK_PATH;
            break;
        case Archetype::ROADMAKER:
            mission = BotObjective::ObjectiveType::MAKE_ROAD; // Should only be applied to carts
            // Also, we don't have any roadmakers for now...
            break;
        case Archetype::KILLER:
            // TODO implement KILLER squad behavior with GOAP
            break;
        }
        // this code is prone to logic errors, as we imperatively need as much target tiles as bots in the squad
        // otherwise, some bots WON'T EVER MOVE

        std::unordered_set<Bot *> playableBots{ squad.getAgents().begin(), squad.getAgents().end() };

        // for each target tile, we search the nearest bot
        for (tileindex_t tile : targetTiles) {
            if(tile == (tileindex_t)-1) {
                LOG("Could not find a valid target for goal " << (int)squad.getArchetype());
                continue; // an objective cannot be fullfilled
            }
            if(playableBots.empty()) break;
            Bot *nearestBot = nullptr;
            unsigned int nearestDist = INT_MAX;
            for (Bot *bot : playableBots) {
                if (m_gameState->map.distanceBetween(tile, m_gameState->map.getTileIndex(bot->getX(), bot->getY())) < nearestDist) {
                    nearestBot = bot;
                    nearestDist = m_gameState->map.distanceBetween(tile, m_gameState->map.getTileIndex(bot->getX(), bot->getY()));
                }
            }
            playableBots.erase(nearestBot);
            // he acts only if he can
            if (nearestBot->getCooldown() >= game_rules::MAX_ACT_COOLDOWN)
                continue;
            // we put this tile as the bot's objective
            //lux::Annotate::sidetext(nearestBot->getId() + " has objective " + std::to_string((int)mission) + " at tile " + std::to_string(m_gameState->map.getTilePosition(tile).first) + ";" + std::to_string(m_gameState->map.getTilePosition(tile).second));
            BotObjective objective{ mission, tile };
            nearestBot->getBlackboard().insertData(bbn::AGENT_SELF, nearestBot);
            nearestBot->getBlackboard().insertData(bbn::AGENT_OBJECTIVE, objective);
            nearestBot->getBlackboard().setParentBoard(m_globalBlackboard);
            nearestBot->act();
        }
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
    BENCHMARK_BEGIN(createSquads);
    m_squads = currentStrategy.createSquads(stanceToTake, m_gameState);
    m_previousEnemyStance = std::move(enemyStance);
    BENCHMARK_END(createSquads);
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

    LOG("Turn " + std::to_string(gameState.currentTurn));

    auto cityClusters = getCityClusters(gameState);

    { // remove small enemy clusters (ongoing expansions)
      auto &enemyClusters = cityClusters[Player::ENEMY];
      std::erase_if(enemyClusters, [](auto &cluster) { return cluster.cityTileCount < 3; });
    }

    std::vector<std::string> seenIds{}; 

    bool b = false;

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
          if (bot->getId() == bot2->getId()) continue;
          auto path2 = (gameState.ennemyPath.at(bot2->getId())).propagateAllTimes(params::propagationRadius);
          // We check if paths are similar
          if ((path.propagateAllTimes(params::propagationRadius)).getSimilarity(path2, params::similarityTolerance) >= params::similarPercentage) {
            seenIds.push_back(bot2->getId());
            if (bot->getType() == UnitType::WORKER)
                enemySquadInfo.botNb++;
            else
                enemySquadInfo.cartNb++;
          }
      }

      // propagate the path (blur) to make it more sensitive to surroundings
      InfluenceMap propagatedPath = path.propagateAllTimes(params::propagationRadius);
      propagatedPath.normalize();
        
      // guess the squad's current objective

      // if bot's path covers a resource point... TODO might need to propagate resourceInfluence
      if (propagatedPath.coversTiles(gameState.resourcesInfluence.propagateAllTimes(1), params::resourceTilesNeeded))
      {
          // enemyCities is an InfluenceMap created to facilitate operations with path
          InfluenceMap enemyCities{gameState.citiesInfluence.getWidth(), gameState.citiesInfluence.getHeight()};
          bool coversACity = false;
          for (int i = 0; i < cityClusters[Player::ENEMY].size(); i++){
              InfluenceMap ccMap{ gameState.citiesInfluence.getWidth(), gameState.citiesInfluence.getHeight() };
              ccMap.addTemplateAtIndex(ccMap.getIndex(cityClusters[Player::ENEMY][i].center_x, cityClusters[Player::ENEMY][i].center_y), influence_templates::ENEMY_CITY_CLUSTER_PROXIMITY);
              if (propagatedPath.coversTiles(ccMap, params::cityTilesNeeded)) {
                  coversACity = true;
                  break;
              }
          };
      
          // if bot's path covers a resource point AND an enemy city, he's either expanding the city or fueling it (same movement pattern, in fact)
          if (coversACity) {
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
            // TODO we may wanna create a specific parameter for the length of this path
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

    int citizens = 0, killers = 0, settlers = 0, troublemakers = 0, farmers = 0, roadmakers = 0;

    for (size_t i = 0; i < enemySquads.size(); i++) {
        switch (enemySquads[i].mission) {
        case Archetype::CITIZEN: citizens++; break;
        case Archetype::KILLER: killers++; break;
        case Archetype::SETTLER: settlers++; break;
        case Archetype::TROUBLEMAKER: troublemakers++; break;
        case Archetype::FARMER: farmers++; break;
        case Archetype::ROADMAKER: roadmakers++; break;
        default:
            break;
        }
    }

    lux::Annotate::sidetext("nb of SETTLER : " + std::to_string(settlers));
    lux::Annotate::sidetext("nb of CITIZEN : " + std::to_string(citizens));
    lux::Annotate::sidetext("nb of FARMER : " + std::to_string(farmers));
    lux::Annotate::sidetext("nb of KILLER : " + std::to_string(killers));
    lux::Annotate::sidetext("nb of TROUBLEMAKER : " + std::to_string(troublemakers));
    lux::Annotate::sidetext("nb of ROADMAKER : " + std::to_string(roadmakers));

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

    std::map<Archetype, size_t> priorities{}; // FUTURE should be a constexpr std::array<priority_t, 6>
    priorities.emplace(Archetype::CITIZEN, 2);
    priorities.emplace(Archetype::FARMER, 2);
    priorities.emplace(Archetype::KILLER, 2);
    priorities.emplace(Archetype::ROADMAKER, 2);
    priorities.emplace(Archetype::SETTLER, 2);
    priorities.emplace(Archetype::TROUBLEMAKER, 2);

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

    //Retaliation
    for (size_t i = 0; i < enemyStance.size() && (availableBots > 0 || availableCarts > 0); i++) {
        SquadRequirement sr{};
        const EnemySquadInfo &enemySquad = enemyStance[i];
        switch (enemySquad.mission) {
        case Archetype::ROADMAKER:
        {
            sr.botNb = 1;
            sr.cartNb = 0;
            sr.priority = priorities[Archetype::ROADMAKER];
            sr.mission = Archetype::TROUBLEMAKER;
            auto [targetX, targetY] = enemySquad.path.getRandomValuedPoint();
            sr.missionTarget = gameState.map.getTileIndex(targetX, targetY);
            squadRequirements.emplace_back(sr);
            priorities[Archetype::ROADMAKER] = priorities[Archetype::ROADMAKER]+1;
            availableBots--;
        } break; // ROADMAKER blocked by TROUBLEMAKER
        //case SETTLER:
        //    sr.botNb = x; // x + y = enough to surround the squad (4 for 1, 6 for 2...)
        //    sr.cartNb = y;
        //    sr.priority = priorities[SETTLER];
        //    sr.mission = KILLER;
        //    // its quite hard to separate the code that dictates what squads to create to the one that actually creates them...
        //    sr.missionTarget = SquadRequirement::ANY_TARGET;
        //    squadRequirements.emplace_back(sr);
        //    priorities.emplace(SETTLER, priorities[SETTLER]+1);
        //    availableBots -= x;
        //    availableCarts -= y;
        //    break; // SETTLER blocked by KILLER : kill'em and steal their spot
        default:
            priorities[enemySquad.mission] = priorities[enemySquad.mission]+1;
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
            bestBots.pop();
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
        // 1/3 farmers
        if(lateAssign % 3 == 0) {
            tileindex_t targetCity = pathing::getBestCityFeedingLocation(remainingBot, gameState);
            if(targetCity != (tileindex_t)-1) {
                newSquads.emplace_back(std::vector<Bot*>{ remainingBot }, Archetype::FARMER, targetCity);
                continue;
            }
        }
        // 2/3 settlers
        newSquads.emplace_back(std::vector<Bot*>{ remainingBot }, Archetype::SETTLER, pathing::getBestCityBuildingLocation(remainingBot, gameState));
    }

    return newSquads;
}