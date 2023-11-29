#ifndef GAME_RULES_H
#define GAME_RULES_H

namespace game_rules
{

static constexpr float MAX_ACT_COOLDOWN = 1.f; // units must have cooldown<1 to act
static constexpr float MAX_ROAD = 6.f;
static constexpr size_t DAY_DURATION = 30;
static constexpr size_t NIGHT_DURATION = 10;
static constexpr size_t DAYNIGHT_CYCLE_DURATION = DAY_DURATION + NIGHT_DURATION;
static constexpr size_t WORKER_CARRY_CAPACITY = 100;
// these could be stored better
static constexpr size_t FUEL_VALUE_WOOD = 1;
static constexpr size_t FUEL_VALUE_COAL = 10;
static constexpr size_t FUEL_VALUE_URANIUM = 40;
static constexpr size_t COLLECT_RATE_WOOD = 20;
static constexpr size_t COLLECT_RATE_COAL = 5;
static constexpr size_t COLLECT_RATE_URANIUM = 2;
static constexpr size_t MIN_RESEARCH_WOOD = 0;
static constexpr size_t MIN_RESEARCH_COAL = 50;
static constexpr size_t MIN_RESEARCH_URANIUM = 200;

}

#endif