#include "InfluenceMap.h"

#include <algorithm>
#include <execution>

InfluenceMap& InfluenceMap::propagate(tileindex_t index, float initialInfluence, float(*propagationFunction)(float, float))
{
  auto [x1, y1] = getCoord(index);

  for (int y2 = 0; y2 < m_height; ++y2) {
    for (int x2 = 0; x2 < m_width; ++x2) {
      m_map[index] += propagationFunction(initialInfluence, static_cast<float>(std::abs(x1 - x2) + std::abs(y1 - y2)));
    }
  }

  return *this;
}

InfluenceMap& InfluenceMap::setValueAtIndex(tileindex_t index, float value)
{
  m_map[index] = value;
  return *this;
}

InfluenceMap& InfluenceMap::addValueAtIndex(tileindex_t index, float value)
{
  m_map[index] += value;
  return *this;
}

InfluenceMap& InfluenceMap::multiplyValueAtIndex(tileindex_t index, float value)
{
  m_map[index] *= value;
  return *this;
}

InfluenceMap& InfluenceMap::addMap(const InfluenceMap &influenceMap, float weight)
{
  std::transform(std::execution::par, m_map.begin(), m_map.end(), influenceMap.m_map.begin(), m_map.begin(),
      [weight](float currentScore, float otherScore) {
    return currentScore + (otherScore * weight);
  });

  return *this;
}

InfluenceMap& InfluenceMap::multiplyMap(const InfluenceMap &influenceMap, float weight)
{
  std::transform(std::execution::par, m_map.begin(), m_map.end(), influenceMap.m_map.begin(), m_map.begin(),
      [weight](float currentScore, float otherScore) {
    return currentScore * (otherScore * weight);
  });

  return *this;
}

InfluenceMap& InfluenceMap::normalize()
{
  // TODO: get the min and max in one loop
  float minVal = *std::min_element(std::execution::par, m_map.begin(), m_map.end());
  float maxVal = *std::max_element(std::execution::par, m_map.begin(), m_map.end());

  if (minVal == maxVal)
    return *this;

  std::transform(std::execution::par, m_map.begin(), m_map.end(), m_map.begin(),
    [minVal, maxVal](float score) { return (score - minVal) / (maxVal - minVal); });

  return *this;
}

InfluenceMap& InfluenceMap::flip()
{
  std::transform(std::execution::par, m_map.begin(), m_map.end(), m_map.begin(),
      [](float score) { return 1.0f - score; });

  return *this;
}

tileindex_t InfluenceMap::getHighestPoint() const
{
  auto maxIterator = std::max_element(std::execution::par, m_map.begin(), m_map.end());
  return static_cast<int>(std::distance(m_map.begin(), maxIterator));
}

std::vector<tileindex_t> InfluenceMap::getNHighestPoints(int n) const
{
  // Create a vector of pairs where the first element of the pair is the value
  // and the second element is the index
  std::vector<std::pair<float, tileindex_t>> indexedVec;
  indexedVec.reserve(m_map.size());
  for (tileindex_t i = 0; i < m_map.size(); ++i) {
    indexedVec.emplace_back(m_map[i], i);
  }

  // Partially sort the indexed vector based on the values
  std::ranges::partial_sort(indexedVec, indexedVec.begin() + n, std::greater());

  std::vector<tileindex_t> resultIndices(n);
  for (tileindex_t i = 0; i < n; ++i) {
    resultIndices[i] = indexedVec[i].second;
  }

  return resultIndices;
}