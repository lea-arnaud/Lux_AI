
#include <vector>
#include "Action.h"

#ifndef PLANNER_H
#define PLANNER_H

namespace goap
{

class Planner
{
public:
	static std::vector<Action> plan(const GameState &start, const GameState &goal, const std::vector<Action> &actions);
};


}

#endif

