#include "Benchmarking.h"

#ifdef BENCHMARKING

std::stringstream benchmark::logs;

// not ideal... but will do
#undef MULTIBENCHMARK_DEFINE
#define MULTIBENCHMARK_DEFINE(name) long long benchmark::__##name##_total, benchmark::__##name##_count;

MULTIBENCHMARK_DEFINE(Astar);
MULTIBENCHMARK_DEFINE(AgentBT);
MULTIBENCHMARK_DEFINE(getBestCityBuildingLocation);
MULTIBENCHMARK_DEFINE(getBestCityFeedingLocation);

#endif