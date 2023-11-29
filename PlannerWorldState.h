#include <map>
#ifndef PLANNER_WORLD_STATE_H
#define PLANNER_WORLD_STATE_H
namespace goap
{
struct PlannerWorldState
{

	float priority; // useful if this is a goal state, to distinguish from other possible goals
	std::map<int, bool> vars; // the variables that in aggregate describe a worldstate for the planner

	void setVariable(const int var_id, const bool value);
	bool getVariable(const int var_id) const;
	bool meetsGoal(const PlannerWorldState &goal_state) const;
	int distanceTo(const PlannerWorldState &goal_state) const;
	bool operator==(const PlannerWorldState &other) const;

};
}
#endif
