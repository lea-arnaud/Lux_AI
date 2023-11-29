// The following code is based on Chris Powell GOAP project avaible here at https://github.com/cpowell/cppGOAP/tree/develop
#include <PlannerWorldState.h>
#include <Action.h>

#ifndef NODE_H
#define NODE_H

namespace goap
{
struct Node
{
    static int last_id; // a static that lets us assign incrementing, unique IDs to nodes

    PlannerWorldState ws;      // The state of the world at this node.
    int id;             // the unique ID of this node
    int parent_id;      // the ID of this node's immediate predecessor
    int g;              // The A* cost from 'start' to 'here'
    int h;              // The estimated remaining cost to 'goal' form 'here'
    const Action *action;


    Node();
    Node(const PlannerWorldState state, int g, int h, int parent_id, const Action *action);

    int f() const
    {
        return g + h;
    }

    friend std::ostream &operator<<(std::ostream &out, const Node &n);


};
bool operator<(const Node &lhs, const Node &rhs);
}

#endif
