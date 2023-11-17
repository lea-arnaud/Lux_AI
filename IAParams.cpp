#include "IAParams.h"

namespace params
{
  
// Max number of turns to keep for ennemy pathing  
int ennemyPathingTurn = 50;

// # Enemy squad detection

// ## Similarity of paths
// Percentage of similarity needed for two paths to be considered as part of the same squad 
float similarPercentage = 80.f;
// Power for the difference between two path points value for similarity (this difference is always <= 1)
float similarityTolerance = 3.f;

// ## Path linking resource and city detection
// Percentage of the InfluenceMap of the resource/city needed to be considered as linking
float resourceCoverageNeeded = 1.f;
float cityCoverageNeeded = 50.f;

// ## Enemy approach detection
// Step in turns between points compared to see if the enemy is approaching
int pathStep = 5;

}