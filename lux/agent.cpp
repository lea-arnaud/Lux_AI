#include "lux/agent.hpp"

#include <algorithm>

#include "BehaviorTreeNodes.h"

static const std::shared_ptr<BasicBehavior> BEHAVIOR_WORKER = std::make_shared<BasicBehavior>(nodes::behaviorWorker());
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
    }

    void Agent::ExtractGameState()
    {
        GameState &oldState = m_gameState;
        GameState newState;
        GameStateDiff stateDiff;
        newState.map.setSize(m_mapWidth, m_mapHeight);

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
            }
            else if (input_identifier == INPUT_CONSTANTS::UNITS)
            {
                int i = 1;
                int unittype = std::stoi(updates[i++]);
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
                  newState.bots.push_back(std::make_unique<Bot>(unitid, (UNIT_TYPE)unittype, getPlayer(team), BEHAVIOR_WORKER)); // TODO add cart behavior
                  stateDiff.newBots.push_back(newState.bots.back().get());
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
                  newState.bots.push_back(std::make_unique<Bot>(unitid, UNIT_TYPE::CITY, getPlayer(team), BEHAVIOR_CITY));
                  stateDiff.newBots.push_back(newState.bots.back().get());
                }
                std::unique_ptr<Bot> &updatedAgent = newState.bots.back();
                updatedAgent->setX(x);
                updatedAgent->setY(y);
                updatedAgent->setCooldown(cooldown);
                updatedAgent->getBlackboard().insertData(bbn::AGENT_SELF, &updatedAgent);
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

        m_gameState = std::move(newState);
        m_gameStateDiff = std::move(stateDiff);
    }

    void Agent::GetTurnOrders(std::vector<std::string>& orders)
    {
        // sense
        // (already done by ExtractGameState)
        // think
        m_commander.updateHighLevelObjectives(&m_gameState, m_gameStateDiff);
        // act
        std::vector<TurnOrder> commanderOrders = m_commander.getTurnOrders();
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

