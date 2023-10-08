#pragma once

#include <stdint.h>

using tileindex_t = uint16_t;
using player_t = uint8_t;

struct Player
{
  static constexpr player_t ALLY = 0, ENEMY = 1;
};