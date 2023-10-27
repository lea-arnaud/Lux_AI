#ifndef INFLUENCE_MAP_H
#define INFLUENCE_MAP_H

#include <vector>

// TODO: rajouter des verifications

template <class T, std::size_t N>
   class Tableau {
   public:
      using value_type = T;
      using size_type = std::size_t;
   private:
      value_type vals[N];
   public:
      constexpr Tableau() : vals{ {} }
      {
      }
      constexpr const value_type& operator[](size_type n) const {
         return vals[n];
      }
      constexpr value_type& operator[](size_type n) {
         return vals[n];
      }
   };

class InfluenceMap
{
  int m_width{}, m_height{};
  std::vector<float> m_map;
  //Tableau<float, W * H> m_carte;

public:
  constexpr InfluenceMap(int width, int height);
  constexpr InfluenceMap(int width, int height, float initialInfluence, float (*propagationFunction)(float, float) = nullptr);

  constexpr int getIndex(int x, int y) const { return x + y * m_width; }
  constexpr int getSize() const { return m_width * m_height; }
  constexpr std::pair<int, int> getCenter() const { return {m_width / 2, m_height / 2}; }
  constexpr std::pair<int, int> getCoord(int index) const { return { index / m_width, index % m_width }; }
  std::vector<float> getMap() const { return m_map; };

  constexpr void propagate(int index, float initialInfluence, float (*propagationFunction)(float, float) = nullptr);

  constexpr void addMap(const InfluenceMap &map, float weight = 1.0f);
  constexpr void multiplyMap(const InfluenceMap &map, float weight = 1.0f);

  constexpr void addMapAtIndex(int index, const InfluenceMap &map, float weight = 1.0f);
  constexpr void multiplyMapAtIndex(int index, const InfluenceMap &map, float weight = 1.0f);

  constexpr void normalize();
  constexpr void flip();
  constexpr int getHighestPoint();
};

// constexpr InfluenceMap agentInfluence{ 5, 5, 1.0f };

#endif