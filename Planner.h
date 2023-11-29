// The following code is based on Chris Powell GOAP project avaible here at https://github.com/cpowell/cppGOAP/tree/develop

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
	std::vector<Node> open_;  //open list
	std::vector<Node> closed_; //close list

	bool memberOfClosed(const GameState &gs) const; 
	std::vector<goap::Node>::iterator memberOfOpen(const GameState &gs); 
	Node &popAndClose();
	void addToOpenList(Node &&);
	int calculateHeuristic(const GameState &now, const GameState &gs) const; 


public:
	std::vector<Action> plan(const GameState &start, const GameState &goal, const std::vector<Action> &actions);
};


}

#endif

