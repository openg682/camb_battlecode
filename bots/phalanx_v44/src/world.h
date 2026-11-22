#pragma once
// Static map knowledge, built once per champion on its first turn of play.
#include "common.h"
#include <unordered_map>

inline constexpr float GOLD_WEIGHT = 0.3f; // value of one gold relative to one gem when choosing camps
inline constexpr float SCARCE_GEM_RATE = 2.0f; // maps supplying fewer gems per round than this (outside monoliths)...
inline constexpr float SCARCE_GEM_MULT = 20.0f; // ...value each gem this many times more
inline constexpr float GLUT_RATIO = 8.0f;	 // maps supplying more than this many gold per gem (per round)...
inline constexpr float GLUT_GOLD_SCALE = 0.1f; // ...value gold at this share of the usual weight

struct GroupInfo
{
	int idx = 0;
	std::optional<cambc::map::camp_group> group;
	std::vector<vec2> spawns;
	bitboard tiles; // where this group's camps stand (camps never move)
	vec2 center{};
	float hp = 0, dps = 0;
	int attackers = 0;
	bool heavy = false, has_monolith = false;
	int gold = 0, gems = 0, respawn = 0;
	int ally_dist = 0, enemy_dist = 0;
	float value = 0;
	int base_cost = 0;
	float danger = 0; // damage taken killing it with 1 dps (camp damage x camp health)
};

class World
{
public:
	World();

	int W = 0, H = 0;
	bitboard walls, holes, altar_bb, ally_spawn, enemy_spawn, block;
	std::vector<vec2> altars;
	vec2 ally_c{}, enemy_c{};
	std::vector<GroupInfo> groups;
	std::vector<std::int16_t> tile_group;			   // x + y * W -> group index or -1
	std::unordered_map<int, std::vector<int>> cell_groups; // 8x8 cell of a group's centre -> groups
	// Passable tiles within tribute range of altar i (built on first use).
	const bitboard& altar_ring(int i);
	// True while every tribute tile of altar i is held by a camp group believed alive (some maps guard their
	// only altar with monoliths: it opens once a guard dies).
	bool altar_guarded(int i, int r);
	// Groups standing on every tribute tile of some altar: killing one opens banking there (filled lazily).
	std::vector<char> guard_group;
	void find_guards();

	// Tiles reachable on foot from our spawn, and whether any camp group lies off them (island maps: those
	// groups are reached by dashing or teleporting across short gaps). Filled with the spawn field.
	bitboard home_land;
	bool has_islands = false;
	// Lazily filled path-distance fields (spawn, then one per altar).
	std::optional<cambc::distance_field> spawn_field;
	std::vector<std::optional<cambc::distance_field>> altar_field;
	// Row-major copy of altar_field[i] (one engine call; reading it costs no further fuel). Empty until the field exists.
	const std::vector<std::uint16_t>& altar_linear(int i);
	std::vector<int> group_altar_min; // nearest altar's walking distance from each group
	bool altar_table_done = false;

	// Knowledge of group states: round the group is next expected alive (0 = alive now).
	std::vector<int> respawn_at;
	std::vector<char> reward_ready;

	// Fill missing fields while more than `reserve` fuel remains. Returns true once everything is ready.
	bool precompute(std::uint64_t reserve);
	int home_dist(vec2 p) const;
	int altar_dist(int i, vec2 p) const;
	int group_of_tile(vec2 p) const { return tile_group[p.x + p.y * W]; }
	// Spawn tiles of every group not known to be dead (routes plan around them consistently).
	const bitboard& camp_wall(int r);
	int wall_version() const { return wall_version_; }
	// Rough fuel cost of one full-map distance field.
	std::uint64_t field_cost() const { return static_cast<std::uint64_t>(W) * H * 7 / 2 + 2000; }
	// Groups whose centre lies in the 3x3 block of 8x8 cells around p.
	void groups_near(vec2 p, std::vector<int>& out) const;

private:
	bitboard wall_;
	std::vector<char> wall_key_;
	bool wall_valid_ = false;
	std::vector<std::optional<bitboard>> altar_ring_;
	struct Guard
	{
		bool done = false, open = false;
		std::vector<int> groups;
	};
	std::vector<Guard> guard_;
	std::vector<std::vector<std::uint16_t>> altar_lin_;
	int wall_version_ = 0;
};

inline int cell_key(int cx, int cy) { return cx * 4096 + cy; }
