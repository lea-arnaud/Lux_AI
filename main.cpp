#include "lux/agent.hpp"

#include <ostream>
#include <string>
#include <iostream>
#include <vector>

int main()
{
    std::cerr << "Hello game!" << std::endl; // mandatory log for lux-ai-2021 not to crash on log deletion
    kit::Agent agent = kit::Agent();
    agent.Initialize();

    try {
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
    } catch (const std::runtime_error &e) {
        std::cerr << "Fatal error: " << e.what();
        throw;
    }

    return 0;
}