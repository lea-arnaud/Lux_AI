#include "lux/agent.hpp"

#include <algorithm>

namespace kit
{
    void Agent::Initialize()
    {
        // get agent ID
        mID = std::stoi(kit::getline());
        std::string map_info = kit::getline();

        std::vector<std::string> map_parts = kit::tokenize(map_info, " ");

        int mapWidth = stoi(map_parts[0]);
        int mapHeight = stoi(map_parts[1]);
    }

    void Agent::ExtractGameState()
    {
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
            }
            else if (input_identifier == INPUT_CONSTANTS::RESOURCES)
            {
                std::string type = updates[1];
                int x = std::stoi(updates[2]);
                int y = std::stoi(updates[3]);
                int amt = std::stoi(updates[4]);
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
            }
            else if (input_identifier == INPUT_CONSTANTS::CITY)
            {
                int i = 1;
                int team = std::stoi(updates[i++]);
                std::string cityid = updates[i++];
                float fuel = std::stof(updates[i++]);
                float lightUpkeep = std::stof(updates[i++]);
            }
            else if (input_identifier == INPUT_CONSTANTS::CITY_TILES)
            {
                int i = 1;
                int team = std::stoi(updates[i++]);
                std::string cityid = updates[i++];
                int x = std::stoi(updates[i++]);
                int y = std::stoi(updates[i++]);
                float cooldown = std::stof(updates[i++]);
            }
            else if (input_identifier == INPUT_CONSTANTS::ROADS)
            {
                int i = 1;
                int x = std::stoi(updates[i++]);
                int y = std::stoi(updates[i++]);
                float road = std::stof(updates[i++]);
            }
        }
    }

    void Agent::GetTurnOrders(std::vector<std::string>& orders)
    {
        // sense
        GameState turnState; // TODO fill turn state
        
        // think
        m_commander.updateHighLevelObjectives(turnState);

        // act
        std::vector<TurnOrder> commanderOrders = m_commander.getTurnOrders();
        auto ordersEnd = std::remove_if(commanderOrders.begin(), commanderOrders.end(), [](TurnOrder &t) { return t.type == TurnOrder::DO_NOTHING; });
        orders.resize(std::distance(commanderOrders.begin(), ordersEnd));
        std::transform(commanderOrders.begin(), ordersEnd, orders.begin(), [&](TurnOrder &o) { return o.getAsString(turnState.map); });
    }
}

