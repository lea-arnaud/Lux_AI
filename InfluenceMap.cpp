#include "InfluenceMap.h"

#include <algorithm>

InfluenceMap::InfluenceMap(int width, int height) :
  m_width{ width },
  m_height{ height }
{
  m_map = std::vector<float>(width * height, 0.0f);
}

void InfluenceMap::propagate(int index, float initialInfluence, float (*propagationFunction)(float, float))
{
  if (!propagationFunction)
    propagationFunction = [](float influence, float distance) { return influence - influence*distance/5.0f; };

  int x1 = index % m_width;
  int y1 = index / m_width;

  for (int i = 0; i < m_map.size(); ++i) {
    int x2 = i % m_width;
    int y2 = i / m_width;
    int distance = std::abs(x1 - x2) + std::abs(y1 - y2);
    m_map[i] += propagationFunction(initialInfluence, static_cast<float>(distance));
  }
}

void InfluenceMap::addMap(const InfluenceMap &influenceMap, float weight)
{
  std::transform(m_map.begin(), m_map.end(), influenceMap.getMap().begin(), m_map.begin(),
  [weight](float currentScore, float otherScore) {
    return currentScore + (otherScore * weight);
  });
}

void InfluenceMap::multiplyMap(const InfluenceMap &influenceMap, float weight)
{
  std::transform(m_map.begin(), m_map.end(), influenceMap.getMap().begin(), m_map.begin(),
  [weight](float currentScore, float otherScore) {
    return currentScore * (otherScore * weight);
  });
}

void InfluenceMap::normalize()
{
  float minVal = *std::min_element(m_map.begin(), m_map.end());
  float maxVal = *std::max_element(m_map.begin(), m_map.end());

  if (minVal == maxVal) return;

  std::for_each(m_map.begin(), m_map.end(), [minVal, maxVal](float score) { return (score - minVal) / (maxVal - minVal); });
}

void InfluenceMap::flip()
{
  std::for_each(m_map.begin(), m_map.end(), [](float score) { return 1.0f - score; });
}

int InfluenceMap::getHighestPoint()
{
  auto maxIterator = std::max_element(m_map.begin(), m_map.end());
  return static_cast<int>(std::distance(m_map.begin(), maxIterator));
}
