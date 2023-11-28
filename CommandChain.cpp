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
    updateBlackBoard();
    rearrangeSquads(diff);
}

void Commander::updateBlackBoard()
{
    static size_t turnNumber = 0;

    turnNumber++;

    int nbAgents = 0;
    int nbWorkers = 0;
    int nbCarts = 0;
    int nbCities = 0;

    m_blackboardKeepAlive.agentsPositions.clear();
    m_blackboardKeepAlive.nonCityPositions.clear();
    std::ranges::transform(m_gameState->bots, std::back_inserter(m_blackboardKeepAlive.agentsPositions),
      [this](const auto &bot) { return m_gameState->map.getTileIndex(*bot); });
    std::ranges::for_each(m_gameState->bots,
      [&](const auto &bot) { if (bot->getType() != UnitType::CITY) m_blackboardKeepAlive.nonCityPositions.push_back(m_gameState->map.getTileIndex(*bot)); });

    for (auto &bot : m_gameState->bots) {
        if (bot->getTeam() != Player::ALLY) continue;
        if (bot->getType() == UnitType::CITY) {
            nbCities++;
        }
        if (bot->getType() == UnitType::WORKER) {
            nbAgents++;
            nbWorkers++;
        }
        if (bot->getType() == UnitType::CART) {
            nbCities++;
            nbCarts++;
        }
    }

    m_globalBlackboard->insertData(bbn::GLOBAL_TURN, turnNumber);
    m_globalBlackboard->insertData(bbn::GLOBAL_GAME_STATE, m_gameState);
    m_globalBlackboard->insertData(bbn::GLOBAL_MAP, &m_gameState->map);
    m_globalBlackboard->insertData(bbn::GLOBAL_TEAM_RESEARCH_POINT, m_gameState->playerResearchPoints[Player::ALLY]);
    m_globalBlackboard->insertData(bbn::GLOBAL_AGENTS_POSITION, &m_blackboardKeepAlive.agentsPositions);
    m_globalBlackboard->insertData(bbn::GLOBAL_NONCITY_POSITION, &m_blackboardKeepAlive.nonCityPositions);
    m_globalBlackboard->insertData(bbn::GLOBAL_AGENTS, nbAgents);
    m_globalBlackboard->insertData(bbn::GLOBAL_WORKERS, nbWorkers);
    m_globalBlackboard->insertData(bbn::GLOBAL_CARTS, nbCarts);
    m_globalBlackboard->insertData(bbn::GLOBAL_CITY_COUNT, (int)m_gameState->citiesBot.size());
    m_globalBlackboard->insertData(bbn::GLOBAL_FRIENDLY_CITY_COUNT, nbCities);
}

std::vector<TurnOrder> Commander::getTurnOrders(const GameStateDiff &diff)
{
    std::vector<TurnOrder> orders;
    std::vector<Bot *> friendlyCities{};
    std::vector<Bot *> availableCities{};

    m_globalBlackboard->insertData(bbn::GLOBAL_ORDERS_LIST, &orders);

    int availableUnits = m_globalBlackboard->getData<int>(bbn::GLOBAL_FRIENDLY_CITY_COUNT) * 2 - m_globalBlackboard->getData<int>(bbn::GLOBAL_AGENTS);

    for (auto &city : m_gameState->bots) {
        if (city->getType() == UnitType::CITY && city->getTeam() == Player::ALLY) {
            friendlyCities.emplace_back(city.get());
            if (city->getCooldown() < game_rules::MAX_ACT_COOLDOWN)
                availableCities.emplace_back(city.get());
        }
    }

    //For each squad...
    ranges::for_each(m_squads, [&diff,&friendlyCities,&availableUnits](Squad &squad) {
        //Seek created bots that were demanded
        for (std::pair<Bot*, UnitType> cityProd : squad.getAgentsInCreation()) {
            if (cityProd.first->getActedState()) {
                for (Bot *bot : diff.newBots) {
                    if (cityProd.first->getX() == bot->getX() && cityProd.first->getY() == bot->getY()) {
                        squad.getAgents().push_back(bot);
                        cityProd.first->release();
                        break;
                    }
                }
            }
        }
        //squad.addCreatedAgents(diff);
        //And ask for ones that weren't created yet
        squad.sendReinforcementsRequest(friendlyCities, availableUnits);
    });

#ifdef _DEBUG
    int citizens = 0, killers = 0, settlers = 0, troublemakers = 0, farmers = 0, roadmakers = 0;

    for (size_t i = 0; i < m_squads.size(); i++) {
        switch (m_squads[i].getArchetype()) {
        case Archetype::CITIZEN: citizens++; break;
        case Archetype::KILLER: killers++; break;
        case Archetype::SETTLER: settlers++; break;
        case Archetype::TROUBLEMAKER: troublemakers++; break;
        case Archetype::FARMER: farmers++; break;
        case Archetype::ROADMAKER: roadmakers++; break;
        default: break;
        }
    }

    lux::Annotate::sidetext("nb of ally SETTLER : " + std::to_string(settlers));
    lux::Annotate::sidetext("nb of ally CITIZEN : " + std::to_string(citizens));
    lux::Annotate::sidetext("nb of ally FARMER : " + std::to_string(farmers));
    lux::Annotate::sidetext("nb of ally KILLER : " + std::to_string(killers));
    lux::Annotate::sidetext("nb of ally TROUBLEMAKER : " + std::to_string(troublemakers));
    lux::Annotate::sidetext("nb of ally ROADMAKER : " + std::to_string(roadmakers));
#endif

    std::ranges::for_each(availableCities, [&, this](Bot *city) {
        // by default, cities do research. If they have been reserved to create more workers
        // send that order instead
        BotObjective objective{ BotObjective::ObjectiveType::CREATE_WORKER };
        //BotObjective objective{ BotObjective::ObjectiveType::RESEARCH };
        city->getBlackboard().insertData(bbn::AGENT_SELF, city);
        if (!city->getReserveState())
            city->getBlackboard().insertData(bbn::AGENT_OBJECTIVE, objective);
        city->getBlackboard().setParentBoard(m_globalBlackboard);
        city->act();
    });

    if (params::trainingMode)
        statistics::gameStats.printGameStats(m_globalBlackboard);

    std::ranges::for_each(m_squads, [&, this](Squad &squad)
    {
        if (squad.getAgents().size() > 0) 
        {
            std::vector<tileindex_t> targetTiles{};
            BotObjective::ObjectiveType mission = BotObjective::ObjectiveType::BUILD_CITY;
            switch (squad.getArchetype()) {
            case Archetype::CITIZEN:
                targetTiles = pathing::getManyCityBuildingLocations(squad.getAgents()[0], m_gameState, squad.getAgents().size());
                break;
            case Archetype::SETTLER:
                targetTiles = pathing::getManyExpansionLocations(squad.getAgents()[0], m_gameState, squad.getAgents().size());
                break;
            case Archetype::FARMER:
                mission = BotObjective::ObjectiveType::FEED_CITY;
                for (auto bot : squad.getAgents())
                    targetTiles.push_back(squad.getTargetTile());
                //targetTiles = pathing::getManyResourceFetchingLocations(squad.getAgents()[0], m_gameState, squad.getAgents().size());
                break;
            case Archetype::TROUBLEMAKER:
                targetTiles = pathing::getManyBlockingPathLocations(squad.getAgents()[0], m_gameState, squad.getAgents().size());
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
            // TODO rework this code, it is prone to logic errors, as we imperatively need as much target tiles as bots in the squad
            // otherwise, some bots WON'T EVER MOVE

            std::unordered_set<Bot *> playableBots{ squad.getAgents().begin(), squad.getAgents().end() };

            // for each target tile, we search the nearest bot
            for (tileindex_t tile : targetTiles) {
                if (tile == (tileindex_t)-1) {
                    LOG("Could not find a valid target for goal " << (int)squad.getArchetype());
                    continue; // an objective cannot be fullfilled
                }
                if (playableBots.empty()) break;
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
    case Archetype::ROADMAKER:    return false;
    case Archetype::KILLER:       return false;
    case Archetype::TROUBLEMAKER: return false;
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
  BENCHMARK_BEGIN(getEnemyStance);
  auto enemyStance = currentStrategy.getEnemyStance(*m_gameState);
  BENCHMARK_END(getEnemyStance);
  if(m_previousEnemyStance.empty() || shouldUpdateSquads(diff, enemyStance)) {
    lux::Annotate::sidetext("! Updating squads");
    BENCHMARK_BEGIN(adaptToEnemy);
    auto stanceToTake = currentStrategy.adaptToEnemy(enemyStance, *m_gameState, m_globalBlackboard);
    BENCHMARK_END(adaptToEnemy);
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

    //LOG("Turn " + std::to_string(gameState.currentTurn));

    auto cityClusters = getCityClusters(gameState);

    { // remove small enemy clusters (ongoing expansions)
      auto &enemyClusters = cityClusters[Player::ENEMY];
      std::erase_if(enemyClusters, [](auto &cluster) { return cluster.cityTileCount < 3; });
    }

    InfluenceMap propagatedInfluence = gameState.resourcesInfluence.propagateAllTimes(1);

    std::unordered_set<std::string> seenIds;

    std::unordered_map<std::string, InfluenceMap> propagatedPaths;
    for(auto &[botId, path] : gameState.ennemyPath) {
      auto propagated = path.propagateAllTimes(params::propagationRadius);
      propagated.normalize();
      propagatedPaths.emplace(botId, std::move(propagated));
    }

    for(const std::unique_ptr<Bot> &bot : gameState.bots) {
      // We don't look at enemies nor cities
      if (bot->getType() == UnitType::CITY || bot->getTeam() != Player::ENEMY)
        continue;

      // We don't look at already assigned bots
      if (seenIds.contains(bot->getId()))
        continue;

      EnemySquadInfo enemySquadInfo{
        bot->getType() == UnitType::WORKER ? (unsigned int)1 : (unsigned int)0,
        bot->getType() == UnitType::CART ? (unsigned int)1 : (unsigned int)0,
        gameState.ennemyPath.at(bot->getId()), // FIX
        Archetype::CITIZEN
      };

      // propagate the path (blur) to make it more sensitive to surroundings
      InfluenceMap &propagatedPath = propagatedPaths[bot->getId()];

      // compare with not already assigned bots
      for(const std::unique_ptr<Bot> &bot2 : gameState.bots)
      {
          // We don't look at enemies nor cities
          if (bot->getType() == UnitType::CITY || bot->getTeam() != Player::ENEMY)
              continue;
          // We don't look at already assigned bots
          if (seenIds.contains(bot->getId()))
              continue;
          if (!gameState.ennemyPath.contains(bot2->getId())) continue;
          if (bot->getId() == bot2->getId()) continue;
          auto &path2 = propagatedPaths[bot2->getId()];
          // We check if paths are similar
          if (propagatedPath.propagateAllTimes(params::propagationRadius).getSimilarity(path2, params::similarityTolerance) >= params::similarPercentage) {
            seenIds.insert(bot2->getId());
            if (bot->getType() == UnitType::WORKER)
                enemySquadInfo.botNb++;
            else
                enemySquadInfo.cartNb++;
          }
      }
        
      // guess the squad's current objective

      // if bot's path covers a resource point... TODO might need to propagate resourceInfluence
      if (propagatedPath.coversTiles(propagatedInfluence, params::resourceTilesNeeded))
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
          }
      
          // if bot's path covers a resource point AND an enemy city, he's either expanding the city or fueling it (same movement pattern, in fact)
          if (coversACity) {
              auto &path = gameState.ennemyPath.at(bot->getId());
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
            if (gameState.ennemyPath.at(bot->getId()).approachesPoint(cc.center_x, cc.center_y, params::ennemyPathingTurn, params::pathStep))
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

#ifdef _DEBUG
    int citizens = 0, killers = 0, settlers = 0, troublemakers = 0, farmers = 0, roadmakers = 0;

    for (size_t i = 0; i < enemySquads.size(); i++) {
        switch (enemySquads[i].mission) {
        case Archetype::CITIZEN: citizens++; break;
        case Archetype::KILLER: killers++; break;
        case Archetype::SETTLER: settlers++; break;
        case Archetype::TROUBLEMAKER: troublemakers++; break;
        case Archetype::FARMER: farmers++; break;
        case Archetype::ROADMAKER: roadmakers++; break;
        default: break;
        }
    }

    /*lux::Annotate::sidetext("nb of SETTLER : " + std::to_string(settlers));
    lux::Annotate::sidetext("nb of CITIZEN : " + std::to_string(citizens));
    lux::Annotate::sidetext("nb of FARMER : " + std::to_string(farmers));
    lux::Annotate::sidetext("nb of KILLER : " + std::to_string(killers));
    lux::Annotate::sidetext("nb of TROUBLEMAKER : " + std::to_string(troublemakers));
    lux::Annotate::sidetext("nb of ROADMAKER : " + std::to_string(roadmakers));*/
#endif

    return enemySquads;
}

std::vector<SquadRequirement> Strategy::adaptToEnemy(const std::vector<EnemySquadInfo> &enemyStance, const GameState &gameState, std::shared_ptr<Blackboard> blackBoard)
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

    std::map<Archetype, size_t> priorities{};
    priorities.emplace(Archetype::CITIZEN, 2);
    priorities.emplace(Archetype::FARMER, 2);
    priorities.emplace(Archetype::KILLER, 2);
    priorities.emplace(Archetype::ROADMAKER, 2);
    priorities.emplace(Archetype::SETTLER, 2);
    priorities.emplace(Archetype::TROUBLEMAKER, 2);

    int craftableBots = std::max(0, (int)(blackBoard->getData<int>(bbn::GLOBAL_FRIENDLY_CITY_COUNT) - availableBots - availableCarts));

    // explore
    // (after all requirements have been fullfilled, remaining bots will be assigned
    // to settlers that target a tile near them)
    availableBots -= std::min(availableBots, 2ull);

    // sustain
    for(size_t i = 0; i < allyCities.size() && availableBots + craftableBots > 0; i++) {
        auto &cc = allyCities[i];
        SquadRequirement sr{ 1,0,10,Archetype::FARMER, gameState.map.getTileIndex(cc.center_x, cc.center_y) };
        squadRequirements.push_back(sr);
        if (availableBots > 0)
            availableBots--;
        else
            craftableBots--;
    }

    //Retaliation
    for (size_t i = 0; i < enemyStance.size() && (availableBots + craftableBots > 0 || availableCarts + craftableBots > 0); i++) {
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
            squadRequirements.push_back(sr);
            priorities[Archetype::ROADMAKER] = priorities[Archetype::ROADMAKER]+1;
            if (availableBots > 2)
                availableBots--;
            else
                craftableBots--;

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
        /*for (int i = 0; i < sr.botNb; i++)
        {
            if (availableBots > 0)
                availableBots--;
            else
                craftableBots--;
        }*/
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
        Squad nSquad{};
        // if there are only 2 bots left, we save them for farmers / settlers
        for(size_t remainingWorkers = sr.botNb, remainingCarts = sr.cartNb; (remainingWorkers > 0 || remainingCarts > 0); ) {
            if (bestBots.size() <= 2ull)
            {
                for (int i = 0; i < remainingWorkers; i++)
                    nSquad.getAgentsToCreate().push_back({ {gameState->map.getTilePosition(sr.missionTarget)}, UnitType::WORKER });
                for (int i = 0; i < remainingCarts; i++)
                    nSquad.getAgentsToCreate().push_back({ {gameState->map.getTilePosition(sr.missionTarget)}, UnitType::CART });
                remainingWorkers = 0;
                remainingCarts = 0;
            }
            else
            {
                Bot *availableBot = bestBots.top().first;
                bestBots.pop();
                if (availableBot->getType() == UnitType::CART && remainingCarts > 0) {
                    nSquad.getAgents().push_back(availableBot);
                    unassignedBots.erase(availableBot);
                    remainingCarts--;
                } else if (availableBot->getType() == UnitType::WORKER && remainingWorkers > 0) {
                    nSquad.getAgents().push_back(availableBot);
                    unassignedBots.erase(availableBot);
                    remainingWorkers--;
                }
            }
        }
        nSquad.setArchetype(sr.mission);
        nSquad.setTargetTile(sr.missionTarget);
        newSquads.emplace_back(nSquad);
    }

    // assign remaining bots as settlers/farmers
    size_t lateAssign = -1;
    for(Bot *remainingBot : unassignedBots) {

        lateAssign++;

        // 1/4 settler
        if(lateAssign % 4 == 0) {
            tileindex_t targetCity = pathing::getBestCityBuildingLocation(remainingBot, gameState);
            if(targetCity != (tileindex_t)-1) {
                newSquads.emplace_back(std::vector<Bot*>{ remainingBot }, Archetype::SETTLER, targetCity);
                continue;
            }
        }
        // 1/4 citizen
        if(lateAssign % 4 == 1)
        {
            newSquads.emplace_back(std::vector<Bot *>{ remainingBot }, Archetype::CITIZEN, pathing::getBestExpansionLocation(remainingBot, gameState));
            continue;
        }

        // 2/4 farmer
        newSquads.emplace_back(std::vector<Bot *>{ remainingBot }, Archetype::FARMER, pathing::getBestCityFeedingLocation(remainingBot, gameState));
    }

    return newSquads;
}

void Squad::sendReinforcementsRequest(std::vector<Bot *> &cities, int &availableUnits)
{
    std::vector<std::pair<std::pair<int, int>, UnitType>> notReservedBots{};
    for (auto bot : m_agentsToCreate) {
        Bot *nearestCity = cities[0];
        float nearestDistance = std::numeric_limits<unsigned int>::max();
        bool foundCity = false;
        for (Bot *city : cities) {
            unsigned int botToCityDist = abs(bot.first.first - city->getX()) + abs(bot.first.second - city->getY());
            if (botToCityDist < nearestDistance && !city->getReserveState()) {
                nearestCity = city;
                nearestDistance = abs(bot.first.first - city->getX()) + abs(bot.first.second - city->getY());
                foundCity = true;
            }
        }
        if (foundCity) {
            if (bot.second == UnitType::WORKER)
                nearestCity->getBlackboard().insertData(bbn::AGENT_OBJECTIVE, BotObjective{ BotObjective::ObjectiveType::CREATE_WORKER, 0 });
            else
                nearestCity->getBlackboard().insertData(bbn::AGENT_OBJECTIVE, BotObjective{ BotObjective::ObjectiveType::CREATE_CART, 0 });
            nearestCity->reserve();
            m_agentsInCreation.push_back(std::pair<Bot *, UnitType>(nearestCity, bot.second));
        } else {
            std::pair<std::pair<int, int>, UnitType> botCopy{ bot };
            notReservedBots.push_back(botCopy);
        }
    };
    m_agentsToCreate = notReservedBots;
}

void Squad::addCreatedAgents(const GameStateDiff &diff)
{
    for (auto cityProd : m_agentsInCreation) {
        if (cityProd.first->getActedState()) {
            for (Bot *bot : diff.newBots) {
                if (cityProd.first->getX() == bot->getX() && cityProd.first->getY() == bot->getY()) {
                    m_agents.push_back(bot);
                    cityProd.first->release();
                    break;
                }
            }
        }
    }
}