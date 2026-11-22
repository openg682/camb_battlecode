#pragma once
// Champion decisions.
//
// The team farms in one to three squads. The leader (the first living champion to act each round) picks
// how many squads to run from the camp value per round each split would earn; each squad's captain (its
// living member with the highest handle) picks the squad's camp group. Rewards are split between everyone
// standing on a group's spawn tiles, so one member claims while the rest move on. Every champion banks its
// own gems.
#include "combat.h"
#include "comms.h"
#include "shop.h"
#include "world.h"
#include <map>
#include <unordered_map>

// Last-known state of an allied champion.
struct Mate
{
	vec2 pos{};
	float hp = 0, mhp = 1, dps = 60;
	int rng = 1;
	int seen = -1000;
	bool has_stats = false;
};

class Bot
{
public:
	void play_turn();

private:
	// setup
	void init(const cambc::champion& me);
	// knowledge
	void update_knowledge();
	// squads
	std::vector<const Mate*> squad_members(int k, int s) const;
	bool captain() const;
	struct SquadStats
	{
		std::vector<vec2> pts;
		float dps = 0, hp = 0, ranged = 0;
	};
	SquadStats squad_stats(const std::vector<const Mate*>& ms) const;
	void lead(bool decide);
	int choose_k(int k_cur, const comms::Mem& mem);
	void plan_squad(bool urgent_only);
	// Best group for a squad at pts and its value per round. `field` gives walking distances from the
	// squad (nullptr: straight-line estimates).
	std::pair<int, float> scan_groups(const std::vector<vec2>& pts, float dps, float hpv, int cur, int gems,
		const std::vector<int>& exclude, float ranged_dps, const cambc::distance_field* field, const vec2* avoid = nullptr, int avoid_dist = 10);
	int travel_to(int gi, const std::vector<vec2>& pts, const cambc::distance_field* field) const;
	std::pair<int, int> nearest_altar();
	bool should_bank(int gems, int d) const;
	float plan_est_ = 0, act_est_ = 0; // recent fuel cost of a squad plan and of act() (islands fuel guard)
	std::map<std::uint32_t, bool> was_alive_; // allies alive last turn (to count team deaths)
	std::vector<int> team_deaths_;			   // rounds our champions died
	int brake_until_ = -1;					   // losing fights: avoid enemy contact until this round
	void read_plan();
	// actions
	void act();
	float team_power_allies(const std::vector<cambc::champion>& units) const;
	bool engage(const std::vector<cambc::champion>& near_enemies, bool force_retreat = false);
	bool attack_best(const std::vector<cambc::champion>& enemies);
	template <class U> bool attack_unit(const U& target, const std::vector<U>& pool);
	template <class U> void try_reset(U target, const std::vector<U>& pool);
	bool cast_heal_ally(const cambc::ability& a);
	bool cast_on_camp(const cambc::camp& camp, const std::vector<cambc::camp>& live);
	bool try_submit();
	void record_state(int r);
	bool place_ward();
	bool place_lock();
	std::optional<cambc::altar_lock> enemy_lock(vec2 altar) const;
	bool break_lock(vec2 altar);
	void bank(int gems);
	bool clear_path();
	bool attack_nearby_camps();
	bool camp_fight_active() const;
	// farming
	void farm(const GroupInfo& g);
	const bitboard& approach(const GroupInfo& g);
	bitboard camp_goal(const cambc::camp& target);
	bool tank_engaged(vec2 tpos) const;
	bool group_cleared(const GroupInfo& g) const;
	std::vector<cambc::camp> near_camps(int radius) const;
	std::vector<cambc::camp> group_camps(const GroupInfo& g) const;
	bool claiming() const;
	void capture(const GroupInfo& g);
	void gather();
	// islands
	const bitboard& my_land();
	const bitboard& land_of(const GroupInfo& g);
	bool hop_toward(const GroupInfo& g);
	bool has_crystal() const;
	bool try_shortcut(int ai);
	bool waiting_for_squad(const GroupInfo& g);
	bool should_stand(const std::vector<cambc::champion>& near_enemies) const;
	int assemble_round_ = -10, assemble_group_ = -1, assemble_wait_ = 0;
	bool shortcut_map();
	int shortcut_map_ = -1; // 1 if a teleport crystal saves a long walk from our spawn to an altar (-1 = unknown)

	// state
	std::optional<World> w_;
	std::optional<Shopper> shopper_;
	Tactician tactician_;
	const cambc::champion* me_ = nullptr;
	cambc::champion_type kind_{};
	std::uint32_t handle_ = 0;
	bool melee_ = true, remote_shop_ = false, w_done_ = false;
	int r_ = 0;
	vec2 pos_{};
	const char* why_ = "?";
	std::map<const char*, int> tally_; // rounds per activity label (TALLY)
	int next_tally_ = 250;

	std::vector<std::uint32_t> handles_;		 // living-or-not allies, toughest first
	std::array<std::map<std::uint32_t, int>, 6> parts_; // parts_[k][handle] = squad when running k squads
	std::map<std::uint32_t, Mate> ally_cache_;
	std::vector<std::pair<std::uint32_t, const Mate*>> team_items_; // living allies' records
	std::vector<std::uint32_t> alive_;
	std::vector<cambc::champion> allies_, enemies_;
	std::vector<cambc::champion> ally_units_, enemy_units_; // every champion, built once (construction is costly)
	int k_window_ = -1;									  // last 20-round window this champion decided the squad count in
	bitboard camp_bb_;
	std::vector<cambc::camp> camps_; // visible camps within 6 tiles

	comms::Mem mem_{};
	int k_ = 1, squad_ = 0, target_ = -1, mode_ = comms::FARM, altar_ = -1;
	int team_enemy_age_ = 999;
	int last_b7_ = 0;
	int enemy_seen_ = -1000, seen_enemy_local_ = -1000, retreat_round_ = -100, last_fight_ = -100;
	std::map<int, std::pair<int, int>> splan_; // squad -> (round planned, target)
	std::map<int, std::pair<int, int>> sq_dl_; // squad -> (target, deadline round)
	std::vector<int> blacklist_;			   // group -> round it becomes eligible again
	std::map<int, int> altar_ban_;			   // altar -> round the ban ends
	std::unordered_map<int, bitboard> approach_cache_;
	bool banking_ = false;
	bitboard my_land_;
	bool my_land_valid_ = false;
	std::unordered_map<int, bitboard> land_cache_;
	int caution_wait_ = 0, caution_round_ = -100;
	float last_hp_ = 1e9f; // health at the end of our previous turn
	int bank_stuck_ = 0;
	int waited_ = 0, wait_round_ = -10;
	// Last turn's situation, formatted only when we die (string formatting costs a sixth of a turn).
	struct State
	{
		int r = 0, x = 0, y = 0, hp = 0, ne = 0, nd = 0, na = 0, gems = 0, mode = 0, target = 0, k = 0;
		const char* why = "?";
	};
	State last_state_;
	bool have_state_ = false;
};
