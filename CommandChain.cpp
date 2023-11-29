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
#include "AIParams.h"
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
    std::ranges::for_each(m_squads, [&diff,&friendlyCities,&availableUnits](Squad &squad) {
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

    std::ranges::for_each(availableCities, [&, this](Bot *city) {
        // by default, cities do research. If they have been reserved to create more workers
        // send that order instead
        BotObjective objective{ BotObjective::ObjectiveType::RESEARCH };
        //BotObjective objective{ BotObjective::ObjectiveType::RESEARCH };
        if (city->getUnitToCreate() == UnitType::CART)
            objective.type = BotObjective::ObjectiveType::CREATE_CART;
        if (city->getUnitToCreate() == UnitType::WORKER)
            objective.type = BotObjective::ObjectiveType::CREATE_WORKER;
        city->getBlackboard().insertData(bbn::AGENT_SELF, city);
        city->getBlackboard().insertData(bbn::AGENT_OBJECTIVE, objective);
        city->getBlackboard().setParentBoard(m_globalBlackboard);
        city->act();
    });

    if (params::trainingMode)
        statistics::gameStats.printGameStats(m_globalBlackboard);

    std::ranges::for_each(m_squads, [&, this](Squad &squad) {
        int squadSize = static_cast<int>(squad.getAgents().size());
        if (squadSize > 0 && !squad.getOrderGiven()) {
            std::vector<tileindex_t> targetTiles{};
            BotObjective::ObjectiveType mission = BotObjective::ObjectiveType::BUILD_CITY;
            tileindex_t firstBotPosition = m_gameState->map.getTileIndex(squad.getAgents()[0]->getX(), squad.getAgents()[0]->getY());
            switch (squad.getArchetype()) {
            case Archetype::CITIZEN:
                targetTiles = pathing::getManyExpansionLocations(firstBotPosition, m_gameState, squadSize);
                break;
            case Archetype::SETTLER:
                squad.setOrderGiven(true);
                targetTiles = pathing::getManyCityBuildingLocations(firstBotPosition, m_gameState, squadSize);
                // we change settlers to citizens once they accomplish their goals, so that their city doesn't get destroyed
                for (auto &bot : squad.getAgents()) {
                    tileindex_t botTile = m_gameState->map.getTileIndex(bot->getX(), bot->getY());
                    if (m_gameState->map.tileAt(botTile).getType() == TileType::ALLY_CITY && std::ranges::find(targetTiles, botTile) != targetTiles.end()) {
                        squad.setArchetype(Archetype::CITIZEN);
                        targetTiles = pathing::getManyExpansionLocations(firstBotPosition, m_gameState, squadSize);
                        break;
                    }
                }
                break;
            case Archetype::FARMER:
                mission = BotObjective::ObjectiveType::FEED_CITY;
                for (auto &bot : squad.getAgents())
                    targetTiles.push_back(squad.getTargetTile());
                break;
            case Archetype::TROUBLEMAKER:
                targetTiles = pathing::getManyBlockingPathLocations(firstBotPosition, m_gameState, squadSize);
                mission = BotObjective::ObjectiveType::GO_BLOCK_PATH;
                break;
            case Archetype::ROADMAKER:
                targetTiles = pathing::getManyCityBuildingLocations(firstBotPosition, m_gameState, squadSize);
                mission = BotObjective::ObjectiveType::MAKE_ROAD; // only applied to carts
                break;
            case Archetype::KILLER:
                // TODO implement KILLER squad behavior with GOAP
                break;
            }

            std::unordered_set<Bot *> playableBots{ squad.getAgents().begin(), squad.getAgents().end() };

            // for each target tile, we search the nearest bot
            for (tileindex_t tile : targetTiles) {
                if (tile == (tileindex_t)-1) {
                    LOG("Could not find a valid target for goal " << (int)squad.getArchetype());
                    continue; // an objective cannot be fullfilled
                }
                if (playableBots.empty()) break;
                Bot *nearestBot = nullptr;
                size_t nearestDist = std::numeric_limits<size_t>::max();
                for (Bot *bot : playableBots) {
                    size_t botDist = m_gameState->map.distanceBetween(tile, m_gameState->map.getTileIndex(bot->getX(), bot->getY()));
                    if (botDist < nearestDist) {
                        nearestBot = bot;
                        nearestDist = botDist;
                    }
                }
                playableBots.erase(nearestBot);
                // he acts only if he can
                if (nearestBot->getCooldown() >= game_rules::MAX_ACT_COOLDOWN)
                    continue;
                // we put this tile as the bot's objective
                BotObjective objective{ mission, tile };
                if (mission == BotObjective::ObjectiveType::MAKE_ROAD)
                    objective.returnTile = pathing::getBestExpansionLocation(m_gameState->map.getTileIndex(nearestBot->getX(), nearestBot->getY()), m_gameState);
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
    if (squad.getTargetTile() == (tileindex_t)-1) return true; // a squad could not find a valid objective on the previous turn
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
  auto enemyStance = currentStrategy.getEnemyStance(*m_gameState);
  if(m_previousEnemyStance.empty() || shouldUpdateSquads(diff, enemyStance)) {
    lux::Annotate::sidetext("! Updating squads");
    auto stanceToTake = currentStrategy.adaptToEnemy(enemyStance, *m_gameState, m_globalBlackboard);
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

std::pair<int,int> Strategy::getNbrAgent(const CityCluster &cityCluster, int botsAllowed) const
{
  // A > 0, A = apport moyen en ressource d'un farmer par tour
  constexpr float A = 2.0f;
  // B < 0, B = apport moyenne en ressource d'une cityTile par tour
  constexpr float B = -5.75f;
  // C > 0, C = création moyenne de villes d'un citizen par tour
  constexpr float C = 0.05f;
  constexpr float CAP_SECURITE = 10.0f;

  // Calcul venant du système A * (nbr de FARMER) + B * (nombre de CityTiles + C * (nbr de CITIZEN)) et (nbr de FARMER) + (nbr de CITIZEN) = 1 (résultat normalisé)

  int nC = std::floor((botsAllowed - CAP_SECURITE/A + B * cityCluster.cityTileCount / A) * (1.f / (1.f - B * C / A)));
  nC = std::max(0, nC);
  int nF = botsAllowed - nC;

  LOG("nF = " + std::to_string(nF) + "; nC = " + std::to_string(nC));

  return std::make_pair(nF,nC);
}

std::vector<EnemySquadInfo> Strategy::getEnemyStance(const GameState &gameState)
{
    std::vector<EnemySquadInfo> enemySquads;

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

      // if bot's path covers a resource point...
      if (propagatedPath.coversTiles(propagatedInfluence, params::resourceTilesNeeded))
      {
          // enemyCities is an InfluenceMap created to facilitate operations with path
          InfluenceMap enemyCities{gameState.citiesInfluence.getWidth(), gameState.citiesInfluence.getHeight()};
          bool coversACity = false;
          for (size_t i = 0; i < cityClusters[Player::ENEMY].size(); i++){
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
            // FUTURE we may wanna create a specific parameter for the length of this path
            if (gameState.ennemyPath.at(bot->getId()).approachesPoint(cc.center_x, cc.center_y, params::ennemyPathingTurn, params::pathStep)) {
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

std::pair<int,std::vector<SquadRequirement>> Strategy::adaptToEnemy(const std::vector<EnemySquadInfo> &enemyStance, const GameState &gameState, std::shared_ptr<Blackboard> blackBoard)
{
    std::vector<SquadRequirement> squadRequirements{};

    for (auto city : gameState.citiesBot)
    {
        if (city->getTeam() != Player::ALLY) continue;
        city->release();
    }

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
    priorities.emplace(Archetype::CITIZEN, 5);
    priorities.emplace(Archetype::FARMER, 5);
    priorities.emplace(Archetype::KILLER,5);
    priorities.emplace(Archetype::ROADMAKER, 5);
    priorities.emplace(Archetype::SETTLER, 5);
    priorities.emplace(Archetype::TROUBLEMAKER, 5);

    int craftableBots = std::max(0, (int)(blackBoard->getData<int>(bbn::GLOBAL_FRIENDLY_CITY_COUNT) - availableBots - availableCarts));
    

    // we always need at least one CITIZEN
    SquadRequirement src{ 1,0,1,Archetype::CITIZEN,0 };
    if (allyCities.size() > 0)
        src.missionTarget = gameState.map.getTileIndex(allyCities[0].center_x, allyCities[0].center_y);
    else
    {
        bool assigned = false;
        for (auto city : gameState.citiesBot)
        {
            if (city->getTeam() == Player::ALLY)
            {
                src.missionTarget = gameState.map.getTileIndex(city->getX(), city->getY());
                assigned = true;
                break;
            }
        }
        if (!assigned)
            for (int i = 0; i < gameState.bots.size(); i++) {
                if (gameState.bots[i]->getTeam() == Player::ALLY) {
                    src.missionTarget = gameState.map.getTileIndex(gameState.bots[i]->getX(), gameState.bots[i]->getY());
                    assigned = true;
                    break;
                }
            }
    }
    squadRequirements.push_back(src);

    // sustain
    for(size_t i = 0; i < allyCities.size() && availableBots + craftableBots > 0; i++) {
        auto &cc = allyCities[i];
        SquadRequirement sr{ 1,0,3,Archetype::FARMER, gameState.map.getTileIndex(cc.center_x, cc.center_y) };
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
    }

    // allocate 2 roadmakers if we are far enough in the game
    // (early making workers is way more important)
    if(gameState.currentTurn > 100)
    for(int i = 0; i < 2 && craftableBots > 0; i++, craftableBots--) {
        SquadRequirement sr{ 0,1,10,Archetype::ROADMAKER, 0 };
        squadRequirements.push_back(sr);
        craftableBots--;
    }

    std::ranges::sort(squadRequirements, std::less{}, [](auto &sr) { return sr.priority; });
    return { craftableBots, squadRequirements };
}


std::vector<Squad> Strategy::createSquads(const std::pair<int, std::vector<SquadRequirement>> &squadRequirementsData, GameState *gameState)
{
    std::vector<Squad> newSquads;
    std::unordered_set<Bot *> unassignedBots;
    int unCreatedBots = squadRequirementsData.first;
    auto squadRequirements = squadRequirementsData.second;

    // count required bots per mission archetype
    // note that some bots will be left without assignment after all squads requirements
    // have been fullfilled, these bots will become citizen,farmers&settlers so the
    // totalPerArchetype table will underestimate the number of bots for these three types
    auto archetypeIndex = [](Archetype archetype) -> size_t { return static_cast<size_t>(archetype); };
    std::array<size_t, archetypeIndex(Archetype::__COUNT)> totalPerArchetype;
    for(auto &requirement : squadRequirements) {
      totalPerArchetype[archetypeIndex(requirement.mission)] += requirement.botNb;
      totalPerArchetype[archetypeIndex(requirement.mission)] += requirement.cartNb;
    }

    // collect workers/carts that can be assigned
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

    // assign remaining bots as citizen/farmers
    /*
    size_t nbRemainingBots = unassignedBots.size() + unCreatedBots;
    std::vector<CityCluster> allyClusters = getCityClusters(*gameState)[Player::ALLY];
    if (allyClusters.size() > 0) {
        std::vector<std::pair<CityCluster, std::pair<int, int>>> clusterNeeds{};
        size_t maxTileNumber = 0;
        size_t minTileNumber = std::numeric_limits<size_t>::max();
        for (auto cc : allyClusters) {
            if (cc.cityTileCount > maxTileNumber)
                maxTileNumber = cc.cityTileCount;
            if (cc.cityTileCount > minTileNumber)
                minTileNumber = cc.cityTileCount;
        }
        size_t invTileSum = 0;
        for (auto cc : allyClusters) {
            invTileSum += maxTileNumber + minTileNumber - cc.cityTileCount;
        }
        if (invTileSum <= 0)
            invTileSum = 1;
        int attributedBots = 0;
        for (auto cc : allyClusters) {
            int nbBotsAllowed = std::max(0, (int)((maxTileNumber + minTileNumber - cc.cityTileCount)*nbRemainingBots/invTileSum));
            std::pair<int, int> farmersAndCitizens = getNbrAgent(cc, nbBotsAllowed);
            attributedBots += farmersAndCitizens.first + farmersAndCitizens.second;
            clusterNeeds.push_back({ cc, farmersAndCitizens });
        }

        if (attributedBots > nbRemainingBots) LOG("ALED");

        int index = 0;
        if (!unassignedBots.empty())
            for (auto bot : unassignedBots) {
                while (clusterNeeds[index % clusterNeeds.size()].second.first <= 0 && clusterNeeds[index % clusterNeeds.size()].second.second <= 0) {
                    index++;
                    if (index > 100) break;
                }
                if (index > 100) break;
                auto cityC = clusterNeeds[index % clusterNeeds.size()].first;
                Squad botSquad{};
                botSquad.getAgents().push_back(bot);
                if (clusterNeeds[index % clusterNeeds.size()].second.first > 0 && gameState->map.distanceBetween(pathing::getBestCityFeedingLocation(gameState->map.getTileIndex(bot->getX(), bot->getY()), gameState), gameState->map.getTileIndex(bot->getX(), bot->getY()))
                    < gameState->map.distanceBetween(gameState->map.getTileIndex(cityC.center_x, cityC.center_y), gameState->map.getTileIndex(bot->getX(), bot->getY()))) {
                    botSquad.setArchetype(Archetype::FARMER);
                    clusterNeeds[index % clusterNeeds.size()].second.first--;
                } else {
                    botSquad.setArchetype(Archetype::CITIZEN);
                    clusterNeeds[index % clusterNeeds.size()].second.first--;
                }
                botSquad.setTargetTile(gameState->map.getTileIndex(cityC.center_x, cityC.center_y));
                newSquads.emplace_back(botSquad);
                attributedBots--;
                index++;
            }
        for (auto cityN : clusterNeeds) {
            for (int nbFarmer = 0; nbFarmer < cityN.second.first; nbFarmer++) {
                Squad botSquad{};
                botSquad.setArchetype(Archetype::FARMER);
                std::pair<int, int> cityCoords = { cityN.first.center_x, cityN.first.center_y };
                botSquad.getAgentsToCreate().push_back({ cityCoords, UnitType::WORKER });
                botSquad.setTargetTile(gameState->map.getTileIndex(cityN.first.center_x, cityN.first.center_y));
                newSquads.emplace_back(botSquad);
                attributedBots--;
                unCreatedBots--;
            }
            for (int nbCitizen = 0; nbCitizen < cityN.second.second; nbCitizen++) {
                Squad botSquad{};
                botSquad.setArchetype(Archetype::CITIZEN);
                std::pair<int, int> cityCoords = { cityN.first.center_x, cityN.first.center_y };
                botSquad.getAgentsToCreate().push_back({ cityCoords, UnitType::WORKER });
                botSquad.setTargetTile(gameState->map.getTileIndex(cityN.first.center_x, cityN.first.center_y));
                newSquads.emplace_back(botSquad);
                attributedBots--;
                unCreatedBots--;
            }
        }

        if (attributedBots != 0) LOG("Not normal");
    }*/

    //could be better if we find a way to prioritize the city that needs the most bots

    size_t lateAssign = -1;
    for(Bot *remainingBot : unassignedBots) {

        if (remainingBot->getType() != UnitType::WORKER)
        {
            Squad s{};
            s.getAgents().push_back(remainingBot);
            s.setArchetype(Archetype::ROADMAKER);
            newSquads.emplace_back(s);
            continue;
        }

        lateAssign++;

        // 3/6 citizen
        if (lateAssign % 6 <= 2) {
            newSquads.emplace_back(std::vector<Bot *>{ remainingBot }, Archetype::CITIZEN, pathing::getBestExpansionLocation(gameState->map.getTileIndex(remainingBot->getX(), remainingBot->getY()), gameState));
            continue;
        }
        // 1 out of 2/6 farmer
        if (lateAssign % 6 == 3){
            newSquads.emplace_back(std::vector<Bot *>{ remainingBot }, Archetype::FARMER, pathing::getBestCityFeedingLocation(gameState->map.getTileIndex(remainingBot->getX(), remainingBot->getY()), gameState));
            continue;
        }
        // 1/6 settler
        if(lateAssign % 6 == 4) {
            tileindex_t targetCity = pathing::getBestCityBuildingLocation(gameState->map.getTileIndex(remainingBot->getX(), remainingBot->getY()), gameState);
            if(targetCity != (tileindex_t)-1) {
                newSquads.emplace_back(std::vector<Bot*>{ remainingBot }, Archetype::SETTLER, targetCity);
                continue;
            }
        }
        // 2 out of 2/6 farmer
        newSquads.emplace_back(std::vector<Bot *>{ remainingBot }, Archetype::FARMER, pathing::getBestCityFeedingLocation(gameState->map.getTileIndex(remainingBot->getX(), remainingBot->getY()), gameState));
    }

    for (int i = 0; i < unCreatedBots; i++) {

        Squad squadToBe{};

        lateAssign++;

        tileindex_t weakestCityTile{};
        tileindex_t strongestCityTile{};
        float minCityScore = std::numeric_limits<float>::max();
        float maxCityScore = std::numeric_limits<float>::lowest();
        auto cities = getCityClusters(*gameState)[Player::ALLY];
        for (auto city : cities) {
            if (minCityScore > city.cityTileCount)
            {
                minCityScore = static_cast<float>(city.cityTileCount);
                weakestCityTile = gameState->map.getTileIndex(city.center_x, city.center_y);
            }
            if(maxCityScore < city.cityTileCount)
            {
                maxCityScore = static_cast<float>(city.cityTileCount);
                strongestCityTile = gameState->map.getTileIndex(city.center_x, city.center_y);
            }
        }

        // 3/6 citizen
        if (lateAssign % 6 <= 2) {
            std::pair<std::pair<int, int>, UnitType> newBot{ gameState->map.getTilePosition(pathing::getBestExpansionLocation(weakestCityTile, gameState)), UnitType::WORKER };
            squadToBe.getAgentsToCreate().emplace_back(newBot);
            newSquads.emplace_back(squadToBe);
            continue;
        }
        // 1 out of 2/6 farmer
        if (lateAssign % 6 == 3) {
            std::pair<std::pair<int, int>, UnitType> newBot{ gameState->map.getTilePosition(pathing::getBestCityFeedingLocation(strongestCityTile, gameState)), UnitType::WORKER };
            squadToBe.getAgentsToCreate().emplace_back(newBot);
            newSquads.emplace_back(squadToBe);
            continue;
        }
        // 1/6 settler
        if (lateAssign % 6 == 4) {
            tileindex_t targetCity = pathing::getBestCityBuildingLocation(strongestCityTile, gameState);
            if (targetCity != (tileindex_t)-1) {
                std::pair<std::pair<int, int>, UnitType> newBot{ gameState->map.getTilePosition(targetCity), UnitType::WORKER };
                squadToBe.getAgentsToCreate().emplace_back(newBot);
                newSquads.emplace_back(squadToBe);
                continue;
            }
        }
        // 2 out of 2/6 farmer
        std::pair<std::pair<int, int>, UnitType> newBot{ gameState->map.getTilePosition(pathing::getBestCityFeedingLocation(strongestCityTile, gameState)), UnitType::WORKER };
        squadToBe.getAgentsToCreate().emplace_back(newBot);
        newSquads.emplace_back(squadToBe);
    }

    return newSquads;
}

void Squad::sendReinforcementsRequest(std::vector<Bot *> &cities, int &availableUnits)
{
    std::vector<std::pair<std::pair<int, int>, UnitType>> notReservedBots{};
    if (cities.size() <= 0) return;
    for (auto bot : m_agentsToCreate) {
        Bot *nearestCity = cities[0];
        unsigned int nearestDistance = std::numeric_limits<unsigned int>::max();
        bool foundCity = false;
        for (Bot *city : cities) {
            unsigned int botToCityDist = abs(bot.first.first - city->getX()) + abs(bot.first.second - city->getY());
            if (botToCityDist < nearestDistance && !city->getReserveState()) {
                nearestCity = city;
                nearestDistance = botToCityDist;
                foundCity = true;
            }
        }
        if (foundCity) {
            nearestCity->reserve(bot.second);
            m_agentsInCreation.emplace_back(nearestCity, bot.second);
        } else {
            notReservedBots.push_back(bot);
        }
    }
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

