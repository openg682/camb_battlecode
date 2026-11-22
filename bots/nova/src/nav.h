#pragma once
// Movement: one bounded distance-field search per call, stepping to the best reachable tile.
#include "world.h"

namespace nav
{
// Dash along the route while travelling (set per turn by the bot: the carry, with no enemy close).
inline bool travel_dash = false;
// Static blockers plus visible units, teleport reservations and tiles where a move recently failed.
bitboard blocked_now(const World& w, vec2 me_pos);
// Attempt moves in order (at most three); a failed move marks the tile as occupied for a few rounds.
bool try_moves(const cambc::champion& me, const std::vector<vec2>& candidates);
// Step towards the nearest goal tile. Returns true if the champion moved.
bool step_towards(World& w, const cambc::champion& me, const bitboard& goals, const bitboard* avoid = nullptr);
// Step to the adjacent tile that maximises distance from the nearest threat, preferring our spawn.
bool step_away(World& w, const cambc::champion& me, const std::vector<vec2>& threats);
} // namespace nav
