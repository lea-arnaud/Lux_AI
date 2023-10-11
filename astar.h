#ifndef ASTAR_H
#define ASTAR_H

#include "Map.h"
#include "lux\kit.hpp"
#include "Bot.h"
#include <vector>
#include <queue>

enum Category { CLOSED = 0, OPEN, UNVISITED };

struct AStarNode
{
	tileindex_t m_nodeIndex;
	// Estimated total cost
	double m_f;
	// Cost so far
	double m_g;
	AStarNode* m_parent;
	Category m_category;

	AStarNode() : m_f{}, m_g{}, m_parent{ nullptr }, m_category{ UNVISITED } {}

	AStarNode(tileindex_t nodeIndex, double f, double g, AStarNode* parent)
		: m_nodeIndex{ nodeIndex }, m_f{ f }, m_g{ g }, m_parent{ parent }, m_category{ UNVISITED } {}
};

struct AStarNodeCompare
{
	const std::vector<AStarNode> nodeRecords;

	AStarNodeCompare(const std::vector<AStarNode>& nodes) : nodeRecords{ nodes } {}

	bool operator()(int left, int right) const
	{
		return nodeRecords[left].m_g > nodeRecords[right].m_g;
	}
};

inline double heuristic(std::pair<int, int> pos1, std::pair<int, int> pos2)
{
	return std::abs(pos1.first - pos2.first) + std::abs(pos1.second - pos2.second);
}

inline std::vector<tileindex_t> aStar(const Map& map, const Bot& start, tileindex_t goalIndex)
{

	AStarNode currentRecord;
	std::vector<AStarNode> nodeRecords(map.getMapSize());
	for (tileindex_t i = 0; i < nodeRecords.size(); ++i) {
		AStarNode temp{ i, heuristic(map.getTilePosition(i), map.getTilePosition(goalIndex)), 0, nullptr};

		if (i == map.getTileIndex(start)) temp.m_category = OPEN;

		nodeRecords[i] = temp;
	}

	AStarNodeCompare comparator(nodeRecords);
	std::priority_queue<int, std::vector<int>, AStarNodeCompare> openSet(comparator);

	openSet.push(map.getTileIndex(start));

	// Iterate through processing each node.
	while (!openSet.empty()) {

		int currentIndex = openSet.top();
		openSet.pop();
		currentRecord = nodeRecords[currentIndex];

		// If it is the goal, then terminate.
		if (currentIndex == goalIndex) break;

		// Otherwise get its outgoing connections.
		for (tileindex_t neighbourIndex : map.getValidNeighbours(currentIndex)) {
			// Get the cost estimate for the neighbor.
			double tentativeG = currentRecord.m_g + 1; // Assuming uniform cost for all edges
			double tentativeF;

			// If the node is closed we may have to skip, or remove it from the closed list.
			if (nodeRecords[neighbourIndex].m_category == CLOSED) {
				// If we didn't find a shorter route, skip.
				if (nodeRecords[neighbourIndex].m_g <= tentativeG) continue;

				// Otherwise remove it from the closed list.
				nodeRecords[neighbourIndex].m_category = UNVISITED;

				tentativeF = nodeRecords[neighbourIndex].m_f - nodeRecords[neighbourIndex].m_g;
			}
			// Skip if the node is open and we've not found a better route.
			else if (nodeRecords[neighbourIndex].m_category == OPEN) {
				if (nodeRecords[neighbourIndex].m_g <= tentativeG) continue;
				tentativeF = nodeRecords[neighbourIndex].m_f - nodeRecords[neighbourIndex].m_g;
			}
			else {
				tentativeF = heuristic(map.getTilePosition(neighbourIndex), map.getTilePosition(goalIndex));
			}

			// We're here if we need to update the node. Update the cost, estimate and parent
			nodeRecords[neighbourIndex].m_g = tentativeG;
			nodeRecords[neighbourIndex].m_f = tentativeG + tentativeF;
			nodeRecords[neighbourIndex].m_parent = new AStarNode(currentRecord);

			if (nodeRecords[neighbourIndex].m_category != OPEN) {
				nodeRecords[neighbourIndex].m_category = OPEN;
				openSet.push(neighbourIndex);
			}

		}

		// We've finished looking at the connections for the current node, so add it to the closed list and remove it from the open list.
		nodeRecords[currentIndex].m_category = CLOSED;
	}

	if (currentRecord.m_nodeIndex != goalIndex) return std::vector<tileindex_t>();

	// Reconstruct the path
	std::vector<tileindex_t> path;

	while (currentRecord.m_parent != nullptr) {
		path.push_back(currentRecord.m_parent->m_nodeIndex);
		currentRecord = *currentRecord.m_parent;
	}

	std::reverse(path.begin(), path.end());
	return path;
}

#endif