// The following code is based on Chris Powell GOAP project avaible here at https://github.com/cpowell/cppGOAP/tree/develop
#include "Action.h"
#include <stdexcept>

goap::Action::Action() : cost_(0)
{}

goap::Action::Action(std::string name, int cost) : Action()
{
	name_ = name;
	cost_ = cost;

}

//Return true if the given WorldState meets the preconditions of this action
bool goap::Action::canRun(const PlannerWorldState &worldState) const
{
    for (const auto &precond : preconditions) {
        try {
            if (worldState.vars.at(precond.first) != precond.second) {
                return false;
            }
        } catch (const std::out_of_range &) {
            return false;
        }
    }
    return true;
}

//Apply the effect on the next WorldState
goap::PlannerWorldState goap::Action::run(const PlannerWorldState &worldState) const
{
    PlannerWorldState tmp(worldState);
    for (const auto &effect : effects) {
        tmp.setVariable(effect.first, effect.second);
    }
    return tmp;
}
