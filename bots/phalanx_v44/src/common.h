#pragma once
// Shared helpers: exception-safe unit queries, tile geometry and a small fixed-size map.
#include <cambc.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using cambc::bitboard;
using cambc::team;
using cambc::vec2;

inline int cheb(vec2 a, vec2 b) { return static_cast<int>(a.chebyshev_distance(b)); }

// Filled Chebyshev square of the given radius.
inline bitboard ring(vec2 center, int radius)
{
	bitboard b;
	b.draw_chebyshev(center, static_cast<float>(radius));
	return b;
}

inline std::uint32_t handle_of(const cambc::champion& c)
{
	return static_cast<std::uint32_t>(static_cast<cambc::handle_champion_t>(c));
}

inline std::uint32_t handle_of(const cambc::camp& c)
{
	return static_cast<std::uint32_t>(static_cast<cambc::handle_camp_t>(c));
}

// Current health, or 0 when the unit has died or left vision.
template <class U> inline float hp_of(const U& u)
{
	try
	{
		return u.health();
	}
	catch (...)
	{
		return 0.0f;
	}
}

inline bool camp_awake(const cambc::camp& c)
{
	try
	{
		return c.is_awake();
	}
	catch (...)
	{
		return false;
	}
}

inline bool attacking_camp(cambc::camp_type k)
{
	return k == cambc::camp_type::wolf || k == cambc::camp_type::elk || k == cambc::camp_type::monolith
		|| k == cambc::camp_type::golem;
}

inline float est_dps(const cambc::champion& c)
{
	try
	{
		return c.strength() * 1.1f + c.focus() * 0.25f;
	}
	catch (...)
	{
		return 60.0f;
	}
}

inline const char* champion_label(cambc::champion_type t)
{
	using T = cambc::champion_type;
	switch (t)
	{
	case T::ryth: return "RYTH";
	case T::vesper_vale: return "VESPER_VALE";
	case T::vendetta: return "VENDETTA";
	case T::cassi: return "CASSI";
	case T::issac: return "ISSAC";
	case T::stellar: return "STELLAR";
	case T::prism: return "PRISM";
	case T::marcella: return "MARCELLA";
	case T::willow: return "WILLOW";
	case T::ren: return "REN";
	case T::varg: return "VARG";
	case T::tavi: return "TAVI";
	case T::heart_of_ruin: return "HEART_OF_RUIN";
	}
	return "?";
}

// Base health by champion type: squads are dealt round-robin from the toughest down.
inline int base_hp(cambc::champion_type t)
{
	using T = cambc::champion_type;
	switch (t)
	{
	case T::ryth:
	case T::marcella: return 1200;
	case T::vesper_vale:
	case T::vendetta:
	case T::varg:
	case T::heart_of_ruin: return 1000;
	case T::ren:
	case T::issac:
	case T::tavi: return 900;
	case T::prism:
	case T::willow: return 700;
	case T::stellar: return 650;
	case T::cassi: return 600;
	}
	return 900;
}
