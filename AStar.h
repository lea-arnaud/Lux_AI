#ifndef ASTAR_H
#define ASTAR_H

#include <vector>
#include <queue>

#include "Map.h"
#include "lux\kit.hpp"
#include "Bot.h"

enum Category { CLOSED = 0, OPEN, UNVISITED };

struct AStarNode
{
	tileindex_t nodeIndex;
	// Estimated total cost
	double f;
	// Cost so far
	double g = std::numeric_limits<double>::max();
	AStarNode* parent = nullptr;
	Category category = UNVISITED;

	AStarNode() : nodeIndex{}, f{} {}
	AStarNode(tileindex_t nodeIndex, double fscore) : nodeIndex(nodeIndex), f(fscore) {}
};

inline double heuristic(std::pair<int, int> pos1, std::pair<int, int> pos2)
{
	return std::abs(pos1.first - pos2.first) + std::abs(pos1.second - pos2.second);
}

struct AStarExplorationEntry {
  tileindex_t tile;
  double fScore;
};

inline std::vector<tileindex_t> aStar(const Map &map, const Bot &start, tileindex_t goalIndex, const std::vector<tileindex_t> &agentsPosition, pathflags_t pathFlags)
{
	AStarNode currentRecord;
	std::vector<AStarNode> nodeRecords(map.getMapSize());
	for (tileindex_t i = 0; i < nodeRecords.size(); ++i)
		nodeRecords[i] = { i, heuristic(map.getTilePosition(i), map.getTilePosition(goalIndex)) };
	nodeRecords[map.getTileIndex(start)].category = OPEN;
	nodeRecords[map.getTileIndex(start)].g = 0;

	auto comp = [](const AStarExplorationEntry &e1, const AStarExplorationEntry &e2) { return e1.fScore > e2.fScore; };
	std::priority_queue<AStarExplorationEntry, std::vector<AStarExplorationEntry>, decltype(comp)> openSet(comp);

	openSet.push({ map.getTileIndex(start), 0 });

	// Iterate through processing each node.
	while (!openSet.empty()) {

		tileindex_t currentIndex = openSet.top().tile;
		openSet.pop();
		currentRecord = nodeRecords[currentIndex];

		// If it is the goal, then terminate.
		if (currentIndex == goalIndex) break;
		if (currentRecord.category == CLOSED) continue;

		// Otherwise get its outgoing connections.
		for (tileindex_t neighbourIndex : map.getValidNeighbours(currentIndex, pathFlags)) {
			// Get the cost estimate for the neighbor.
			double tentativeG = currentRecord.g + 1 + (Tile::MAX_ROAD - map.tileAt(neighbourIndex).getRoadAmount());

			if (tentativeG <= 25.0f 
			  && map.tileAt(neighbourIndex).getType() != TileType::ALLY_CITY 
			  && std::ranges::find(agentsPosition, neighbourIndex) != agentsPosition.end())
				continue;

			AStarNode &neighbourRecord = nodeRecords[neighbourIndex];

			// If the node is closed we may have to skip.
			if (neighbourRecord.category == CLOSED) {
				continue;
			}
			// Skip if the node is open and we've not found a better route.
			else if (neighbourRecord.g <= tentativeG) {
				continue;
			}

			double tentativeF = heuristic(map.getTilePosition(neighbourIndex), map.getTilePosition(goalIndex));

			// We're here if we need to update the node. Update the cost, estimate and parent
			neighbourRecord.g = tentativeG;
			neighbourRecord.f = tentativeG + tentativeF;
			neighbourRecord.parent = &nodeRecords[currentIndex];
			neighbourRecord.category = OPEN;
			openSet.push({ neighbourIndex, neighbourRecord.f });
		}

		// We've finished looking at the connections for the current node, so add it to the closed list and remove it from the open list.
		nodeRecords[currentIndex].category = CLOSED;
	}

	if (currentRecord.nodeIndex != goalIndex) return std::vector<tileindex_t>();

	// Reconstruct the path
	std::vector<tileindex_t> path;

	while (currentRecord.parent != nullptr) {
		path.push_back(currentRecord.nodeIndex);
		currentRecord = *currentRecord.parent;
	}

	path.push_back(currentRecord.nodeIndex);

	return path;
}

#endif