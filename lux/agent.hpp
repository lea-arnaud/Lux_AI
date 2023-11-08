#ifndef agent_h
#define agent_h

#include "lux/kit.hpp"
#include "CommandChain.h"

namespace kit
{
    class Agent
    {
    public:
        void Initialize();
        void ExtractGameState();
        void GetTurnOrders(std::vector<std::string>& orders);

    private:
        player_t getPlayer(int teamId);
        static kit::ResourceType getResourceType(const std::string &name);

    private:
        int mID = 0;
        int m_mapWidth{}, m_mapHeight{};
        GameState m_gameState{}; // current game state
        GameStateDiff m_gameStateDiff; // game state changes since previous turn
        Commander m_commander;
    };
}
#endif
