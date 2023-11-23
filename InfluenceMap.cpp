#include "InfluenceMap.h"

#include <algorithm>

#ifdef _DEBUG
std::mt19937 g_randomEngine{ 0 };
#else
std::mt19937 g_randomEngine{ std::random_device{}()};
#endif

void InfluenceMap::setSize(int width, int height)
{
  m_width = width;
  m_height = height;
  m_map.clear();
  m_map.resize(width * height, 0.0f);
}

void InfluenceMap::propagate(tileindex_t index, float initialInfluence, float(*propagationFunction)(float, float))
{
  auto [x1, y1] = getCoord(index);

  for (int y2 = 0; y2 < m_height; ++y2) {
    for (int x2 = 0; x2 < m_width; ++x2) {
      m_map[index] += propagationFunction(initialInfluence, static_cast<float>(std::abs(x1 - x2) + std::abs(y1 - y2)));
    }
  }
}

void InfluenceMap::setValueAtIndex(tileindex_t index, float value)
{
  m_map[index] = value;
}

void InfluenceMap::addValueAtIndex(tileindex_t index, float value)
{
  m_map[index] += value;
}

void InfluenceMap::multiplyValueAtIndex(tileindex_t index, float value)
{
  m_map[index] *= value;
}

void InfluenceMap::addMap(const InfluenceMap &influenceMap, float weight)
{
  std::transform(m_map.begin(), m_map.end(), influenceMap.m_map.begin(), m_map.begin(),
      [weight](float currentScore, float otherScore) {
    return currentScore + (otherScore * weight);
  });
}

void InfluenceMap::multiplyMap(const InfluenceMap &influenceMap, float weight)
{
  std::transform(m_map.begin(), m_map.end(), influenceMap.m_map.begin(), m_map.begin(),
      [weight](float currentScore, float otherScore) {
    return currentScore * (otherScore * weight);
  });
}

void InfluenceMap::normalize()
{
  float minVal = std::numeric_limits<float>::max();
  float maxVal = std::numeric_limits<float>::lowest();
  for(float val : m_map) {
    minVal = std::min(val, minVal);
    maxVal = std::max(val, maxVal);
  }
  if (minVal == maxVal)
    return;

  std::ranges::transform(m_map, m_map.begin(),
                         [minVal, maxVal](float score) { return (score - minVal) / (maxVal - minVal); });
}

void InfluenceMap::flip()
{
  std::ranges::transform(m_map, m_map.begin(),
                         [](float score) { return 1.0f - score; });
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
  for (int i = 0; i < getSize(); i++)
    similarity += std::max(0.f, 1 - std::powf(std::abs(m_map[i] - map2.m_map[i]), similarityTolerance));
  return similarity * 100.f / getSize();
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

bool InfluenceMap::covers(const InfluenceMap &mapToCover, float coverageNeeded) const
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

bool InfluenceMap::approachesPoint(int tile_x, int tile_y, int length, int step) const
{
  // we collect the points corresponding to each turn - i * step, for i in [|0, length/step|]
  std::vector<std::pair<size_t, float>> distances{};
  // we seek in the map
  for (tileindex_t i = 0; i < getSize(); i++)
  {
    auto [x, y] = getCoord(i);
    // we seek the value
    for (size_t j = 0; j < length; j += step) {
      // this should be verified for one tile only, so we're gonna break once it is verified
      if (m_map[i] <= j / length && m_map[i] > (j-1)/length) {
        distances.emplace_back(j, std::sqrtf((float)((tile_x - x) * (tile_x - x) + (tile_y - y) * (tile_y - y))));
        break;
      }
    }
  }
  std::ranges::sort(distances);
  for (size_t i = 1; i < distances.size(); i++)
    if (distances[i-1].second > distances[i].second)
      return false;
  return true;
}

std::pair<unsigned, unsigned> InfluenceMap::getRandomValuedPoint() const
{
  std::vector<tileindex_t> valuedPoints{};
  for (int i = 0; i < getSize(); i++)
    if (m_map[i] > 0)
      valuedPoints.push_back(i);
  return getCoord(valuedPoints[g_randomEngine()%valuedPoints.size()]);
}
