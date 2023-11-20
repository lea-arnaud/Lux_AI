#ifndef BENCHMARKING_H
#define BENCHMARKING_H

#include <sstream>
#include <chrono>

#define BENCHMARKING

#ifdef BENCHMARKING
#define BENCHMARK_DURATION(name) (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - __##name##_t0).count()/1e6)
#define BENCHMARK_BEGIN(name) auto __##name##_t0 = std::chrono::high_resolution_clock::now()
#define BENCHMARK_END(name) benchmark::logs << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - __##name##_t0).count()/1e6 << "ms for " #name << "\n"
#define MULTIBENCHMARK_DEFINE(name) extern long long __##name##_total, __##name##_count
#define MULTIBENCHMARK_BEGIN(name) benchmark::__##name##_total = 0; benchmark::__##name##_count = 0
#define MULTIBENCHMARK_LAPBEGIN(name) auto __##name##_lapt0 = std::chrono::high_resolution_clock::now()
#define MULTIBENCHMARK_LAPEND(name) ++benchmark::__##name##_count; benchmark::__##name##_total += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - __##name##_lapt0).count()
#define MULTIBENCHMARK_END(name) benchmark::logs << (benchmark::__##name##_total/1e6) << "ms for " #name " x" << benchmark::__##name##_count << "\n"
#else
#define BENCHMARK_BEGIN(name)
#define BENCHMARK_END(name)
#define MULTIBENCHMARK_DEFINE(name)
#define MULTIBENCHMARK_BEGIN(name)
#define MULTIBENCHMARK_LAPBEGIN(name)
#define MULTIBENCHMARK_LAPEND(name)
#define MULTIBENCHMARK_END(name)
#endif

#ifdef BENCHMARKING
namespace benchmark
{

extern std::stringstream logs;

MULTIBENCHMARK_DEFINE(Astar);
MULTIBENCHMARK_DEFINE(AgentBT);
MULTIBENCHMARK_DEFINE(getBestCityBuildingLocation);
MULTIBENCHMARK_DEFINE(getBestCityFeedingLocation);

}
#endif

#endif