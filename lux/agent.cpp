#include "lux/agent.hpp"

#include <algorithm>

#include "BehaviorTreeNodes.h"
#include "Benchmarking.h"
#include "IAParams.h"

static const std::shared_ptr<BasicBehavior> BEHAVIOR_WORKER = std::make_shared<BasicBehavior>(nodes::behaviorWorker());
static const std::shared_ptr<BasicBehavior> BEHAVIOR_CART = std::make_shared<BasicBehavior>(nodes::behaviorCart());
static const std::shared_ptr<BasicBehavior> BEHAVIOR_CITY = std::make_shared<BasicBehavior>(nodes::behaviorCity());

namespace kit
{
    void Agent::Initialize()
    {
        mID = std::stoi(kit::getline());
        std::string map_info = kit::getline();

        std::vector<std::string> map_parts = kit::tokenize(map_info, " ");

        m_mapWidth = stoi(map_parts[0]);
        m_mapHeight = stoi(map_parts[1]);
        m_gameState.map.setSize(m_mapWidth, m_mapHeight);
        m_gameState.citiesInfluence.setSize(m_mapWidth, m_mapHeight);
        m_gameState.resourcesInfluence.setSize(m_mapWidth, m_mapHeight);
    }

    void Agent::ExtractGameState()
    {
        GameState &oldState = m_gameState;
        GameState newState;
        GameStateDiff stateDiff;
        newState.currentTurn = oldState.currentTurn + 1;
        newState.map.setSize(m_mapWidth, m_mapHeight);
        newState.citiesInfluence.setSize(m_mapWidth, m_mapHeight);
        newState.resourcesInfluence.setSize(m_mapWidth, m_mapHeight);
        newState.ennemyPath = std::move(oldState.ennemyPath);

        while (true)
        {
            std::string updateInfo = kit::getline();
            if (updateInfo == kit::INPUT_CONSTANTS::DONE)
            {
                break;
            }
            std::vector<std::string> updates = kit::tokenize(updateInfo, " ");
            std::string input_identifier = updates[0];
            if (input_identifier == INPUT_CONSTANTS::RESEARCH_POINTS)
            {
                int team = std::stoi(updates[1]);
                int researchPoints = std::stoi(updates[2]);
                newState.playerResearchPoints[getPlayer(team)] = researchPoints;
            }
            else if (input_identifier == INPUT_CONSTANTS::RESOURCES)
            {
                kit::ResourceType resourceType = getResourceType(updates[1]);
                int x = std::stoi(updates[2]);
                int y = std::stoi(updates[3]);
                int amt = std::stoi(updates[4]);
                newState.map.tileAt(x, y).setResourceAmount(amt);
                newState.map.tileAt(x, y).setType(TileType::RESOURCE, resourceType);
                newState.resourcesIndex.push_back(newState.map.getTileIndex(x, y));

                // TODO: maybe take into account the ResearchPoint
                switch (resourceType) {
                case kit::ResourceType::wood: 
                  newState.resourcesRemaining += amt * game_rules::FUEL_VALUE_WOOD; break;
                case kit::ResourceType::coal: 
                  newState.resourcesRemaining += amt * game_rules::FUEL_VALUE_COAL; break;
                case kit::ResourceType::uranium: 
                  newState.resourcesRemaining += amt * game_rules::FUEL_VALUE_URANIUM; break;
                }
            }
            else if (input_identifier == INPUT_CONSTANTS::UNITS)
            {
                int i = 1;
                UnitType unitType = (UnitType)std::stoi(updates[i++]);
                int team = std::stoi(updates[i++]);
                std::string unitid = updates[i++];
                int x = std::stoi(updates[i++]);
                int y = std::stoi(updates[i++]);
                float cooldown = std::stof(updates[i++]);
                int wood = std::stoi(updates[i++]);
                int coal = std::stoi(updates[i++]);
                int uranium = std::stoi(updates[i++]);

                auto existingAgent = std::ranges::find_if(oldState.bots,
                  [&unitid](const std::unique_ptr<Bot> &agent) { return agent->getId() == unitid; });
                if (existingAgent != oldState.bots.end()) {
                  newState.bots.emplace_back(std::move(*existingAgent));
                  oldState.bots.erase(existingAgent);
                } else {
                  newState.bots.push_back(std::make_unique<Bot>(unitid, unitType, getPlayer(team), unitType == UnitType::CART ? BEHAVIOR_CART : BEHAVIOR_WORKER));
                  stateDiff.newBots.push_back(newState.bots.back().get());
                }

                if (getPlayer(team) == Player::ENEMY) {
                  if (newState.ennemyPath.contains(unitid))
                    newState.ennemyPath[unitid].addMap(newState.ennemyPath[unitid], -1.0f/params::ennemyPathingTurn);
                  else 
                    newState.ennemyPath.insert({ unitid, InfluenceMap{ m_mapWidth, m_mapHeight } });
                  newState.ennemyPath[unitid].addValueAtIndex(newState.map.getTileIndex(x, y), 1.0f);
                }

                std::unique_ptr<Bot> &updatedAgent = newState.bots.back();
                updatedAgent->setX(x);
                updatedAgent->setY(y);
                updatedAgent->setCooldown(cooldown);
                updatedAgent->setWoodAmount(wood);
                updatedAgent->setCoalAmount(coal);
                updatedAgent->setUraniumAmount(uranium);
            }
            else if (input_identifier == INPUT_CONSTANTS::CITY)
            {
                int i = 1;
                int team = std::stoi(updates[i++]);
                std::string cityid = updates[i++];
                float fuel = std::stof(updates[i++]);
                float lightUpkeep = std::stof(updates[i++]);
                newState.cities.push_back(std::make_unique<City>(cityid, getPlayer(team), fuel, lightUpkeep));

                if (getPlayer(team) == Player::ALLY) {
                  newState.resourcesNeeded += lightUpkeep;
                  newState.resourcesOwned += fuel;
                }
            }
            else if (input_identifier == INPUT_CONSTANTS::CITY_TILES)
            {
                int i = 1;
                int team = std::stoi(updates[i++]);
                std::string cityid = updates[i++];
                int x = std::stoi(updates[i++]);
                int y = std::stoi(updates[i++]);
                float cooldown = std::stof(updates[i++]);

                // lux-ai does not provide ids for city units by default
                std::string unitid = "c_" + std::to_string(x) + "_" + std::to_string(y);
                
                auto existingAgent = std::ranges::find_if(oldState.bots,
                  [&unitid](const std::unique_ptr<Bot> &agent) { return agent->getId() == unitid; });
                if (existingAgent != oldState.bots.end()) {
                  newState.bots.emplace_back(std::move(*existingAgent));
                  oldState.bots.erase(existingAgent);
                } else {
                  newState.bots.push_back(std::make_unique<Bot>(unitid, UnitType::CITY, getPlayer(team), BEHAVIOR_CITY));
                  stateDiff.newBots.push_back(newState.bots.back().get());
                }
                std::unique_ptr<Bot> &updatedAgent = newState.bots.back();
                updatedAgent->setX(x);
                updatedAgent->setY(y);
                updatedAgent->setCooldown(cooldown);
                newState.citiesBot.push_back(updatedAgent.get());
                newState.map.tileAt(x, y).setType(getPlayer(team) == Player::ALLY ? TileType::ALLY_CITY : TileType::ENEMY_CITY);
            }
            else if (input_identifier == INPUT_CONSTANTS::ROADS)
            {
                int i = 1;
                int x = std::stoi(updates[i++]);
                int y = std::stoi(updates[i++]);
                float road = std::stof(updates[i++]);
                tileindex_t tile = oldState.map.getTileIndex(x, y);
                newState.map.tileAt(tile).setRoadAmount(road);
                if (oldState.map.tileAt(tile).getRoadAmount() != road)
                    stateDiff.updatedRoads.push_back(tile);
            }
        }

        stateDiff.deadBots = std::move(oldState.bots);

        newState.map.rebuildResourceAdjencies();
        m_gameState = std::move(newState);
        m_gameStateDiff = std::move(stateDiff);
    }

    void Agent::GetTurnOrders(std::vector<std::string>& orders)
    {
        // sense
        // (already done by ExtractGameState)
        // think
        BENCHMARK_BEGIN(computeInfluence);
        m_gameState.computeInfluence(m_gameStateDiff);
        BENCHMARK_END(computeInfluence);
        BENCHMARK_BEGIN(updateHighLevelObjectives);
        m_commander.updateHighLevelObjectives(&m_gameState, m_gameStateDiff);
        BENCHMARK_END(updateHighLevelObjectives);
        // act
        BENCHMARK_BEGIN(getTurnOrders);
        std::vector<TurnOrder> commanderOrders = m_commander.getTurnOrders(m_gameStateDiff);
        BENCHMARK_END(getTurnOrders);
        auto ordersEnd = std::ranges::remove_if(commanderOrders, [](TurnOrder &t) { return t.type == TurnOrder::DO_NOTHING; }).begin();
        std::transform(commanderOrders.begin(), ordersEnd, std::back_inserter(orders), [&](TurnOrder &o) { return o.getAsString(m_gameState.map); });
    }

    player_t Agent::getPlayer(int teamId)
    {
        return teamId == mID ? Player::ALLY : Player::ENEMY;
    }

    kit::ResourceType Agent::getResourceType(const std::string &name)
    {
        if(name == "wood") return kit::ResourceType::wood;
        else if(name == "coal") return kit::ResourceType::coal;
        else if(name == "uranium") return kit::ResourceType::uranium;
        throw std::runtime_error("Got unexpected resource name " + name);
    }
}

