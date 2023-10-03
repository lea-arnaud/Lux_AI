#include "lux/agent.hpp"

#include <ostream>
#include <string>
#include <iostream>
#include <vector>

int main()
{
    kit::Agent agent = kit::Agent();
    agent.Initialize();

    while (true)
    {
        agent.ExtractGameState();

        std::vector<std::string> orders = std::vector<std::string>();
        agent.GetTurnOrders(orders);

        //Send orders to game engine
        for (int i = 0; i < orders.size(); i++)
        {
            if (i != 0)
            {
                std::cout << ",";
            }
        
            std::cout << orders[i];
        }
        std::cout << std::endl;
    
        // end turn
        kit::end_turn();
    }

    return 0;
}
