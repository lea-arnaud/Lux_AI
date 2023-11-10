#include "lux/agent.hpp"

#include <ostream>
#include <string>
#include <iostream>
#include <vector>

#include "Log.h"
#include "lux/annotate.hpp"

int main()
{
    std::cerr << "WITH INITIAL TIMEOUT" << std::endl; std::this_thread::sleep_for(std::chrono::seconds(8)); // uncomment to get enough time to attach debugger
    int playedTurns = 360; // set to only play up to that turn
    if (playedTurns != 360) std::cerr << "WITH MAX TURN PLAYED " << playedTurns << std::endl;

    std::cerr << "Hello game!" << std::endl; // mandatory log for lux-ai-2021 not to crash on log deletion
    kit::Agent agent = kit::Agent();
    agent.Initialize();

    try {
        while (true)
        {
            agent.ExtractGameState();

            std::vector<std::string> orders;
            lux::Annotate::s_logs.swap(orders); // not the cleanest, but for logs this will do
            if(--playedTurns > 0) agent.GetTurnOrders(orders);

            //Send orders to game engine
            for (int i = 0; i < orders.size(); i++) {
                if (i != 0)
                    std::cout << ",";
                std::cout << orders[i];
            }
            std::cout << std::endl;
    
            // end turn
            kit::end_turn();
        }
    } catch (const std::runtime_error &e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        throw;
    }

    return 0;
}
