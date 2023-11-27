#include "Planner.h"
#include <cassert>
#include <algorithm>
using namespace goap;

// this planner is from https://github.com/cpowell/cppGOAP/blob/develop/goap/Planner.cpp

bool goap::Planner::memberOfClosed(const GameState &gs) const
{
    if (std::find_if(begin(closed_), end(closed_), [&](const Node &n) { return n.gs_ == gs; }) == end(closed_)) {
        return false;
    }
    return true;
}

std::vector<goap::Node>::iterator goap::Planner::memberOfOpen(const GameState &gs)
{
    return std::find_if(begin(open_), end(open_), [&](const Node &n) { return n.gs_ == gs; });
}

Node &goap::Planner::popAndClose()
{
    assert(!open_.empty());
    closed_.push_back(std::move(open_.front()));
    open_.erase(open_.begin());

    return closed_.back();
}

void goap::Planner::addToOpenList(Node&& n)
{
    auto it = std::lower_bound(begin(open_),
                                   end(open_),
                                   n);
    open_.emplace(it, std::move(n));
}

int goap::Planner::calculateHeuristic(const GameState & now, const GameState & goal) const
{
    return now.distanceTo(goal);
}

std::vector<Action> goap::Planner::plan(const GameState &start, const GameState &goal, const std::vector<Action> &actions)
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
        if (current.gs_.meetsGoal(goal)) {
            std::vector<Action> the_plan;
            do {
                the_plan.push_back(*current.action_);
                auto itr = std::find_if(begin(open_), end(open_), [&](const Node &n) { return n.id_ == current.parent_id_; });
                if (itr == end(open_)) {
                    itr = std::find_if(begin(closed_), end(closed_), [&](const Node &n) { return n.id_ == current.parent_id_; });
                }
                current = *itr;
            } while (current.parent_id_ != 0);
            return the_plan;
        }

        // Check each node REACHABLE from current -- in other words, where can we go from here?
        for (const auto &potential_action : actions) {
            if (potential_action.canRun(current.gs_)) {
                GameState outcome = potential_action.run(current.gs_);

                // Skip if already closed
                if (memberOfClosed(outcome)) {
                    continue;
                }

                // Look for a Node with this WorldState on the open list.
                auto p_outcome_node = memberOfOpen(outcome);
                if (p_outcome_node == end(open_)) { // not a member of open list
                    // Make a new node, with current as its parent, recording G & H
                    Node found(outcome, current.g_ + potential_action.getCost(), calculateHeuristic(outcome, goal), current.id_, &potential_action);
                    // Add it to the open list (maintaining sort-order therein)
                    addToOpenList(std::move(found));
                } else { // already a member of the open list
                    // check if the current G is better than the recorded G
                    if (current.g_ + potential_action.getCost() < p_outcome_node->g_) {
                        p_outcome_node->parent_id_ = current.id_;                  // make current its parent
                        p_outcome_node->g_ = current.g_ + potential_action.getCost(); // recalc G & H
                        p_outcome_node->h_ = calculateHeuristic(outcome, goal);
                        p_outcome_node->action_ = &potential_action;

                        // resort open list to account for the new F
                        // sorting likely invalidates the p_outcome_node iterator, but we don't need it anymore
                        std::sort(begin(open_), end(open_));
                    }
                }
            }
        }

    return std::vector<Action>();
}
