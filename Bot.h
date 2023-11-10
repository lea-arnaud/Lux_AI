#ifndef BOT_H
#define BOT_H

#include <string>

#include "Types.h"
#include "BehaviorTree.h"

enum UNIT_TYPE {
  WORKER = 0,
  CART,
  CITY
};

class Bot
{
public:
  Bot(const std::string &id, UNIT_TYPE type, player_t team, const std::shared_ptr<BasicBehavior> &behaviorTree)
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
  UNIT_TYPE getType() const { return m_type; }
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

  Blackboard &getBlackboard() { return *m_blackboard; }
  void act() { m_behaviorTree->run(m_blackboard); }

  Bot(const Bot &) = delete;
  Bot &operator=(const Bot &) = delete;
  Bot(Bot &&) = delete;
  Bot &operator=(Bot &&) = delete;

private:
  std::string m_id;
  int         m_x{}, m_y{};
  float       m_cooldown{};
  int         m_wood{}, m_coal{}, m_uranium{};
  UNIT_TYPE   m_type;
  player_t    m_team;
  std::shared_ptr<Blackboard>    m_blackboard;
  std::shared_ptr<BasicBehavior> m_behaviorTree;
};

#endif