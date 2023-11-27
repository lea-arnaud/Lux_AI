#include "Action.h"

goap::Action::Action() : cost_(0)
{}

goap::Action::Action(std::string name, int cost) : Action()
{
	name_ = name;
	cost_ = cost;

}

bool goap::Action::canRun(const GameState & gameState) const
{
	return false;
}

GameState goap::Action::run(const GameState &gameState) const
{
	return GameState();
}
