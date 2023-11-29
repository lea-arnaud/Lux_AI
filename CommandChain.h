#ifndef COMMAND_CHAIN_H
#define COMMAND_CHAIN_H

#include <utility>

#include "Bot.h"
#include "GameState.h"
#include "TurnOrder.h"
#include "Types.h"

struct BotObjective
{
  enum class ObjectiveType
  {
	// worker/cart objectives
    GO_BLOCK_PATH, // move to targetTile
    BUILD_CITY, // fetch resources, go to targetTile and build a city tile
    FEED_CITY, // fetch resources and return to targetTile (a city tile)
	MAKE_ROAD, // move from targetTile to returnTile and back

	// city objectives
	CREATE_CART,
	CREATE_WORKER,
	RESEARCH,
  } type;

  tileindex_t targetTile; // only available for worker/cart objectives
  tileindex_t returnTile; // only available for MAKE_ROAD
};

enum class Archetype
{
	SETTLER, //constructs cityTiles outside main cities
	CITIZEN, //constructs cityTiles inside main cities
	FARMER, //collects resources to bring to cities
	ROADMAKER, //creates roads
	TROUBLEMAKER, //destroys and/or blocks roads
	KILLER, //besieges units and/or cities
	__COUNT,
};

class Squad
{
public:
	Squad() : m_agents{}, m_agentsInCreation{}, m_agentsToCreate{} {};
	Squad(std::vector<Bot *> bots, Archetype type, tileindex_t archetypeTargetTile)
		: m_agents(std::move(bots)), m_type(type), m_targetTile(archetypeTargetTile)
	{}

	std::vector<Bot *> &getAgents() { return m_agents; }
	std::vector<std::pair<std::pair<int, int>, UnitType>> &getAgentsToCreate() { return m_agentsToCreate; }
	std::vector<std::pair<Bot *, UnitType>> &getAgentsInCreation() { return m_agentsInCreation; }
	Archetype getArchetype() const { return m_type; }
	void setArchetype(Archetype a) { m_type = a; }
	tileindex_t getTargetTile() const { return m_targetTile; }
	void setTargetTile(tileindex_t tile) { m_targetTile = tile; }
	tileindex_t getReturnTile() const { return m_returnTile; }
	void setReturnTile(tileindex_t tile) { m_returnTile = tile; }

	void sendReinforcementsRequest(std::vector<Bot *> &cities, int &availableUnits);
	void addCreatedAgents(const GameStateDiff &diff);

	void setOrderGiven(bool b) { orderGiven = b; }
	bool getOrderGiven() { return orderGiven; }


private:
	std::vector<Bot *> m_agents{};
	//Bot + UnitType of agent in creation
	std::vector<std::pair<Bot *, UnitType>> m_agentsInCreation{};
	std::vector<std::pair<std::pair<int, int>, UnitType>> m_agentsToCreate{};
	Archetype m_type;
	tileindex_t m_targetTile;
	//ROADMAKER only
	tileindex_t m_returnTile;
	bool orderGiven = false;
};

struct SquadRequirement
{
	static constexpr tileindex_t ANY_TARGET = -1;

	size_t botNb;
	size_t cartNb;
	size_t priority;
	tileindex_t missionTarget;
	Archetype mission;
	SquadRequirement() = default;
	SquadRequirement(size_t bNb, size_t cNb, size_t p, Archetype mission, tileindex_t missionTarget)
	  : botNb(bNb), cartNb(cNb), priority(p), mission(mission), missionTarget(missionTarget)
	{}
};

struct CityCluster {
	int center_x;
	int center_y;
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
	EnemySquadInfo(size_t bNb, size_t cNb, InfluenceMap path, Archetype order) : botNb(bNb), cartNb(cNb), path(std::move(path)), mission(order) {}
	void setStart(unsigned int x, unsigned int y) { start_x = x; start_y = y; }
	void setStart(std::pair<unsigned int, unsigned int> p) { start_x = p.first; start_y = p.second; }
	void setDestination(unsigned int x, unsigned int y) { dest_x = x; dest_y = y; }
	void setDestination(std::pair<unsigned int, unsigned int> p) { dest_x = p.first; dest_y = p.second; }
};

class Strategy
{
public:
	Strategy() = default;

	std::vector<EnemySquadInfo> getEnemyStance(const GameState &gameState);
	std::pair<int, std::vector<SquadRequirement>> adaptToEnemy(const std::vector<EnemySquadInfo> &enemyStance, const GameState &gameState, std::shared_ptr<Blackboard> blackBoard);
	std::vector<Squad> createSquads(const std::pair<int, std::vector<SquadRequirement>> &squadRequirements, GameState* gameState);

private:
	// creates clusters based on distance, does not quite return the cities provided by lux-ai because
	// two city tiles that are not adjacent but close enough are considered to be in the same cluster
	std::array<std::vector<CityCluster>, Player::COUNT> getCityClusters(const GameState &gameState);
	// { Nbr Citizen, Nbr Farmer } 
	std::pair<int, int> getNbrAgent(const CityCluster &cityCluster, int botsAllowed) const;
};

class Commander
{
private:
	std::vector<Squad> m_squads;
	std::shared_ptr<Blackboard> m_globalBlackboard;
	std::vector<EnemySquadInfo> m_previousEnemyStance;

	GameState *m_gameState;

	Strategy currentStrategy;

	// fields that are passed in the global black board by raw pointers that must be kept alive
	struct {
	  std::vector<tileindex_t> agentsPositions;
	  std::vector<tileindex_t> nonCityPositions;
	} m_blackboardKeepAlive;

public:
	Commander();
	void updateHighLevelObjectives(GameState *state, const GameStateDiff &diff);
	void updateBlackBoard();
	std::vector<TurnOrder> getTurnOrders(const GameStateDiff &diff);

	bool shouldUpdateSquads(const GameStateDiff &diff, const std::vector<EnemySquadInfo> &newEnemyStance);
	void rearrangeSquads(const GameStateDiff &diff);

	std::shared_ptr<Blackboard> getBlackBoard() { return m_globalBlackboard; }
};

#endif