#pragma once
// Champion-versus-champion tactics: focus fire, kiting, casting and retreat.
//
// Basic attacks deal the attacker's strength and abilities deal modifier x focus; nothing mitigates.
// Everyone moves one tile a turn and may move and attack in either order, so a ranged champion that
// ends its turn three tiles from a melee enemy cannot be hit back by it.
#include "world.h"

struct Foe
{
	cambc::champion u;
	vec2 pos{};
	float hp = 0, dps = 0, burst = 0;
	int rng = 1;
	std::uint32_t gems = 0;
	std::uint32_t handle = 0;
	bool stunned = false, linked = false, heart = false;
	float shield = 0; // Heart of Ruin self-shield (counted only for Heart)
	float ehp() const { return hp + shield; }
	// Distance from which this enemy can hit a tile next turn (one step plus range).
	int reach() const { return rng + (stunned ? 0 : 1); }
};

// An awake attacking camp: never moves, hits the nearest champion within its range.
struct CampThreat
{
	vec2 pos{};
	float dps = 0;
	int rng = 0;
};

class Tactician
{
public:
	// Play one combat turn. `allies` are visible living allied champions (including self).
	bool turn(World& w, const cambc::champion& me, const std::vector<cambc::champion>& enemies,
		const std::vector<cambc::champion>& allies, const std::vector<cambc::camp>& camps, bool retreat, bool aggressive);
	// A hurt champion that should fight on rather than flee this turn (see Bot::should_stand).
	bool standing = false;

private:
	World* w_ = nullptr;
	const cambc::champion* me_ = nullptr;
	vec2 pos_{}, anchor_{};
	std::optional<vec2> anchor_others_; // centre of the other visible allies
	int rng_ = 1;
	float str_ = 0;
	std::vector<Foe> foes_;
	std::vector<CampThreat> camp_threats_;
	std::vector<vec2> ally_pos_; // visible allies this turn (us included)

	int find(std::uint32_t handle) const;
	void prune();
	int pick_focus() const;
	bool in_range(const Foe& f) const { return cheb(pos_, f.pos) <= rng_; }
	int killable_in_range() const;
	int best_in_range(int focus) const;
	bool basic(Foe& f);
	void attack_reset(std::uint32_t focus_handle);
	bool cast(int& focus, const std::vector<cambc::champion>& allies, bool retreat);
	bool cast_heal(const cambc::ability& a, const std::vector<cambc::champion>& allies);
	bool cast_dash(const cambc::ability& a, int focus, bool retreat);
	bool cast_blast_dash(const cambc::ability& a, int focus);
	bool move(int focus, bool attacked, bool retreat, bool aggressive);
	float threat_at(vec2 t) const;
	float melee_threat(vec2 t) const;
	bool kite_tile_exists() const;
	float burst_at(vec2 t) const;
	bool lethal(vec2 t) const;
	bool kite_ = false;
};
