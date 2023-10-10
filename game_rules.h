#ifndef GAME_RULES_H
#define GAME_RULES_H

namespace game_rules
{

static constexpr float MAX_ACT_COOLDOWN = 1.f; // units must have cooldown<1 to act
static constexpr size_t DAY_DURATION = 10;
static constexpr size_t NIGHT_DURATION = 10;
static constexpr size_t DAYNIGHT_CYCLE_DURATION = DAY_DURATION + NIGHT_DURATION;
static constexpr size_t WORKER_CARRY_CAPACITY = 100;

}

#endif