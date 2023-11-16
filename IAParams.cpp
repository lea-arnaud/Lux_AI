#include "IAParams.h"

// Max number of turns to keep for ennemy pathing  
int params::ennemyPathingTurn = 50;

// # Enemy squad detection

// ## Similarity of paths
// Percentage of similarity needed for two paths to be considered as part of the same squad 
float params::similarPercentage = 80.f;
// Power for the difference between two path points value for similarity (this difference is always <= 1)
float params::similarityTolerance = 3.f;

// ## Path linking resource and city detection
// Percentage of the InfluenceMap of the resource/city needed to be considered as linking
float params::resourceCoverageNeeded = 1.f;
float params::cityCoverageNeeded = 50.f;

// ## Enemy approach detection
// Step in turns between points compared to see if the enemy is approaching
int params::pathStep = 5;