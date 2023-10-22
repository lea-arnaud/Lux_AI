#ifndef INFLUENCE_MAP_H
#define INFLUENCE_MAP_H

#include <vector>

class InfluenceMap
{
  int m_width{}, m_height{};
  std::vector<float> m_map;

public:
  InfluenceMap(int width, int height);

  std::vector<float> getMap() const { return m_map; };

  void propagate(int index, float initialInfluence, float (*propagationFunction)(float, float) = nullptr);

  void addMap(const InfluenceMap &map, float weight);
  void multiplyMap(const InfluenceMap &map, float weight);

  void normalize();
  void flip();
  int getHighestPoint();
};

#endif