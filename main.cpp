#include "lux/agent.hpp"

#include <ostream>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>

#include "IAParams.h"
#include "Benchmarking.h"
#include "Log.h"
#include "lux/annotate.hpp"
#include "Statistics.h"

#include <stdio.h>

int main()
{
    params::updateParams();
    //std::cerr << "WITH INITIAL TIMEOUT" << std::endl; std::this_thread::sleep_for(std::chrono::seconds(8)); // uncomment to get enough time to attach debugger

    statistics::GameStats{};

    /*std::ifstream paramFile{};

    paramFile.open("./../../parametersFile.txt");
    std::string text{};

    while (std::getline(paramFile,text))
    {
        if (text.find("pathLength=") != std::string::npos)
        {
            params::ennemyPathingTurn = std::stoi(text.substr(11));
        }
        if (text.find("similarityPercentage=") != std::string::npos) {
            params::similarPercentage = std::stof(text.substr(21));
        }
        if (text.find("similarityTolerance=") != std::string::npos) {
            params::similarityTolerance = std::stof(text.substr(20));
        }
        if (text.find("resourceCoverage=") != std::string::npos) {
            params::resourceCoverageNeeded = std::stof(text.substr(17));
        }
        if (text.find("cityCoverage=") != std::string::npos) {
            params::cityCoverageNeeded = std::stof(text.substr(13));
        }
        if (text.find("pathStep=") != std::string::npos) {
            params::pathStep = std::stoi(text.substr(9));
        }
        if (text.find("fileName=") != std::string::npos) {
            params::statPath = text.substr(9);
            LOG("found it ! " + params::statPath);
        }
    }*/

    int playedTurns = 360; // set to only play up to that turn
    if (playedTurns != 360) std::cerr << "WITH MAX TURN PLAYED " << playedTurns << std::endl;

    std::cerr << "Hello game!" << std::endl; // mandatory log for lux-ai-2021 not to crash on log deletion
    kit::Agent agent = kit::Agent();
    agent.Initialize();

    int currentTurn = 0;

    #ifdef BENCHMARKING
    std::string benchmarkFile = "..\\..\\benchmark" + std::to_string(agent.getId()) + ".txt";
    std::ofstream{ benchmarkFile };
    #endif

    try {
        while (true)
        {
            currentTurn++;
            
            #ifdef BENCHMARKING
            benchmark::logs.str("");
            benchmark::logs << "Turn " << currentTurn << "\n";
            #endif

            BENCHMARK_BEGIN(TurnTotal);
            MULTIBENCHMARK_BEGIN(Astar);
            MULTIBENCHMARK_BEGIN(AgentBT);
            MULTIBENCHMARK_BEGIN(getBestCityBuildingLocation);
            MULTIBENCHMARK_BEGIN(getBestCityFeedingLocation);

            BENCHMARK_BEGIN(ExtractGameState);
            agent.ExtractGameState();
            BENCHMARK_END(ExtractGameState);

            std::vector<std::string> orders;
            lux::Annotate::s_logs.swap(orders); // not the cleanest, but for logs this will do
            if(currentTurn < playedTurns) agent.GetTurnOrders(orders);

            //Send orders to game engine
            for (int i = 0; i < orders.size(); i++) {
                if (i != 0)
                    std::cout << ",";
                std::cout << orders[i];
            }
            std::cout << std::endl;
            
            // end turn
            kit::end_turn();
            MULTIBENCHMARK_END(AgentBT);
            MULTIBENCHMARK_END(Astar);
            MULTIBENCHMARK_END(getBestCityBuildingLocation);
            MULTIBENCHMARK_END(getBestCityFeedingLocation);
            BENCHMARK_END(TurnTotal);

            #ifdef BENCHMARKING
            if(BENCHMARK_DURATION(TurnTotal) > 3.) // only log turns that overshoot 3ms
              std::ofstream{ benchmarkFile, std::ios::app } << benchmark::logs.str() << std::endl;
            #endif
        }
    } catch (const std::runtime_error &e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
    }

    return 0;
}
