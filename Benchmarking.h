#ifndef BENCHMARKING_H
#define BENCHMARKING_H

#include <sstream>
#include <chrono>

#define BENCHMARK_BEGIN(name) auto __##name##_t0 = std::chrono::high_resolution_clock::now()
#define BENCHMARK_END(name) benchmark::logs << #name " took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - __##name##_t0).count() << "ms\n"
#define BENCHMARK_STEP(name, step) benchmark::logs << #name "#" #step " took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - __##name##_t0).count() << "ms\n"
#define MULTIBENCHMARK_BEGIN(name) long long __##name##_total = 0; long long __##name##_count = 0;
#define MULTIBENCHMARK_LAPBEGIN(name) auto __##name##_lapt0 = std::chrono::high_resolution_clock::now()
#define MULTIBENCHMARK_LAPEND(name) ++__##name##_count; __##name##_total += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - __##name##_lapt0).count();
#define MULTIBENCHMARK_END(name) benchmark::logs << #name " took " << __##name##_total << "ms and was called " << __##name##_count << " times\n"

namespace benchmark
{

extern std::stringstream logs;

}

#endif