#ifndef BEHAVIOR_TREE_NAMES_H
#define BEHAVIOR_TREE_NAMES_H

#include <string>

// TODO find a way to define names as std::string in a header file *without* using static
#define DEF_BLACKBOARD_ENTRY(name) static const std::string name = #name

namespace blackboard_names
{

// global-scope entries
DEF_BLACKBOARD_ENTRY(GLOBAL_MAP); // Map*
DEF_BLACKBOARD_ENTRY(GLOBAL_TURN); // size_t
DEF_BLACKBOARD_ENTRY(GLOBAL_ORDERS_LIST); // std::vector<TurnOrder>*

// agent-scope entries
DEF_BLACKBOARD_ENTRY(AGENT_SELF); // Bot*
DEF_BLACKBOARD_ENTRY(AGENT_PATHFINDING_GOAL); // tileindex_t
DEF_BLACKBOARD_ENTRY(AGENT_PATHFINDING_PATH); // std::vector<tileindex_t>

}

namespace bbn = blackboard_names;

#endif