// The following code is based on Chris Powell GOAP project avaible here at https://github.com/cpowell/cppGOAP/tree/develop
#include "Action.h"

goap::Action::Action() : cost_(0)
{}

goap::Action::Action(std::string name, int cost) : Action()
{
	name_ = name;
	cost_ = cost;

}

//Return true if the given GameState meets the preconditions of this action
bool goap::Action::canRun(const GameState & gameState) const
{
    for (const auto &precond : preconditions) {
        try {
            if (gameState.vars.at(precond.first) != precond.second) {
                return false;
            }
        } catch (const std::out_of_range &) {
            return false;
        }
    }
    return true;
}

//Apply the effect on the next GameState
GameState goap::Action::run(const GameState &gameState) const
{
    GameState tmp(gameState);
    for (const auto &effect : effects) {
        tmp.setVariable(effect.first, effect.second);
    }
    return tmp;
}
