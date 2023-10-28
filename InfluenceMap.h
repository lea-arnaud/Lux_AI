#ifndef INFLUENCE_MAP_H
#define INFLUENCE_MAP_H

#include <array>
#include <type_traits>
#include <algorithm>
#include <vector>

template <class T>
constexpr T absolute(const T &x, std::enable_if_t<std::is_arithmetic_v<T>> * = nullptr)
{
  return x < 0 ? -x : x;
}

template <unsigned int W, unsigned int H>
class InfluenceTemplate
{
  friend class InfluenceMap;
  std::array<float, W *H> m_map;
public:
  constexpr InfluenceTemplate(int x1, int y1, float influence, float (*propagationFunction)(float, float))
  {
    for (int i = 0; i < W * H; ++i) {
      int x2 = i % W;
      int y2 = i / W;
      int distance = absolute<int>(x1 - x2) + absolute<int>(y1 - y2);
      m_map[i] = propagationFunction(influence, static_cast<float>(distance));
    }
  }
};

constexpr InfluenceTemplate<3, 3> test{ 1, 1, 1.0f,
  [](float influence, float distance) {
    return 0.0f;
  } };

// TODO: rajouter des verifications

class InfluenceMap
{
  int m_width{}, m_height{};
  std::vector<float> m_map;

public:
  InfluenceMap(int width, int height) : m_width{ width }, m_height{ height }
  {
    m_map = std::vector<float>(width * height, 0.0f);
  }

  int getIndex(int x, int y) const { return x + y * m_width; }
  std::pair<int, int> getCoord(int index) const { return { index / m_width, index % m_width }; }
  std::pair<int, int> getCenter() const { return { m_width / 2, m_height / 2 }; }
  int getSize() const { return m_width * m_height; }

  void propagate(int x1, int y1, float initialInfluence, float (*propagationFunction)(float, float))
  {
    for (int y2 = 0; y2 < m_height; ++y2) {
      for (int x2 = 0; x2 < m_width; ++x2) {
        m_map[getIndex(x2, y2)] += propagationFunction(initialInfluence, static_cast<float>(std::abs(x1 - x2) + std::abs(y1 - y2)));
      }
    }
  }

  void addMap(const InfluenceMap &influenceMap, float weight = 1.0f)
  {
    std::transform(m_map.begin(), m_map.end(), influenceMap.m_map.begin(), m_map.begin(),
      [weight](float currentScore, float otherScore) {
      return currentScore + (otherScore * weight);
    });
  }

  void multiplyMap(const InfluenceMap &influenceMap, float weight = 1.0f)
  {
    std::transform(m_map.begin(), m_map.end(), influenceMap.m_map.begin(), m_map.begin(),
      [weight](float currentScore, float otherScore) {
      return currentScore * (otherScore * weight);
    });
  }

  template <unsigned int W, unsigned int H>
  void addTemplateAtLocation(int x, int y, const InfluenceTemplate<W, H> &influenceTemplate, float weight = 1.0f)
  {
    int deltaX = x - W / 2;
    int deltaY = y - H / 2;

    for (int i = 0; i < W * H; ++i) {
      int x1 = i / W;
      int y1 = i % W;
      m_map[getIndex(x1 + deltaX, y1 + deltaY)] += influenceTemplate.m_map[i] * weight;
    }
  }

  template <unsigned int W, unsigned int H>
  void multiplyTemplateAtLocation(int x1, int y1, const InfluenceTemplate<W, H> &influenceTemplate, float weight = 1.0f)
  {
    int deltaX = x - W / 2;
    int deltaY = y - H / 2;

    for (int i = 0; i < W * H; ++i) {
      int x1 = i / W;
      int y1 = i % W;
      m_map[getIndex(x1 + deltaX, y1 + deltaY)] *= influenceTemplate.m_map[i] * weight;
    }
  }

  void normalize()
  {
    // TODO: get the min and max in one loop
    float minVal = *std::min_element(m_map.begin(), m_map.end());
    float maxVal = *std::max_element(m_map.begin(), m_map.end());

    if (minVal == maxVal) return;

    std::transform(m_map.begin(), m_map.end(), m_map.begin(),
      [minVal, maxVal](float score) { return (score - minVal) / (maxVal - minVal); });
  }

  void flip()
  {
    std::transform(m_map.begin(), m_map.end(), m_map.begin(),
      [](float score) { return 1.0f - score; });
  }

  int getHighestPoint()
  {
    auto maxIterator = std::max_element(m_map.begin(), m_map.end());
    return static_cast<int>(std::distance(m_map.begin(), maxIterator));
  }
};

#endif