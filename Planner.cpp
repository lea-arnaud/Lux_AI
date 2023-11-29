// The following code is based on Chris Powell GOAP project avaible here at https://github.com/cpowell/cppGOAP/tree/develop

#include "Planner.h"
#include <cassert>
#include <algorithm>
using namespace goap;


// Return true if the current GameState is in the closed list
bool goap::Planner::memberOfClosed(const PlannerWorldState &ws) const
{
    if (std::find_if(begin(closed_), end(closed_), [&](const Node &n) { return n.ws == ws; }) == end(closed_)) {
       return false;
    }
    return true;
}

// Return a pointer to the node of the open list if found
std::vector<goap::Node>::iterator goap::Planner::memberOfOpen(const PlannerWorldState &ws)
{
    return std::find_if(begin(open_), end(open_), [&](const Node &n) { return n.ws == ws; });
}

//Take the first node frome the open list and moves it to the closed one
Node &goap::Planner::popAndClose()
{
    assert(!open_.empty());
    closed_.push_back(std::move(open_.front()));
    open_.erase(open_.begin());

    return closed_.back();
}

//Move the node in the open list
void goap::Planner::addToOpenList(Node&& n)
{
    auto it = std::lower_bound(begin(open_),
                                   end(open_),
                                   n);
    open_.emplace(it, std::move(n));
}

//Calculate the distance between to given GameState
int goap::Planner::calculateHeuristic(const PlannerWorldState & now, const PlannerWorldState & goal) const
{
    return now.distanceTo(goal);
}

//Return the list of action (/!\in reverse order) found by the planner
//Throw an exception if no plan could be made with the given actions
std::vector<Action> goap::Planner::plan(const PlannerWorldState &start, const PlannerWorldState &goal, const std::vector<Action> &actions)
{

    if (start.meetsGoal(goal)) {
        return std::vector<goap::Action>();
    }

    open_.clear();
    closed_.clear();

    Node starting_node(start, 0, calculateHeuristic(start, goal), 0, nullptr);

    open_.push_back(std::move(starting_node));

  
    while (open_.size() > 0) {

        // Look for Node with the lowest-F-score on the open list. Switch it to closed,
        // and hang onto it -- this is our latest node.
        Node &current(popAndClose());

        // Is our current state the goal state? If so, we've found a path, yay.
        if (current.ws.meetsGoal(goal)) {
            std::vector<Action> the_plan;
            do {
                the_plan.push_back(*current.action);
                auto itr = std::find_if(begin(open_), end(open_), [&](const Node &n) { return n.id == current.parent_id; });
                if (itr == end(open_)) {
                    itr = std::find_if(begin(closed_), end(closed_), [&](const Node &n) { return n.id == current.parent_id; });
                }
                current = *itr;
            } while (current.parent_id != 0);
            return the_plan;
        }

        // Check each node reachable from current
        for (const auto &potential_action : actions) {
            if (potential_action.canRun(current.ws)) {
                PlannerWorldState outcome = potential_action.run(current.ws);

                // Skip if already closed
                if (memberOfClosed(outcome)) {
                    continue;
                }

                // Look for a Node with this WorldState on the open list.
                auto p_outcome_node = memberOfOpen(outcome);
                if (p_outcome_node == end(open_)) { // not a member of open list
                    // Make a new node, with current as its parent, recording G & H
                    Node found(outcome, current.g + potential_action.getCost(), calculateHeuristic(outcome, goal), current.id, &potential_action);
                    // Add it to the open list (maintaining sort-order therein)
                    addToOpenList(std::move(found));
                } else { // already a member of the open list
                    // check if the current G is better than the recorded G
                    if (current.g + potential_action.getCost() < p_outcome_node->g) {
                        p_outcome_node->parent_id = current.id;                  // make current its parent
                        p_outcome_node->g = current.g + potential_action.getCost(); // recalc G & H
                        p_outcome_node->h = calculateHeuristic(outcome, goal);
                        p_outcome_node->action = &potential_action;

                        // resort open list to account for the new F
                        // sorting likely invalidates the p_outcome_node iterator, but we don't need it anymore
                        std::sort(begin(open_), end(open_));
                    }
                }
            }
        }

    return std::vector<Action>();
    }
}
