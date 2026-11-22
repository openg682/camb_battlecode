#pragma once
// Team protocol over the 8-byte shared memory.
//
// byte 0     round stamp (round & 0xFF) written by this round's leader (the first living champion to act)
// byte 1-3   one byte per squad: bits 0-5 target camp group index + 1 (0 = none), bits 6-7 squad mode
// byte 4     bits 0-2 number of squads - 1; bits 3-7 (round / 10) & 31 when that number last changed
// byte 5     round & 0xFF when any champion last saw an enemy outside its spawn (0 = never); squad 4's byte
//            when the team runs more than three squads
// byte 6     where: cell (x / CELL) << 4 | (y / CELL); squad 5's byte when the team runs five squads
// byte 7     index + 1 of a camp group someone just saw captured or respawning
#include "common.h"
#include <cstddef>
#include <span>

namespace comms
{
inline constexpr int FARM = 0, GATHER = 1;
inline constexpr int MAX_SQUADS = 5;

// Shared-memory byte holding squad s's plan.
inline std::size_t slot(int s) { return static_cast<std::size_t>(s < 3 ? 1 + s : 2 + s); }
inline constexpr int CELL = 7;

using Mem = std::array<std::uint8_t, 8>;

inline Mem read()
{
	Mem m{};
	cambc::shared_memory.read(0, std::as_writable_bytes(std::span{ m }));
	return m;
}

inline void put(std::size_t i, std::uint8_t v)
{
	std::array<std::uint8_t, 1> b{ v };
	cambc::shared_memory.write(i, std::as_bytes(std::span{ b }));
}

inline std::uint8_t get(std::size_t i)
{
	std::array<std::uint8_t, 1> b{};
	cambc::shared_memory.read(i, std::as_writable_bytes(std::span{ b }));
	return b[0];
}

// True if no living champion has acted yet this round; claims the leader role.
inline bool claim_leader(int round)
{
	const std::uint8_t stamp = static_cast<std::uint8_t>(round & 0xFF);
	if (get(0) == stamp)
		return false;
	put(0, stamp);
	return true;
}

inline void set_squads(int k, int round)
{
	put(4, static_cast<std::uint8_t>(((k - 1) & 7) | (((round / 10) & 31) << 3)));
}

// Rounds (in tens, up to 310) since the number of squads last changed.
inline int squads_age(const Mem& m, int round) { return ((round / 10) - (m[4] >> 3)) & 31; }

inline int squads(const Mem& m) { return std::min((m[4] & 7) + 1, MAX_SQUADS); }

inline void set_squad_plan(int s, int target, int mode)
{
	const int t = (target >= 0 && target < 63) ? target + 1 : 0;
	put(slot(s), static_cast<std::uint8_t>((t & 63) | ((mode & 3) << 6)));
}

inline int squad_target(const Mem& m, int s)
{
	const int t = m[slot(s)] & 63;
	return t ? t - 1 : -1;
}

inline int squad_mode(const Mem& m, int s) { return m[slot(s)] >> 6; }

inline void report_enemy_seen(int round, vec2 pos)
{
	const std::uint8_t r = static_cast<std::uint8_t>(round & 0xFF);
	put(5, r ? r : 1);
	put(6, static_cast<std::uint8_t>((std::min(pos.x / CELL, 15) << 4) | std::min(pos.y / CELL, 15)));
}

// Rounds since any champion last saw an enemy outside its spawn (999 if never).
inline int enemy_age(const Mem& m, int round)
{
	if (!m[5])
		return 999;
	return (round - m[5]) & 0xFF;
}

inline vec2 enemy_spot(const Mem& m)
{
	return { (m[6] >> 4) * CELL + CELL / 2, (m[6] & 15) * CELL + CELL / 2 };
}

inline void report_group(int idx) { put(7, static_cast<std::uint8_t>((idx + 1) & 0xFF)); }
} // namespace comms
