#ifndef COMMAND_CHAIN_H
#define COMMAND_CHAIN_H

#include <utility>

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
	MAKE_ROAD,
  } type;

  tileindex_t targetTile;
};

enum Archetype
{
	SETTLER, //constructs cityTiles outside main cities
	CITIZEN, //constructs cityTiles inside main cities
	FARMER, //collects resources to bring to cities
	ROADMAKER, //creates roads
	TROUBLEMAKER, //destroys and/or blocks roads
	KILLER //besieges units and/or cities
};

class Squad
{
public:
	Squad(std::vector<Bot *> bots, Archetype order) : m_agents(std::move(bots)), type(order) {}
	std::vector<Bot *> &getAgents() { return m_agents; }
	Archetype getArchetype() const { return type; }

private:
	std::vector<Bot *> m_agents;
	Archetype type;
};

struct SquadRequirement
{
	size_t botNb;
	size_t cartNb;
	size_t priority;
	unsigned int start_x;
	unsigned int start_y;
	unsigned int dest_x;
	unsigned int dest_y;
	Archetype mission;
	SquadRequirement(size_t bNb, size_t cNb, size_t p, int x, int y, Archetype order) : botNb(bNb), cartNb(cNb), priority(p), dest_x(x), dest_y(y), mission(order){}
};

struct CityCluster {
	float center_x;
	float center_y;
	size_t cityTileCount;
};

struct EnemySquadInfo
{
	Archetype mission;
	InfluenceMap path;
	unsigned int start_x;
	unsigned int start_y;
	unsigned int dest_x;
	unsigned int dest_y;
	size_t botNb;
	size_t cartNb;
	EnemySquadInfo(size_t bNb, size_t cNb, InfluenceMap path, Archetype order) : botNb(bNb), cartNb(cNb), path(path), mission(order) {}
};

class Strategy
{
public:
	Strategy() = default;

	std::vector<EnemySquadInfo> getEnemyStance(const GameState &gameState);
	std::vector<SquadRequirement> adaptToEnemy(const std::vector<EnemySquadInfo> &enemyStance, const GameState &gameState);
	std::vector<Squad> createSquads(const std::vector<SquadRequirement> &agentRepartition, GameState* gameState);

private:
  int getDistanceToClosestResource(int x, int y, const Map &map);
  std::array<std::vector<CityCluster>, Player::COUNT> getCityClusters(const GameState &gameState);
};

class Commander
{
private:
	std::vector<Squad> m_squads;
	std::shared_ptr<Blackboard> m_globalBlackboard;
	std::vector<EnemySquadInfo> m_previousEnemyStance;

	GameState *m_gameState;

	Strategy currentStrategy;

public:
  Commander();
  void updateHighLevelObjectives(GameState *state, const GameStateDiff &diff);
  std::vector<TurnOrder> getTurnOrders();

  bool shouldUpdateSquads(const GameStateDiff &diff, const std::vector<EnemySquadInfo> &newEnemyStance);
  void rearrangeSquads(const GameStateDiff &diff);

	std::shared_ptr<Blackboard> getBlackBoard() { return m_globalBlackboard; }

  void commandSquads(bool interrupt) {}
};

#endif