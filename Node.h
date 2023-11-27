#include <GameState.h>
#include <Action.h>

#ifndef NODE_H
#define NODE_H

namespace goap
{
struct Node
{
    static int last_id_; // a static that lets us assign incrementing, unique IDs to nodes

    GameState gs_;      // The state of the world at this node.
    int id_;             // the unique ID of this node
    int parent_id_;      // the ID of this node's immediate predecessor
    int g_;              // The A* cost from 'start' to 'here'
    int h_;              // The estimated remaining cost to 'goal' form 'here'
    const Action *action_;     // The action that got us here (for replay purposes)

    Node();
    Node(const GameState state, int g, int h, int parent_id, const Action *action);

    int f() const
    {
        return g_ + h_;
    }

    //        /**
    //         Less-than operator, needed for keeping Nodes sorted.
    //         @param other the other node to compare against
    //         @return true if this node is less than the other (using F)
    //         */
    //        bool operator<(const Node& other);
    friend std::ostream &operator<<(std::ostream &out, const Node &n);


};
bool operator<(const Node &lhs, const Node &rhs);

/*inline std::ostream &operator<<(std::ostream &out, const Node &n)
{
    out << "Node { id:" << n.id_ << " parent:" << n.parent_id_ << " F:" << n.f() << " G:" << n.g_ << " H:" << n.h_;
    out << ", " << n.gs_ << "}";
    return out;
}*/
}

#endif
