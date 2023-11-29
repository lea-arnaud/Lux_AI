// The following code is based on Chris Powell GOAP project avaible here at https://github.com/cpowell/cppGOAP/tree/develop

#include <string>
#include <unordered_map>
#include <GameState.h>

#ifndef ACTION_H
#define ACTION_H
namespace goap
{

class Action
{
    std::string name_;
    int cost_;

    std::unordered_map<int, bool> preconditions;
    std::unordered_map<int, bool> effects;

public:
    Action();
    Action(std::string name, int cost);

    bool canRun(const GameState& gameState) const;
    GameState run(const GameState& gameState) const;

    void setPrecondition(const int key, const bool value)
    {
        preconditions[key] = value;
    }

    void setEffect(const int key, const bool value)
    {
        effects[key] = value;
    }
    int getCost() const { return cost_; }
};

}

#endif

