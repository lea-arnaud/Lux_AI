#include "Action.h"

goap::Action::Action()
{}

goap::Action::Action(std::string name, int cost)
{}

bool goap::Action::canRun(const GameState & gameState) const
{
	return false;
}

GameState goap::Action::run(const GameState &gameState) const
{
	return GameState();
}
