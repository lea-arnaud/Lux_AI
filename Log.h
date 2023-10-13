#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <vector>
#include <utility>

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
  for (const T &t : vector) out << t << ",";
  return out;
}

#endif