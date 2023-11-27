
#include <vector>
#include "Action.h"
#include "Node.h"
#include "GameState.h"

#ifndef PLANNER_H
#define PLANNER_H

namespace goap
{

class Planner
{
private:
	std::vector<Node> open_;  
	std::vector<Node> closed_; 

	bool memberOfClosed(const GameState &gs) const; //Is this GameState a member of the closed list ?
	std::vector<goap::Node>::iterator memberOfOpen(const GameState &gs); //Is this GameState a member of the opened list ?
	Node &popAndClose(); // Pops the first Node from the 'open' list, moves it to the 'closed' list
	void addToOpenList(Node &&); // Moves the given Node into the 'open' list.
	int calculateHeuristic(const GameState &now, const GameState &gs) const; // Given two worldstates, calculates an estimated distance between the two


public:
	std::vector<Action> plan(const GameState &start, const GameState &goal, const std::vector<Action> &actions);
};


}

#endif

