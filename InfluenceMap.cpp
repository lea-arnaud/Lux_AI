#include "InfluenceMap.h"

#include <algorithm>

InfluenceMap::InfluenceMap(int width, int height, float initialInfluence) :
    m_width{ width },
    m_height{ height }
{
	m_map = std::vector<float>(width * height, initialInfluence);
}

void InfluenceMap::propagate(int index, float initialInfluence, float decay)
{
}

void InfluenceMap::addMap(const InfluenceMap &influenceMap, float weight)
{
    std::transform(m_map.begin(), m_map.end(), influenceMap.getMap().begin(), 
    [weight](float currentScore, float otherScore) {
            return currentScore + (otherScore * weight);
        });
}

void InfluenceMap::multiplyMap(const InfluenceMap &influenceMap, float weight)
{
    std::transform(m_map.begin(), m_map.end(), influenceMap.getMap().begin(), 
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
    return std::distance(m_map.begin(), maxIterator);;
}
