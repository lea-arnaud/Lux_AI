#include "PlannerWorldState.h"
#include <algorithm>
#include <stdexcept>
using namespace goap;

void PlannerWorldState::setVariable(const int var_id, const bool value)
{
    vars[var_id] = value;
}

bool PlannerWorldState::getVariable(const int var_id) const
{
    return vars.at(var_id);
}


bool PlannerWorldState::operator==(const PlannerWorldState &other) const
{
    return (vars == other.vars);
}

//Return true if the current GameState meets the requirement for the goal
bool PlannerWorldState::meetsGoal(const PlannerWorldState &goal_state) const
{
    for (const auto &kv : goal_state.vars) {
        try {
            if (vars.at(kv.first) != kv.second) {
                return false;
            }
        } catch (const std::out_of_range &) {
            return false;
        }
    }
    return true;
}

//Gives how many variables differ from the current state to an other
int PlannerWorldState::distanceTo(const PlannerWorldState &goal_state) const
{
    int result = 0;

    for (const auto &kv : goal_state.vars) {
        auto itr = vars.find(kv.first);
        if (itr == end(vars) || itr->second != kv.second) {
            ++result;
        }
    }

    return result;
}