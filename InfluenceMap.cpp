#include "InfluenceMap.h"
#include <string>
#include <iostream>
#include <algorithm>
#include <execution>

#ifdef _DEBUG
std::mt19937 g_randomEngine{ 0 };
#else
std::mt19937 g_randomEngine{ std::random_device{}()};
#endif

constexpr float PI = 3.14159265359f;

void InfluenceMap::setSize(int width, int height)
{
  m_width = width;
  m_height = height;
  m_map.clear();
  m_map.resize(width * height, 0.0f);
}

InfluenceMap& InfluenceMap::propagate(tileindex_t index, float initialInfluence, float(*propagationFunction)(float, float), int range = 32)
{
  auto [x1, y1] = getCoord(index);

  for (int y2 = 0; y2 < std::min(m_height, range); ++y2) {
    for (int x2 = 0; x2 < std::min(m_width, range); ++x2) {
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
  std::transform(std::execution::seq, m_map.begin(), m_map.end(), influenceMap.m_map.begin(), m_map.begin(),
      [weight](float currentScore, float otherScore) {
    return currentScore + (otherScore * weight);
  });

  return *this;
}

InfluenceMap& InfluenceMap::multiplyMap(const InfluenceMap &influenceMap, float weight)
{
  std::transform(std::execution::seq, m_map.begin(), m_map.end(), influenceMap.m_map.begin(), m_map.begin(),
      [weight](float currentScore, float otherScore) {
    return currentScore * (otherScore * weight);
  });

  return *this;
}

InfluenceMap& InfluenceMap::normalize()
{
  float minVal = std::numeric_limits<float>::max();
  float maxVal = std::numeric_limits<float>::lowest();
  for(float val : m_map) {
    minVal = std::min(val, minVal);
    maxVal = std::max(val, maxVal);
  }
  if (minVal == maxVal)
    return *this;

  std::transform(std::execution::seq, m_map.begin(), m_map.end(), m_map.begin(),
    [minVal, maxVal](float score) { return (score - minVal) / (maxVal - minVal); });

  return *this;
}

InfluenceMap& InfluenceMap::flip()
{
  std::transform(std::execution::seq, m_map.begin(), m_map.end(), m_map.begin(),
      [](float score) { return 1.0f - score; });

  return *this;
}

tileindex_t InfluenceMap::getHighestPoint() const
{
  auto maxIterator = std::ranges::max_element(m_map);
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

  std::vector<tileindex_t> resultIndices;
  resultIndices.reserve(n);
  for (tileindex_t i = 0; i < n && i < indexedVec.size(); ++i)
    resultIndices.push_back(indexedVec[i].second);
  while (resultIndices.size() < n)
    resultIndices.push_back(-1);

  return resultIndices;
}

float InfluenceMap::getSimilarity(const InfluenceMap &map2, float similarityTolerance) const
{
  if (getWidth() != map2.getWidth() || getHeight() != map2.getHeight())
    throw std::runtime_error("Cannot compare maps of different sizes");

  float similarity = 0;
  float count = 0;
  for (int i = 0; i < getSize(); i++) 
  {
      if (m_map[i] == 0 && map2.m_map[i] == 0) continue;
      count++;
      similarity += std::max(0.f, 1 - std::powf(std::abs(m_map[i] - map2.m_map[i]), similarityTolerance));
  }
  return similarity * 100.f / count;
}

std::pair<tileindex_t, tileindex_t> InfluenceMap::getStartAndEndOfPath()
{
  tileindex_t start = 0;
  float sValue = 2;
  tileindex_t end = 0;
  float eValue = 0;

  for (int i = 0; i < getSize(); i++) {
    if (m_map[i] > eValue) {
      end = i;
      eValue = m_map[i];
    }
    if (m_map[i] < sValue) {
      start = i;
      sValue = m_map[i];
    }
  }

  return { start, end };
}

bool InfluenceMap::coversPercentage(const InfluenceMap &mapToCover, float coverageNeeded) const
{
  float total = 0;
  float covered = 0;
  for (int i = 0; i < getSize(); i++) {
    if (mapToCover.m_map[i] > 0) {
      total++;
      if (m_map[i] > 0)
        covered++;
    }
  }
  return covered/total*100.f > coverageNeeded;
}

bool InfluenceMap::coversTiles(const InfluenceMap &mapToCover, int tilesNeeded) const
{
    int covered = 0;
    for (int i = 0; i < getSize(); i++) {
        if (mapToCover.m_map[i] > 0) {
            if (m_map[i] > 0) {
                covered++;
                if (covered >= tilesNeeded)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

bool InfluenceMap::approachesPoint(int tile_x, int tile_y, int length, int step) const
{
  float angle = PI/4.f;

  // we collect the points corresponding to each turn - i * step, for i in [|0, length/step|]
  std::vector<std::pair<tileindex_t, float>> distances{};
  // we seek in the map
  for (tileindex_t i = 0; i < getSize(); i++)
  {
      if (m_map[i] > 0)
          distances.emplace_back(i, m_map[i]);
  }
  std::ranges::sort(distances, std::less{});
  std::pair<double, double> direction{};
  if (distances.size() > step) {
      std::pair<double, double> coords = getCoord(distances[distances.size()-1].first);
      direction.first = (tile_x - coords.first) / sqrt(direction.first * direction.first + direction.second * direction.second);
      direction.second = (tile_y - coords.second) / sqrt(direction.first * direction.first + direction.second * direction.second);
      for (int i = 0; i < std::min(length, (int)distances.size() - step); i += step)
      {
          std::pair<double, double> direction_i{};
          direction_i.first = distances[distances.size()-1-i].first - distances[distances.size()-1-i-step].first;
          direction_i.second = distances[distances.size()-1-i].second - distances[distances.size()-1-i-step].second;
          direction_i.first /= sqrt(direction_i.first * direction_i.first + direction_i.second * direction_i.second);
          direction_i.second /= sqrt(direction_i.first * direction_i.first + direction_i.second * direction_i.second);
          double scalar = direction.first * direction_i.first + direction.second * direction_i.second;
          if (std::abs(std::acos(scalar)) > angle)
              return false;
      }
      return true;
  } else return false;
}

std::pair<unsigned, unsigned> InfluenceMap::getRandomValuedPoint() const
{
  std::vector<tileindex_t> valuedPoints{};
  for (int i = 0; i < getSize(); i++)
    if (m_map[i] > 0)
      valuedPoints.push_back(i);
  return getCoord(valuedPoints[g_randomEngine()%valuedPoints.size()]);
}
