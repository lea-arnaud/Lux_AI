#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <vector>
#include <utility>
#include <chrono>

#ifdef _DEBUG
#define LOG(x) std::cerr << x << std::endl
#else
#define LOG(x)
#endif

template<class T, class U>
std::ostream &operator<<(std::ostream &out, const std::pair<T, U> &pair)
{
  return out << pair.first << "," << pair.second;
}

template<class T>
std::ostream &operator<<(std::ostream &out, const std::vector<T> &vector)
{
  for (const T &t : vector) out << t << " ";
  return out;
}

#endif

#define BENCHMARK_BEGIN(name) auto __##name##_t0 = std::chrono::high_resolution_clock::now()
#define BENCHMARK_END(name) std::cerr << #name " took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - __##name##_t0).count() << "ms\n"
#define BENCHMARK_STEP(name, step) std::cerr << #name "#" #step " took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - __##name##_t0).count() << "ms\n"
#define MULTIBENCHMARK_BEGIN(name) long long __##name##_total = 0; long long __##name##_count = 0;
#define MULTIBENCHMARK_LAPBEGIN(name) auto __##name##_lapt0 = std::chrono::high_resolution_clock::now()
#define MULTIBENCHMARK_LAPEND(name) ++__##name##_count; __##name##_total += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - __##name##_lapt0).count();
#define MULTIBENCHMARK_END(name) std::cerr << #name " took " << __##name##_total << "ms and was called " << __##name##_count << " times\n"
