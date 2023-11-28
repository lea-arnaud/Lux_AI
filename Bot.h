#ifndef BOT_H
#define BOT_H

#include <string>

#include "Types.h"
#include "BehaviorTree.h"
#include "Benchmarking.h"

enum class UnitType {
  WORKER = 0,
  CART,
  CITY
};

class Bot
{
public:
  Bot(const std::string &id, UnitType type, player_t team, const std::shared_ptr<BasicBehavior> &behaviorTree)
    : m_id(id)
    , m_type(type)
    , m_team(team)
    , m_blackboard(std::make_shared<Blackboard>())
    , m_behaviorTree(behaviorTree)
  {}

  const std::string &getId() const { return m_id; }
  int getX() const { return m_x; }
  int getY() const { return m_y; }
  player_t getTeam() const { return m_team; }
  UnitType getType() const { return m_type; }
  float getCooldown() const { return m_cooldown; }
  int getWoodAmount() const { return m_wood; }
  int getCoalAmount() const { return m_coal; }
  int getUraniumAmount() const { return m_uranium; }

  void setX(int x) { m_x = x; }
  void setY(int y) { m_y = y; }
  void setCooldown(float cooldown) { m_cooldown = cooldown; }
  void setWoodAmount(int wood) { m_wood = wood; }
  void setCoalAmount(int coal) { m_coal = coal; }
  void setUraniumAmount(int uranium) { m_uranium = uranium; }

  void reserve() { isReserved = true; }
  void acted() { hasCreated = true; }
  void release() { isReserved = false; hasCreated = false; }

  bool getReserveState() const { return isReserved; }
  bool getActedState() const { return hasCreated; }

  Blackboard &getBlackboard() { return *m_blackboard; }
  void act() {
    MULTIBENCHMARK_LAPBEGIN(AgentBT);
    m_behaviorTree->run(m_blackboard);
    MULTIBENCHMARK_LAPEND(AgentBT);
  }

  Bot(const Bot &) = delete;
  Bot &operator=(const Bot &) = delete;
  Bot(Bot &&) = delete;
  Bot &operator=(Bot &&) = delete;

private:
  std::string m_id;
  int         m_x{}, m_y{};
  float       m_cooldown{};
  int         m_wood{}, m_coal{}, m_uranium{};
  UnitType   m_type;
  player_t    m_team;
  std::shared_ptr<Blackboard>    m_blackboard;
  std::shared_ptr<BasicBehavior> m_behaviorTree;
  // city only, helps optimization on unit creation
  bool isReserved = false;
  bool hasCreated = false;
};

#endif