#ifndef INFLUENCE_MAP_H
#define INFLUENCE_MAP_H

#include <array>
#include <vector>
#include <random>
#include <iostream>

#include "Benchmarking.h"
#include "Types.h"

template <class T>
constexpr T absolute(const T &x, std::enable_if_t<std::is_arithmetic_v<T>> * = nullptr)
{
  return x < 0 ? -x : x;
}

template <typename T>
constexpr T power(T num, unsigned int pow, std::enable_if_t<std::is_arithmetic_v<T>> * = nullptr)
{
  return (pow >= sizeof(unsigned int)*8) ? 0 :
    pow == 0 ? 1 : num * power(num, pow - 1);
}

template <unsigned int W, unsigned int H>
class InfluenceTemplate
{
  friend class InfluenceMap;
  std::array<float, W * H> m_map;

public:
  constexpr InfluenceTemplate(int x1, int y1, float influence, float (*propagationFunction)(float, float));
};

template <unsigned W, unsigned H>
constexpr InfluenceTemplate<W, H>::InfluenceTemplate(int x1, int y1, float influence, float(* propagationFunction)(float, float))
{
  for (int i = 0; i < W * H; ++i) {
    int x2 = i % W;
    int y2 = i / W;
    int distance = absolute<int>(x1 - x2) + absolute<int>(y1 - y2);
    m_map[i] = propagationFunction(influence, static_cast<float>(distance));
  }
}

class InfluenceMap
{
  int m_width{}, m_height{};
  std::vector<float> m_map;

public:
  InfluenceMap() = default;
  InfluenceMap(int width, int height)
    : m_width{ width }, m_height{ height }, m_map(static_cast<size_t>(width) *height, 0.f)
  {}

  InfluenceMap(const InfluenceMap &) = default;
  InfluenceMap &operator=(const InfluenceMap &) = default;
  InfluenceMap(InfluenceMap &&);
  InfluenceMap &operator=(InfluenceMap &&);

  void swap(InfluenceMap &);

  void setSize(int width, int height);

  tileindex_t getIndex(int x, int y) const { return x + y * m_width; }
  float getValue(int x, int y) const { return getValue(getIndex(x, y)); }
  float getValue(tileindex_t index) const { return m_map[index]; }
  std::pair<int, int> getCoord(tileindex_t index) const { return { index % m_width, index / m_width }; }
  std::pair<int, int> getCenter() const { return { m_width / 2, m_height / 2 }; }
  int getSize() const { return m_width * m_height; }
  int getWidth() const { return m_width; }
  int getHeight() const { return m_height; }

  void clear() { std::ranges::fill(m_map, 0.0f); }

  InfluenceMap& propagate(tileindex_t index, float initialInfluence, float (*propagationFunction)(float, float), int range);
  InfluenceMap& setValueAtIndex(tileindex_t index, float value);
  InfluenceMap& addValueAtIndex(tileindex_t index, float value);
  InfluenceMap& multiplyValueAtIndex(tileindex_t index, float value);

  InfluenceMap& addMap(const InfluenceMap &influenceMap, float weight = 1.0f);
  InfluenceMap& multiplyMap(const InfluenceMap &influenceMap, float weight = 1.0f);

  template <unsigned int W, unsigned int H>
  InfluenceMap& addTemplateAtIndex(tileindex_t index, const InfluenceTemplate<W, H> &influenceTemplate, float weight = 1.0f);

  template <unsigned int W, unsigned int H>
  InfluenceMap& multiplyTemplateAtIndex(tileindex_t index, const InfluenceTemplate<W, H> &influenceTemplate, float weight = 1.0f);

  InfluenceMap& normalize();
  InfluenceMap& flip();

  tileindex_t getHighestPoint() const;
  std::vector<tileindex_t> getNHighestPoints(int n) const;

  // Use to compare maps of similar size
  float getSimilarity(const InfluenceMap &map2, float similarityTolerance = 1.f) const;

  // Useful only for paths
  std::pair<tileindex_t, tileindex_t> getStartAndEndOfPath();

  bool coversPercentage(const InfluenceMap& mapToCover, float coverageNeeded) const;

  bool coversTiles(const InfluenceMap &mapToCover, int tilesNeeded) const;

  bool approachesPoint(int tile_x, int tile_y, int length, int step) const;

  std::pair<unsigned int, unsigned int> getRandomValuedPoint() const;

  InfluenceMap propagateAllTimes(int n) const;
};

template <unsigned W, unsigned H>
InfluenceMap &InfluenceMap::addTemplateAtIndex(tileindex_t index, const InfluenceTemplate<W, H> &influenceTemplate, float weight)
{
  int deltaX = index % m_width - W / 2;
  int deltaY = index / m_width - H / 2;

  for (int i = 0; i < W * H; ++i) {
    int x = i % W + deltaX;
    int y = i / W + deltaY;

    if (x < 0 || x >= m_width || y < 0 || y >= m_height) continue;

    m_map[getIndex(x, y)] += influenceTemplate.m_map[i] * weight;
  }

  return *this;
}

template <unsigned W, unsigned H>
InfluenceMap &InfluenceMap::multiplyTemplateAtIndex(tileindex_t index, const InfluenceTemplate<W, H> &influenceTemplate, float weight)
{
  int deltaX = index % m_width - W / 2;
  int deltaY = index / m_width - H / 2;

  for (int i = 0; i < W * H; ++i) {
    int x = i % W + deltaX;
    int y = i / W + deltaY;

    if (x < 0 || x >= m_width || y < 0 || y >= m_height) continue;

    m_map[getIndex(x, y)] *= influenceTemplate.m_map[i] * weight;
  }

  return *this;
}

namespace influence_templates
{

constexpr InfluenceTemplate<3, 3> SHAPE_CROSS{ 1, 1, 1.0f,
                                       [](float influence, float distance)
                                       {
                                         return distance <= 1 ? influence : 0;
                                       } };

constexpr InfluenceTemplate<9, 9> AGENT_PROXIMITY{ 4, 4, 1.0f,
                                       [](float influence, float distance)
                                       {
                                         return influence * (1 - distance / 8.0f);
                                       } };

constexpr InfluenceTemplate<5, 5> RESOURCE_PROXIMITY{ 2, 2, 1.0f,
                                       [](float influence, float distance)
                                       {
                                         return influence * (1 - distance / 4.0f);
                                       } };

constexpr InfluenceTemplate<3, 3> CITY_ADJENCY{ 1, 1, 0.0f,
                                       [](float influence, float distance)
                                       {
                                         return distance == 1.0f ? 1.0f : 0.5f;
                                       } };

constexpr InfluenceTemplate<9, 9> ENEMY_CITY_CLUSTER_PROXIMITY{ 4, 4, 1.0f,
                                       [](float influence, float distance)
                                       {
                                         return influence - power(influence * distance / 8.0f, 2);
                                       } };

}

#endif