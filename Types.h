#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

using tileindex_t = uint16_t;
using player_t = uint8_t;

struct Player
{
  static constexpr player_t ALLY = 0, ENEMY = 1;

  static constexpr player_t COUNT = 2;
};

#endif