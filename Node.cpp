// The following code is based on Chris Powell GOAP project avaible here at https://github.com/cpowell/cppGOAP/tree/develop

#include "Node.h"
#include <iostream>

int goap::Node::last_id = 0;

goap::Node::Node() : g(0), h(0)
{
    id = ++last_id;
}
goap::Node::Node(const PlannerWorldState state, int g, int h, int parent_id, const Action *action) :
    ws(state), g(g), h(h), parent_id(parent_id), action(action)
{
    id = ++last_id;
}

bool goap::operator<(const goap::Node &lhs, const goap::Node &rhs)
{
    return lhs.f() < rhs.f();
}