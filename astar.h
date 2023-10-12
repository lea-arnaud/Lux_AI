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
	double m_g = std::numeric_limits<double>::max();
	AStarNode* m_parent = nullptr;
	Category m_category = UNVISITED;

	AStarNode() : m_nodeIndex{}, m_f{} {}
	AStarNode(tileindex_t nodeIndex, double fscore) : m_nodeIndex(nodeIndex), m_f(fscore) {}
};

struct AStarNodeCompare
{
	const std::vector<AStarNode> &nodeRecords;

	AStarNodeCompare(const std::vector<AStarNode>& nodes) : nodeRecords{ nodes } {}

	bool operator()(tileindex_t left, tileindex_t right) const
	{
		return nodeRecords[left].m_g > nodeRecords[right].m_g;
	}
};

inline double heuristic(std::pair<int, int> pos1, std::pair<int, int> pos2)
{
	return std::abs(pos1.first - pos2.first) + std::abs(pos1.second - pos2.second);
}

struct AStarExplorationEntry {
  tileindex_t tile;
  double gScore;
};

inline std::vector<tileindex_t> aStar(const Map& map, const Bot& start, tileindex_t goalIndex)
{
	AStarNode currentRecord;
	std::vector<AStarNode> nodeRecords(map.getMapSize());
	for (tileindex_t i = 0; i < nodeRecords.size(); ++i)
		nodeRecords[i] = { i, heuristic(map.getTilePosition(i), map.getTilePosition(goalIndex)) };
	nodeRecords[map.getTileIndex(start)].m_category = OPEN;
	nodeRecords[map.getTileIndex(start)].m_g = 0;

	auto comp = [](const AStarExplorationEntry &e1, const AStarExplorationEntry &e2) { return e1.gScore > e2.gScore; };
	std::priority_queue<AStarExplorationEntry, std::vector<AStarExplorationEntry>, decltype(comp)> openSet(comp);

	openSet.push({ map.getTileIndex(start), 0 });

	// Iterate through processing each node.
	while (!openSet.empty()) {

		tileindex_t currentIndex = openSet.top().tile;
		openSet.pop();
		currentRecord = nodeRecords[currentIndex];

		// If it is the goal, then terminate.
		if (currentIndex == goalIndex) break;
		if (currentRecord.m_category == CLOSED) continue;

		// Otherwise get its outgoing connections.
		for (tileindex_t neighbourIndex : map.getValidNeighbours(currentIndex)) {
			// Get the cost estimate for the neighbor.
			double tentativeG = currentRecord.m_g + 1; // Assuming uniform cost for all edges
			double tentativeF;
			AStarNode &neighbourRecord = nodeRecords[neighbourIndex];

			// If the node is closed we may have to skip.
			if (neighbourRecord.m_category == CLOSED) {
				continue;
			}
			// Skip if the node is open and we've not found a better route.
			else if (neighbourRecord.m_g <= tentativeG) {
				continue;
			}

			tentativeF = heuristic(map.getTilePosition(neighbourIndex), map.getTilePosition(goalIndex));

			// We're here if we need to update the node. Update the cost, estimate and parent
			neighbourRecord.m_g = tentativeG;
			neighbourRecord.m_f = tentativeG + tentativeF;
			neighbourRecord.m_parent = &nodeRecords[currentIndex];
			neighbourRecord.m_category = OPEN;
			openSet.push({ neighbourIndex, neighbourRecord.m_f });
		}

		// We've finished looking at the connections for the current node, so add it to the closed list and remove it from the open list.
		nodeRecords[currentIndex].m_category = CLOSED;
	}

	if (currentRecord.m_nodeIndex != goalIndex) return std::vector<tileindex_t>();

	// Reconstruct the path
	std::vector<tileindex_t> path;

	while (currentRecord.m_parent != nullptr) {
		path.push_back(currentRecord.m_nodeIndex);
		currentRecord = *currentRecord.m_parent;
	}

	path.push_back(currentRecord.m_nodeIndex);

	return path;
}

#endif