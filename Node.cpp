#include "Node.h"
#include <iostream>

int goap::Node::last_id_ = 0;
goap::Node::Node() : g_(0), h_(0)
{
    id_ = ++last_id_;
}
goap::Node::Node(const GameState state, int g, int h, int parent_id, const Action *action) :
    gs_(state), g_(g), h_(h), parent_id_(parent_id), action_(action)
{
    id_ = ++last_id_;
}

bool goap::operator<(const goap::Node &lhs, const goap::Node &rhs)
{
    return lhs.f() < rhs.f();
}