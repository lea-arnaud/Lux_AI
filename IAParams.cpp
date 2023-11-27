#include "IAParams.h"
#include <string>
#include <stdio.h>
#include <fstream>

namespace params
{
 
// # Path where the stat files are created
std::string statPath = "default.txt";

// # Max number of turns to keep for ennemy pathing  
int ennemyPathingTurn = 50;

// # Enemy squad detection

// ## Similarity of paths
// Percentage of similarity needed for two paths to be considered as part of the same squad 
float similarPercentage = 40.f;
// Power for the difference between two path points value for similarity (this difference is always <= 1)
float similarityTolerance = 4.f;
// Propagation of paths radius
int propagationRadius = 2;

// ## Path linking resource and city detection - TILES VERSION
// Percentage of the InfluenceMap of the resource/city needed to be considered as linking
int resourceTilesNeeded = 1;
int cityTilesNeeded = 1;

// ## Path linking resource and city detection - PERCENTAGE VERSION (probably less effective)
// Percentage of the InfluenceMap of the resource/city needed to be considered as linking
float resourceCoverageNeeded = 1.f;
float cityCoverageNeeded = 50.f;

// ## Enemy approach detection
// Step in turns between points compared to see if the enemy is approaching
int pathStep = 5;

// ENABLE/DISABLE TRAINING PROCESSUS
bool trainingMode = true;

void updateParams()
{
    std::ifstream paramFile{};

    paramFile.open("./../../parametersFile.txt");
    std::string text{};

    while (std::getline(paramFile, text)) {
        if (text.find("pathLength=") != std::string::npos) {
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
        }
    }
}

}