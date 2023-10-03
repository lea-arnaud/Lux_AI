#ifndef agent_h
#define agent_h

#include "lux/kit.hpp"

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
    };
}
#endif
