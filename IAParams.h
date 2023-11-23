#pragma once
#include <string>


namespace params
{
  // # Path where the stat files are created
  extern std::string statPath;

  // # Max number of turns to keep for ennemy pathing  
  extern int ennemyPathingTurn;

  // # Enemy squad detection

  // ## Similarity of paths
  // Percentage of similarity needed for two paths to be considered as part of the same squad 
  extern float similarPercentage;
  // Power for the difference between two path points value for similarity (this difference is always <= 1)
  extern float similarityTolerance;

  // ## Path linking resource and city detection
  // Percentage of the InfluenceMap of the resource/city needed to be considered as linking
  extern float resourceCoverageNeeded;
  extern float cityCoverageNeeded;
  
  // ## Enemy approach detection
  // Step in turns between points compared to see if the enemy is approaching
  extern int pathStep;

  void updateParams();
}