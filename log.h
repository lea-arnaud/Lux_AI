#pragma once

#include <iostream>
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
