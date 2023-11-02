#ifndef INFLUENCE_MAP_H
#define INFLUENCE_MAP_H

#include <array>
#include <type_traits>
#include <vector>
#include <algorithm>
#include <string>

template <class T>
constexpr T absolute(const T &x, std::enable_if_t<std::is_arithmetic_v<T>> * = nullptr)
{
  return x < 0 ? -x : x;
}

template <unsigned int W, unsigned int H>
class InfluenceTemplate
{
  friend class InfluenceMap;
  std::array<float, W * H> m_map;

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

  std::string toJson() const
  {
    std::string jsonStr = "{\n\t\"w\" : " + std::to_string(W) + ",\n\t\"h\" : " + std::to_string(H) + ",\n";
    jsonStr += "\t\"map\": [";
    for (size_t i = 0; i < m_map.size(); ++i) {
      jsonStr += std::to_string(m_map[i]);
      if (i < m_map.size() - 1) {
        jsonStr += ", ";
      }
    }
    jsonStr += "]\n}";
    return jsonStr;
  }
};

// TODO: rajouter des verifications

class InfluenceMap
{
  int m_width{}, m_height{};
  std::vector<float> m_map;

public:
  InfluenceMap() = default;
  InfluenceMap(int width, int height) : m_width{ width }, m_height{ height }
  {
    m_map = std::vector<float>(width * height, 0.0f);
  }

  void setSize(int width, int height)
  {
    m_width = width;
    m_height = height;

    m_map.resize(width * height, 0.0f);
  }

  int getIndex(int x, int y) const { return x + y * m_width; }
  std::pair<int, int> getCoord(int index) const { return { index % m_width, index / m_width }; }
  std::pair<int, int> getCenter() const { return { m_width / 2, m_height / 2 }; }
  int getSize() const { return m_width * m_height; }

  void clear() { std::fill(m_map.begin(), m_map.end(), 0.0f); }

  void propagate(int index, float initialInfluence, float (*propagationFunction)(float, float))
  {
    auto [x1, y1] = getCoord(index);

    for (int y2 = 0; y2 < m_height; ++y2) {
      for (int x2 = 0; x2 < m_width; ++x2) {
        m_map[index] += propagationFunction(initialInfluence, static_cast<float>(std::abs(x1 - x2) + std::abs(y1 - y2)));
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
  void addTemplateAtIndex(int index, const InfluenceTemplate<W, H> &influenceTemplate, float weight = 1.0f)
  {
    int deltaX = index % W - W / 2;
    int deltaY = index / W - H / 2;

    for (int i = 0; i < W * H; ++i) {
      int x = i % W + deltaX;
      int y = i / W + deltaY;

      if (x < 0 || x >= m_width || y < 0 || y >= m_height) continue;

      m_map[getIndex(x, y)] += influenceTemplate.m_map[i] * weight;
    }
  }

  template <unsigned int W, unsigned int H>
  void multiplyTemplateAtIndex(int index, const InfluenceTemplate<W, H> &influenceTemplate, float weight = 1.0f)
  {
    int deltaX = index % W - W / 2;
    int deltaY = index / W - H / 2;

    for (int i = 0; i < W * H; ++i) {
      int x = i % W + deltaX;
      int y = i / W + deltaY;

      if (x < 0 || x >= m_width || y < 0 || y >= m_height) continue;

      m_map[getIndex(x, y)] += influenceTemplate.m_map[i] * weight;
    }
  }

  void normalize()
  {
    // TODO: get the min and max in one loop
    float minVal = *std::min_element(m_map.begin(), m_map.end());
    float maxVal = *std::max_element(m_map.begin(), m_map.end());

    if (minVal == maxVal)
      return;

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

  std::string toJson() const
  {
    std::string jsonStr = "{\n\t\"w\" : " + std::to_string(m_width) + ",\n\t\"h\" : " + std::to_string(m_height) + ",\n";
    jsonStr += "\t\"map\": [";
    for (size_t i = 0; i < m_map.size(); ++i) {
      jsonStr += std::to_string(m_map[i]);
      if (i < m_map.size() - 1) {
        jsonStr += ", ";
      }
    }
    jsonStr += "]\n}";
    return jsonStr;
  }
};

constexpr InfluenceTemplate<9, 9> agentTemplate{ 4, 4, 1.0f,
                                       [](float influence, float distance)
                                       {
                                         return influence - (influence  * distance / 8.0f);
                                       } };

constexpr InfluenceTemplate<9, 9> ressourceTemplate{ 4, 4, 1.0f,
                                       [](float influence, float distance)
                                       {
                                         return influence - (influence  * distance / 8.0f);
                                       } };

constexpr InfluenceTemplate<3, 3> cityTemplate{ 1, 1, 0.0f,
                                       [](float influence, float distance)
                                       {
                                         return distance == 1.0f ? 1.0f : distance == 0.0f ? -4.0f : 0.0f;
                                       } };

#endif