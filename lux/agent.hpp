#ifndef agent_h
#define agent_h

#include "lux/kit.hpp"
#include "command_chain.h"

namespace kit
{
    class Agent
    {
    protected:
        int mID = 0;
        
    public:
        void Initialize();
        void ExtractGameState();
        void GetTurnOrders(std::vector<std::string>& orders);

    private:
        Commander m_commander;
    };
}
#endif
