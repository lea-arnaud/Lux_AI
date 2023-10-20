#ifndef COMMAND_CHAIN_H
#define COMMAND_CHAIN_H

#include "Bot.h"
#include "GameState.h"
#include "TurnOrder.h"
#include "BehaviorTreeNames.h"
#include "Types.h"

struct BotObjective
{
  enum class ObjectiveType
  {
    GO_BLOCK_PATH,
    BUILD_CITY,
    FEED_CITY,
  } type;

  tileindex_t targetTile;
};

enum Archetype
{
	SETTLER, //constructs cityTiles outside main cities
	CITIZEN, //constructs cityTiles inside main cities
	FARMER, //collects resources to bring to cities
	ROADMAKER, //creates roads
	TROUBLEMAKER //destroys and/or blocks roads
};

class Squad
{
public:
	std::vector<Bot *> &getAgents() { return m_agents; }

	void commandAgents();

private:
	std::vector<Bot *> m_agents;
	Archetype type;
};

class Strategy
{
public:
	Strategy() {};

	std::map<Archetype, size_t> getEnemyStance();
	std::vector<Squad> adaptToEnemy(std::map<Archetype, size_t> enemyStance) {};
};

class Commander
{
private:
	std::vector<Squad> m_squads;
	std::shared_ptr<Blackboard> m_globalBlackboard;

	Strategy currentStrategy;

public:
  Commander();
  void updateHighLevelObjectives(GameState &state, const GameStateDiff &diff);
  std::vector<TurnOrder> getTurnOrders(GameState &gameState);

  void rearrangeSquads() 
  {
	  m_squads = currentStrategy.adaptToEnemy(currentStrategy.getEnemyStance());
  }

  void commandSquads(bool interrupt) {};
};

#endif