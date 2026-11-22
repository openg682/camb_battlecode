#include "bot.h"
#include "nav.h"
#include <cmath>

namespace
{
constexpr int BANK_THRESHOLD = 40;		  // carried gems that send a champion to bank when enemies are about
constexpr int BANK_SOLO = 70;			  // always bank above this
constexpr int BANK_NEAR = 6;			  // bank any gems when an altar is this many steps away
constexpr float LOW_HP = 0.35f;
constexpr float STAND_RATIO = 0.0f;		  // a hurt champion fights on while our local power is this share of theirs (0 = always flee)			  // fraction of max health that triggers a retreat
constexpr float FIGHT_RATIO = 0.8f;		  // below this fraction of their power we retreat
constexpr float ENGAGE_RATIO = 1.05f;	  // above it we press the attack
constexpr std::uint32_t SHOP_GOLD = 300;  // recall to shop when carrying this much gold and nothing else to do
constexpr std::uint32_t SHOP_WALK_GOLD = 150; // walk home to shop with this much gold...
constexpr int SHOP_WALK_DIST = 8;			  // ...when spawn is this few steps away (0 gold = off)
constexpr int ASSEMBLE_WAIT = 0;		  // rounds to wait for squad mates before waking camps too dangerous for those present (0 = off)
constexpr bool CAMP_CAUTION = true;		  // do not wake camps with enemies about (for up to 15 rounds)
constexpr bool DEATH_LOG = true;		  // one line per death (visible in downloaded ladder replays)
constexpr std::uint32_t ENEMY_RACE_GEMS = 850; // bank everything once the enemy has submitted this many
constexpr float DANGER_CAP = 0.7f;		  // skip camps whose damage over the kill would exceed this share of health
constexpr bool WARDS = true;
constexpr bool NO_CHASE = true;
constexpr int CHASE_SLACK = 2;			  // engage enemies within (attack range + this) tiles
constexpr bool CHASE_WOUNDED = true;		  // also engage enemies under 30% health within 6 tiles
constexpr bool HELP_ALLIES = true;		  // also engage enemies within HELP_REACH of a nearby teammate (they poke from range)
constexpr int HELP_REACH = 5;
// Squads
constexpr bool RETREAT_FIX = true;		  // retreat only from camps that can reach us; walk home when stuck; abandon a recall under fire
constexpr int MAX_K = 2;				  // the team farms in at most this many squads
constexpr bool ANY_CLAIMER = false;		  // a target is done once any ally (not only a squad member) claims its reward
constexpr int FIXED_K = 0;				  // always run this many squads (capped by MAX_K and team size; 0 = decide)
constexpr float SPLIT_GAIN = 1.3f;		  // add a squad when the squads' combined value rate beats the plan's by this
constexpr float MERGE_GAIN = 1.1f;		  // drop one when fewer squads beat the current plan by this factor
constexpr int K_EVERY = 20;				  // squad-count decisions on rounds divisible by this
constexpr int K_DWELL = 50;				  // keep a squad count at least this many rounds
constexpr int REGROUP_AGE = 0;			  // one squad while an enemy was seen this recently (0 = never)
constexpr int REGROUP_NEAR = 0;			  // merge into one squad at once when an enemy is seen within this many tiles of us (0 = off)
constexpr int AVOID_AGE = 0;			  // split squads avoid groups near an enemy seen this recently (0 = off)...
constexpr int AVOID_DIST = 10;			  // ...within this many tiles
// Groups nearer the enemy spawn than ours are worth this share (top teams fight us on their side: set H deaths at
// 0.6-0.8 of the way to their spawn on 00galaxy, us at 0.8-0.9 forward on 04wheel at r100; 1 = off).
constexpr float FAR_SIDE_W = 0.7f;
constexpr int FAR_SIDE_MIN_SPAWN = 40; // only on big maps (spawns >= this far apart); small maps lost ~8/12 ungated
constexpr float GUARD_DANGER = 1.0f;	  // damage allowance multiplier for camps guarding an altar's every tribute tile
constexpr float GUARD_BONUS = 0.0f;  	  // extra value for killing such a guard (it unlocks banking)
constexpr float BANK_W = 1.0f;			  // rounds charged per gem for carrying it to an altar: walking distance x this / BANK_LOAD
constexpr float BANK_LOAD = 70.0f;		  // ...the load one banking trip carries
constexpr int HOP_COST = 45;
constexpr float HURT_ROUNDS = 60.0f;		  // rounds charged for taking a whole squad's health in camp damage (the trip home to heal)
constexpr float EARLY_GOLD_W = 0.0f;		  // extra value per gold before EARLY_GOLD_UNTIL: early items snowball (top teams farm gold first)
constexpr int EARLY_GOLD_UNTIL = 300;
constexpr int SHORTCUT_GAIN = 0;		  // flash with a teleport crystal towards an altar when it saves this many steps (0 = off)			  // extra rounds counted for reaching a group off our land (walk to shore, hop)
constexpr int ROAM_WARD = 9;			  // keep a ward within this many tiles of us, on our forward side (0 = altar wards only)
constexpr std::uint64_t PRECOMPUTE_RESERVE = 12'000; // fuel kept back after the end-of-turn precompute
constexpr bool TALLY = false;			  // print each champion's rounds per activity every 250 rounds
constexpr int TRACE_KIND = -1;			  // print one line per round for this champion type (-1 = off)...
constexpr int TRACE_FROM = 0;			  // ...from this round...
constexpr int TRACE_TO = 0;				  // ...to this one
constexpr bool PROFILE = false;			  // print per-phase fuel for expensive turns

bool contains(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }
} // namespace

// ------------------------------------------------------------------ setup
void Bot::init(const cambc::champion& me)
{
	w_.emplace();
	blacklist_.assign(w_->groups.size(), 0);
	kind_ = me.champion_name();
	shopper_.emplace(kind_);
	melee_ = me.attack_range() <= 1;
	handle_ = handle_of(me);
	remote_shop_ = me.passive() == cambc::passive_type::remote_shop;
	std::vector<std::pair<int, std::uint32_t>> tough;
	for (const auto& a : cambc::champions(team::ally))
	{
		int hpv = 900;
		try
		{
			hpv = base_hp(a.champion_name());
		}
		catch (...)
		{
		}
		tough.emplace_back(-hpv, handle_of(a));
	}
	std::sort(tough.begin(), tough.end());
	ally_units_ = cambc::champions(team::ally);
	enemy_units_ = cambc::champions(team::enemy);
	for (const auto& t : tough)
		handles_.push_back(t.second);
	// parts_[k][handle] = squad index when the team runs k squads. Nested: going from two squads to three
	// splits squad 0 (its toughest member keeps it), so nobody else changes squad.
	std::vector<std::uint32_t> a0;
	for (std::size_t i = 0; i < handles_.size(); ++i)
	{
		const auto h = handles_[i];
		parts_[1][h] = 0;
		parts_[2][h] = static_cast<int>(i % 2);
		if (i % 2 == 0)
			a0.push_back(h);
	}
	for (const auto h : handles_)
	{
		if (parts_[2][h] == 1)
			parts_[3][h] = 1;
		else
		{
			const auto idx = std::find(a0.begin(), a0.end(), h) - a0.begin();
			parts_[3][h] = idx % 2 == 0 ? 0 : 2;
		}
	}
	// Four and five squads: split squad 1, then squad 0 (the second member of each leaves).
	for (int k = 4; k <= 5; ++k)
	{
		const int split = k == 4 ? 1 : 0;
		bool first = true;
		for (const auto h : handles_)
		{
			int sq = parts_[k - 1][h];
			if (sq == split)
			{
				if (!first)
					sq = k - 1;
				first = false;
			}
			parts_[k][h] = sq;
		}
	}
}

// ------------------------------------------------------------------ turn
void Bot::play_turn()
{
	const cambc::champion& me = cambc::self();
	if (!w_)
		init(me);
	r_ = static_cast<int>(cambc::round_number());
	if (!me.is_alive())
	{
		if (have_state_)
		{
			// One compact line per death: these show up in ladder replays (team stdout) for analysis.
			const State& st = last_state_;
			std::cout << "DIED r" << st.r << " " << champion_label(kind_) << " " << st.why << " at " << st.x << "," << st.y
					  << " hp " << st.hp << " enemies " << st.ne << " near " << st.nd << " allies6 " << st.na << " gems "
					  << st.gems << " mode " << st.mode << " tgt " << st.target << " k " << st.k << std::endl;
			have_state_ = false;
		}
		shopper_->shop(me);
		if (!w_done_)
			w_done_ = w_->precompute(PRECOMPUTE_RESERVE);
		return;
	}
	me_ = &me;
	const std::uint64_t f0 = cambc::fuel_remaining();
	pos_ = me.position();
	const bool leader = comms::claim_leader(r_);
	allies_.clear();
	alive_.clear();
	for (const auto& a : ally_units_)
	{
		if (!a.is_alive())
			continue;
		const auto h = handle_of(a);
		alive_.push_back(h);
		if (!a.is_visible())
			continue;
		allies_.push_back(a);
		Mate& rec = ally_cache_[h];
		try
		{
			rec.pos = a.position();
			rec.hp = a.health();
			if (r_ % 10 == 0 || !rec.has_stats)
			{
				rec.mhp = a.max_health();
				rec.dps = est_dps(a);
				rec.rng = a.attack_range();
				rec.has_stats = true;
			}
			rec.seen = r_;
		}
		catch (...)
		{
		}
	}
	enemies_.clear();
	for (const auto& e : enemy_units_)
	{
		try
		{
			if (e.is_visible() && e.is_alive())
				enemies_.push_back(e);
		}
		catch (...)
		{
		}
	}
	// Camps: one bitboard of every visible camp, objects only for those near us.
	camp_bb_ = cambc::vision::camp_tiles();
	camps_ = near_camps(6);
	for (const auto& e : enemies_)
	{
		const vec2 ep = e.position();
		if (!w_->enemy_spawn.is_set(ep))
		{
			seen_enemy_local_ = r_;
			if (MAX_K <= 3) // bytes 5-6 hold squad plans beyond three squads
				comms::report_enemy_seen(r_, ep);
			break;
		}
	}
	// Last-known records of every living ally (allies in bushes are invisible).
	team_items_.clear();
	for (const auto& [h, rec] : ally_cache_)
		if (std::find(alive_.begin(), alive_.end(), h) != alive_.end())
			team_items_.emplace_back(h, &rec);
	const std::uint64_t f1 = cambc::fuel_remaining();
	update_knowledge();
	const std::uint64_t f2 = cambc::fuel_remaining();
	if (leader)
		lead(false);
	const std::uint64_t f3 = cambc::fuel_remaining();
	read_plan();
	const bool cap = captain();
	if (cap)
		plan_squad(true); // only when the squad has no valid target; routine re-plans wait for leftover fuel
	const std::uint64_t f4 = cambc::fuel_remaining();
	const float hp_now = me.health();
	if (RETREAT_FIX && me.is_recalling() && hp_now < last_hp_ && hp_now < me.max_health() * 0.3f)
	{
		// Something is hitting us mid-channel: the recall would end in death. Walk out instead.
		try
		{
			me.cancel_recall();
		}
		catch (...)
		{
		}
		why_ = "recall-cancel";
		nav::step_towards(*w_, me, w_->ally_spawn);
	}
	else if (me.is_recalling() || me.is_submitting_gems() || me.is_teleporting())
		why_ = me.is_submitting_gems() ? "tribute" : me.is_recalling() ? "recall" : "teleport";
	else if (cambc::fuel_remaining() < 45'000 && r_ <= 2)
		why_ = "setup"; // the first turn's map setup used the fuel: start moving next turn
	else
	{
		why_ = "?";
		act();
	}
	const std::uint64_t f5 = cambc::fuel_remaining();
	if (TRACE_KIND >= 0 && static_cast<int>(kind_) == TRACE_KIND && r_ >= TRACE_FROM && r_ <= TRACE_TO)
		std::cout << "T r" << r_ << " " << pos_.x << "," << pos_.y << " " << why_ << " tgt " << target_ << " k" << k_ << " s" << squad_
				  << " gems " << me.gem_count() << " gold " << me.gold_count() << " hp " << static_cast<int>(me.health())
				  << " xtal " << has_crystal() << std::endl;
	if (TALLY)
	{
		++tally_[why_];
		// Printing costs fuel: wait for a turn with plenty left.
		if (r_ >= next_tally_ && cambc::fuel_remaining() > 60'000)
		{
			next_tally_ = (r_ / 250 + 1) * 250;
			std::cout << "TALLY r" << r_ << " " << champion_label(kind_) << " k" << k_;
			for (const auto& [label, n] : tally_)
				std::cout << " " << label << "=" << n;
			std::cout << std::endl;
		}
	}
	if (DEATH_LOG)
		record_state(r_);
	last_hp_ = hp_now;
	const std::uint64_t f6 = cambc::fuel_remaining();
	// Planning and precomputation run on whatever fuel this turn has left: teammates acting later this round
	// still see the new plan, and a turn that ran dry would cost us the next turn too.
	if (cap)
		plan_squad(false);
	if (leader)
		lead(true);
	if (!w_done_)
		w_done_ = w_->precompute(PRECOMPUTE_RESERVE);
	if (PROFILE)
	{
		const std::uint64_t f7 = cambc::fuel_remaining();
		if (f0 > f7 && f0 - f7 > 60'000)
			std::cout << "FUEL r" << r_ << " " << champion_label(kind_) << " total=" << (f0 - f7) << " scan=" << (f0 - f1)
					  << " know=" << (f1 - f2) << " lead=" << (f2 - f3) << " plan=" << (f3 - f4) << " act=" << (f4 - f5)
					  << " rec=" << (f5 - f6) << " pre=" << (f6 - f7) << " why=" << why_ << " k=" << k_ << std::endl;
	}
}

// ------------------------------------------------------------------ knowledge
void Bot::update_knowledge()
{
	World& w = *w_;
	std::vector<int> near;
	w.groups_near(pos_, near);
	// A group believed dead whose camps are in sight again has respawned.
	for (int gi : near)
		if (w.respawn_at[gi] && !(w.groups[gi].tiles & camp_bb_).is_empty())
		{
			w.respawn_at[gi] = 0;
			w.reward_ready[gi] = 0;
		}
	// Poll the states of groups near us, skipping groups known to respawn later. Each query costs as much as
	// a short scan, so at most a few per turn, our target first.
	if (target_ >= 0)
	{
		auto it = std::find(near.begin(), near.end(), target_);
		if (it != near.end())
			std::iter_swap(near.begin(), it);
	}
	int polls = 0;
	std::optional<bitboard> vis;
	for (int gi : near)
	{
		if (polls >= 4)
			break;
		const GroupInfo& g = w.groups[gi];
		if (w.respawn_at[gi] > r_ + 2 || cheb(g.center, pos_) > 8)
			continue;
		if (!vis)
			vis = cambc::vision::visible_tiles();
		if ((g.tiles & *vis).is_empty())
			continue;
		// Camps standing on its tiles: alive, no need to ask (each state query costs as much as a short scan).
		if (!(g.tiles & camp_bb_).is_empty())
		{
			w.respawn_at[gi] = 0;
			w.reward_ready[gi] = 0;
			continue;
		}
		++polls;
		try
		{
			const auto st = g.group->state();
			if (const auto* resp = std::get_if<cambc::map::camp_group_respawning>(&st))
			{
				if (w.respawn_at[gi] <= r_)
					comms::report_group(gi);
				w.respawn_at[gi] = r_ + resp->timer;
				w.reward_ready[gi] = 0;
			}
			else if (std::holds_alternative<cambc::map::camp_group_reward_available>(st))
			{
				w.reward_ready[gi] = 1;
				w.respawn_at[gi] = 0;
			}
			else
			{
				w.reward_ready[gi] = 0;
				w.respawn_at[gi] = 0;
			}
		}
		catch (...)
		{
		}
	}
	const auto mem = comms::read();
	if (mem[7] != last_b7_)
	{
		last_b7_ = mem[7];
		const int gi = mem[7] - 1;
		if (gi >= 0 && gi < static_cast<int>(w.groups.size()) && w.respawn_at[gi] <= r_)
		{
			w.respawn_at[gi] = r_ + w.groups[gi].respawn;
			w.reward_ready[gi] = 0;
		}
	}
}

// ------------------------------------------------------------------ squads
std::vector<const Mate*> Bot::squad_members(int k, int s) const
{
	std::vector<const Mate*> out;
	const auto& part = parts_[k];
	for (const auto& [h, m] : team_items_)
	{
		auto it = part.find(h);
		if ((it == part.end() ? 0 : it->second) == s)
			out.push_back(m);
	}
	return out;
}

// True if we are the living member of our squad with the highest handle (the leader, usually the lowest
// handle, already has the most planning to do).
bool Bot::captain() const
{
	const auto& part = parts_[k_];
	std::uint32_t best = 0;
	bool found = false;
	for (const auto h : alive_)
	{
		auto it = part.find(h);
		if ((it == part.end() ? 0 : it->second) != squad_)
			continue;
		if (!found || h > best)
		{
			best = h;
			found = true;
		}
	}
	return found && best == handle_;
}

Bot::SquadStats Bot::squad_stats(const std::vector<const Mate*>& ms) const
{
	SquadStats st;
	long sx = 0, sy = 0;
	for (const auto* m : ms)
	{
		st.pts.push_back(m->pos);
		sx += m->pos.x;
		sy += m->pos.y;
		st.dps += m->dps;
		st.hp += m->hp;
		if (m->rng > 1)
			st.ranged += m->dps;
	}
	if (st.pts.size() > 3)
	{
		const int cx = static_cast<int>(sx / static_cast<long>(ms.size()));
		const int cy = static_cast<int>(sy / static_cast<long>(ms.size()));
		const vec2 c{ cx, cy };
		vec2 mid = st.pts.front();
		for (auto p : st.pts)
			if (cheb(p, c) < cheb(mid, c))
				mid = p;
		st.pts = { mid };
	}
	return st;
}

void Bot::lead(bool decide)
{
	const auto mem = comms::read();
	if (!enemies_.empty())
		enemy_seen_ = r_;
	if (!decide)
		return;
	const int k_cur = comms::squads(mem);
	int k = k_cur;
	if (REGROUP_NEAR && MAX_K <= 3 && k_cur > 1 && comms::enemy_age(mem, r_) <= 3)
	{
		// An enemy is about: fight as one team rather than as squads it can pick off.
		const vec2 spot = comms::enemy_spot(mem);
		for (const auto& [h, m] : team_items_)
			if (m->seen >= r_ - 5 && cheb(m->pos, spot) <= REGROUP_NEAR)
			{
				comms::set_squads(1, r_);
				for (int s = 0; s < std::max(MAX_K, 3); ++s)
					comms::set_squad_plan(s, -1, comms::GATHER);
				splan_.clear();
				return;
			}
	}
	// Once per 20-round window, when this turn has fuel to spare.
	if (FIXED_K > 0)
		k = choose_k(k_cur, mem);
	else if (r_ / K_EVERY != k_window_ && comms::squads_age(mem, r_) * 10 >= K_DWELL && cambc::fuel_remaining() > 70'000)
	{
		k_window_ = r_ / K_EVERY;
		k = choose_k(k_cur, mem);
	}
	if (k != k_cur)
	{
		comms::set_squads(k, r_);
		for (int s = 0; s < std::max(MAX_K, 3); ++s)
			comms::set_squad_plan(s, -1, comms::GATHER);
		splan_.clear();
	}
}

int Bot::choose_k(int k_cur, const comms::Mem& mem)
{
	if (FIXED_K > 0) // whole team, dead or alive: deaths must not reshuffle the squads
		return std::max(1, std::min({ FIXED_K, MAX_K, static_cast<int>(handles_.size()) }));
	const int n = static_cast<int>(team_items_.size());
	if (n <= 1)
		return 1;
	if (REGROUP_AGE && comms::enemy_age(mem, r_) <= REGROUP_AGE)
		return 1;
	// Compare the current squad count with one alternative, alternately one more and one fewer.
	const int top = std::min(MAX_K, n);
	int alt = (r_ / K_EVERY) % 2 == 0 ? k_cur + 1 : k_cur - 1;
	if (alt < 1 || alt > top)
	{
		alt = 2 * k_cur - alt;
		if (alt < 1 || alt > top)
			return k_cur;
	}
	float rate_cur = 0, rate_alt = 0;
	for (int pass = 0; pass < 2; ++pass)
	{
		const int k = pass == 0 ? k_cur : alt;
		std::vector<int> taken;
		float total = 0;
		for (int s = 0; s < k; ++s)
		{
			const auto ms = squad_members(k, s);
			if (ms.empty())
				continue;
			const auto st = squad_stats(ms);
			const auto [t, sc] = scan_groups({ st.pts.front() }, st.dps, st.hp, -1, 0, taken, st.ranged, nullptr);
			if (t >= 0)
			{
				taken.push_back(t);
				total += sc;
			}
		}
		(pass == 0 ? rate_cur : rate_alt) = total;
	}
	const float need = alt > k_cur ? SPLIT_GAIN : MERGE_GAIN;
	return rate_alt > rate_cur * need ? alt : k_cur;
}

// Captain: pick our squad's camp group. A target whose reward is waiting with one of us on its spawn tiles
// is done (that member stays to claim it); a target that has not paid out well past its estimated time is
// dropped for a while.
void Bot::plan_squad(bool urgent_only)
{
	World& w = *w_;
	const int s = squad_;
	int cur = comms::squad_target(mem_, s);
	const auto ms = squad_members(k_, s);
	if (ms.empty())
		return;
	std::vector<int> taken;
	for (int o = 0; o < k_; ++o)
		if (o != s)
		{
			const int t = comms::squad_target(mem_, o);
			if (t >= 0)
				taken.push_back(t);
		}
	if (cur >= 0)
	{
		const GroupInfo& g = w.groups[cur];
		bool someone_on = false;
		for (const auto& [h, m] : team_items_)
			if ((ANY_CLAIMER || std::find(ms.begin(), ms.end(), m) != ms.end()) && g.tiles.is_set(m->pos))
				someone_on = true;
		if (w.reward_ready[cur] && someone_on)
		{
			taken.push_back(cur);
			cur = -1;
		}
		else if (contains(taken, cur))
			cur = -1;
	}
	if (cur >= 0)
	{
		auto it = sq_dl_.find(s);
		if (it != sq_dl_.end() && it->second.first == cur && r_ > it->second.second && w.respawn_at[cur] <= r_)
		{
			blacklist_[cur] = r_ + 150;
			cur = -1;
		}
	}
	auto cache = splan_.find(s);
	if (cache != splan_.end() && cache->second.first == r_)
		return; // already planned this round
	const bool valid = cur >= 0 && w.respawn_at[cur] <= r_ + 4;
	if (valid && urgent_only)
		return;
	if (valid && cache != splan_.end() && cache->second.second == cur && r_ - cache->second.first < 6)
		return;
	if (cambc::fuel_remaining() < w.field_cost() + (urgent_only ? 45'000 : 6'000))
		return; // re-plan on a turn with more fuel to spare
	std::optional<vec2> avoid;
	if (AVOID_AGE && MAX_K <= 3 && k_ > 1 && comms::enemy_age(mem_, r_) <= AVOID_AGE)
		avoid = comms::enemy_spot(mem_);
	const auto st = squad_stats(ms);
	// One distance field from the squad's members gives walking distances to every group.
	cambc::distance_field field;
	bitboard roots, visited;
	for (auto p : st.pts)
		roots.set(p);
	field.compute(w.block & ~roots, roots, visited);
	const auto [t, sc] = scan_groups(st.pts, st.dps, st.hp, cur, static_cast<int>(me_->gem_count()), taken, st.ranged, &field,
		avoid ? &*avoid : nullptr);
	(void)sc;
	splan_[s] = { r_, t };
	if (t >= 0)
	{
		auto it = sq_dl_.find(s);
		if (it == sq_dl_.end() || it->second.first != t)
		{
			const GroupInfo& g = w.groups[t];
			const int travel = travel_to(t, st.pts, &field);
			sq_dl_[s] = { t, r_ + 2 * travel + static_cast<int>(2 * g.hp / std::max(st.dps, 1.0f)) + 40 };
		}
	}
	comms::set_squad_plan(s, t, t >= 0 ? comms::FARM : comms::GATHER);
	target_ = t;
	mode_ = t >= 0 ? comms::FARM : comms::GATHER;
}

int Bot::travel_to(int gi, const std::vector<vec2>& pts, const cambc::distance_field* field) const
{
	const GroupInfo& g = w_->groups[gi];
	if (field != nullptr)
	{
		int best = 65535;
		for (auto t : g.spawns)
			best = std::min(best, static_cast<int>(field->get(t)));
		if (best < 65535)
			return best;
		if (!w_->has_islands)
			return 9999;
		// Off our land: reachable by hopping (dash or teleport crystal); allow for the detour and the hop.
		int dmin = 1 << 20;
		for (auto p : pts)
			dmin = std::min(dmin, cheb(g.center, p));
		return dmin * 3 / 2 + HOP_COST;
	}
	int best = 1 << 20;
	for (auto p : pts)
		best = std::min(best, cheb(g.center, p));
	return best * 13 / 10;
}

std::pair<int, float> Bot::scan_groups(const std::vector<vec2>& pts, float dps, float hpv, int cur, int gems,
	const std::vector<int>& exclude, float ranged_dps, const cambc::distance_field* field, const vec2* avoid)
{
	const float early_gold = r_ < EARLY_GOLD_UNTIL ? EARLY_GOLD_W : 0.0f;
	World& w = *w_;
	int best = -1;
	float best_score = 0;
	const float carry_w = std::min(gems / 150.0f, 1.5f) * 0.5f;
	const float inv_dps = 1.0f / std::max(dps, 1.0f);
	const float hp_cap = hpv * DANGER_CAP;
	const vec2 p0 = pts.front();
	// Walking distance from any member is at least the straight-line distance from the first member minus
	// the squad's spread around it (and the 3 tiles between a group's centre and its edge).
	int slack = 3;
	for (auto p : pts)
		slack = std::max(slack, cheb(p, p0) + 3);
	if (w.guard_group.empty())
		w.find_guards();
	std::vector<std::pair<float, int>> order;
	order.reserve(w.groups.size());
	for (const GroupInfo& g : w.groups)
	{
		const int gi = g.idx;
		if (blacklist_[gi] > r_ || contains(exclude, gi))
			continue;
		// A guard of our only way to bank is worth fighting harder for (see below).
		const bool guard = w.guard_group[gi] && w.respawn_at[gi] <= r_;
		// Too dangerous for this squad (camps focus one champion at a time): skip before the bound search.
		if (!g.heavy && !w.reward_ready[gi] && g.danger * inv_dps > hp_cap * (guard ? GUARD_DANGER : 1.0f))
			continue;
		if (avoid != nullptr && gi != cur && cheb(g.center, *avoid) <= AVOID_DIST)
			continue;
		const int d = cheb(g.center, p0) - slack;
		const float side = cheb(w.ally_c, w.enemy_c) >= FAR_SIDE_MIN_SPAWN && cheb(g.center, w.enemy_c) < cheb(g.center, w.ally_c) ? FAR_SIDE_W : 1.0f;
		float ub = side * (g.value + early_gold * g.gold) / static_cast<float>((d > 0 ? d : 0) + g.base_cost);
		if (gi == cur)
			ub *= 1.5f;
		order.emplace_back(ub, gi);
	}
	// Examine groups best-bound first (a heap: the search usually stops after a handful).
	auto less_ub = [](const auto& a, const auto& b) { return a.first < b.first; };
	std::make_heap(order.begin(), order.end(), less_ub);
	while (!order.empty())
	{
		std::pop_heap(order.begin(), order.end(), less_ub);
		const auto [ub, gi] = order.back();
		order.pop_back();
		if (ub * 1.002f <= best_score)
			break;
		const GroupInfo& g = w.groups[gi];
		const int travel = travel_to(gi, pts, field);
		if (travel >= 9999)
			continue;
		const int ready_in = w.respawn_at[gi] - r_;
		if (ready_in > travel + 4)
			continue;
		float kill = w.reward_ready[gi] ? 0.0f : g.hp * inv_dps;
		float gdps = g.dps;
		if (g.heavy)
		{
			kill = w.reward_ready[gi] ? 0.0f : g.hp / std::max(ranged_dps, 1.0f) + 10;
			gdps = g.dps * 2.0f / g.attackers;
		}
		// Camps focus one champion at a time; skip groups whose damage would overwhelm the squad. The guard of
		// an altar whose every tribute tile it holds unlocks banking: accept more damage and count the unlock.
		const bool guard = w.guard_group[gi] && w.respawn_at[gi] <= r_;
		if (gdps * kill > hp_cap * (guard ? GUARD_DANGER : 1.0f))
			continue;
		const float cost = static_cast<float>(std::max(travel, ready_in)) + kill + g.base_cost
			+ (carry_w + BANK_W * g.gems / BANK_LOAD) * w.group_altar_min[gi]
			+ HURT_ROUNDS * gdps * kill / std::max(hpv, 1.0f);
		const float side_s = cheb(w.ally_c, w.enemy_c) >= FAR_SIDE_MIN_SPAWN && cheb(g.center, w.enemy_c) < cheb(g.center, w.ally_c) ? FAR_SIDE_W : 1.0f;
		float score = side_s * (g.value + early_gold * g.gold + (guard ? GUARD_BONUS : 0.0f)) / cost;
		if (gi == cur)
			score *= 1.5f;
		if (score > best_score)
		{
			best = gi;
			best_score = score;
		}
	}
	return { best, best_score };
}

std::pair<int, int> Bot::nearest_altar()
{
	World& w = *w_;
	int best = -1, best_d = 0;
	for (std::size_t i = 0; i < w.altars.size(); ++i)
	{
		auto ban = altar_ban_.find(static_cast<int>(i));
		if ((ban != altar_ban_.end() && ban->second >= r_) || cheb(w.altars[i], w.enemy_c) <= 7)
			continue;
		if (w.altar_guarded(static_cast<int>(i), r_))
			continue; // carry the gems until a guard falls
		const int d = w.altar_dist(static_cast<int>(i), pos_);
		if (d >= 9999)
			continue;
		if (best < 0 || d < best_d)
		{
			best = static_cast<int>(i);
			best_d = d;
		}
	}
	return { best, best < 0 ? 9999 : best_d };
}

// Take our own gems to an altar `d` steps away now?
bool Bot::should_bank(int gems, int d) const
{
	if (banking_)
		return true; // finish the trip
	// Carried gems count for nothing at the round limit or once the enemy reaches the target.
	if (r_ + d + 15 >= static_cast<int>(cambc::constants::max_rounds))
		return true;
	if (cambc::global_submitted_gems(team::enemy) >= ENEMY_RACE_GEMS)
		return true;
	if (cambc::global_submitted_gems(team::ally) + static_cast<std::uint32_t>(gems) >= cambc::constants::winning_gems)
		return true;
	if (gems >= BANK_SOLO || target_ < 0)
		return true;
	if (r_ - seen_enemy_local_ <= 30 && gems >= BANK_THRESHOLD)
		return true;
	return d <= BANK_NEAR;
}

void Bot::read_plan()
{
	mem_ = comms::read();
	k_ = comms::squads(mem_);
	auto it = parts_[k_].find(handle_);
	squad_ = it == parts_[k_].end() ? 0 : it->second;
	target_ = comms::squad_target(mem_, squad_);
	mode_ = comms::squad_mode(mem_, squad_);
	team_enemy_age_ = comms::enemy_age(mem_, r_);
}

// ------------------------------------------------------------------ actions
void Bot::act()
{
	const cambc::champion& me = *me_;
	World& w = *w_;
	const vec2 pos = pos_;
	const float myhp = me.health(), mhp = me.max_health();
	const bool at_spawn = w.ally_spawn.is_set(pos);
	std::vector<cambc::champion> near_enemies;
	for (const auto& e : enemies_)
	{
		const vec2 ep = e.position();
		if (cheb(ep, pos) > 8)
			continue;
		// Enemies resting in their own spawn heal fully every round and cannot be chased.
		if (w.enemy_spawn.is_set(ep))
			continue;
		near_enemies.push_back(e);
	}

	if (at_spawn || (remote_shop_ && me.gold_count() >= 30))
	{
		if ((w.has_islands || shortcut_map()) && !has_crystal())
		{
			try
			{
				const auto tp = cambc::consumable_type::tp_crystal;
				if (me.gold_count() >= tp.cost())
					me.buy(tp);
			}
			catch (...)
			{
			}
		}
		shopper_->shop(me);
		if (myhp < mhp * 0.95f && near_enemies.empty())
		{
			why_ = "heal";
			return; // spawn heals fully next round
		}
	}

	// Retreat when badly hurt, unless the fight at hand is still ours to win (fleeing at equal speed only
	// gives the chaser free hits).
	tactician_.standing = false;
	if (myhp < mhp * LOW_HP && !at_spawn && should_stand(near_enemies))
	{
		tactician_.standing = true;
		why_ = "stand";
		engage(near_enemies, false);
		tactician_.standing = false;
		return;
	}
	if (myhp < mhp * LOW_HP && !at_spawn)
	{
		std::vector<vec2> threats;
		for (const auto& e : near_enemies)
			if (cheb(e.position(), pos) <= e.attack_range() + 3)
				threats.push_back(e.position());
		for (const auto& c : camps_)
			if (attacking_camp(c.kind()) && camp_awake(c) && hp_of(c) > 0
				&& cheb(c.position(), pos) <= (RETREAT_FIX ? static_cast<int>(c.attack_range()) + 1 : 4))
				threats.push_back(c.position());
		why_ = "retreat";
		if (threats.empty())
		{
			try
			{
				me.recall();
			}
			catch (...)
			{
				// Cannot channel here: walk home instead of standing still.
				if (RETREAT_FIX)
					nav::step_towards(w, me, w.ally_spawn);
			}
			return;
		}
		if (!near_enemies.empty())
		{
			engage(near_enemies, true);
			return;
		}
		attack_best(near_enemies);
		if (!nav::step_away(w, me, threats) && RETREAT_FIX)
			nav::step_towards(w, me, w.ally_spawn);
		return;
	}

	if (NO_CHASE && !near_enemies.empty())
	{
		// Only fight enemies close enough to trade blows, or worth chasing (carrying gems).
		const int myr = me.attack_range();
		bool close = false;
		for (const auto& e : near_enemies)
		{
			if (cheb(e.position(), pos) <= std::max(myr, e.attack_range()) + CHASE_SLACK)
			{
				close = true;
				break;
			}
			// A badly hurt enemy is worth chasing down before it heals.
			if (CHASE_WOUNDED)
			{
				try
				{
					if (e.health() < e.max_health() * 0.3f && cheb(e.position(), pos) <= 6)
					{
						close = true;
						break;
					}
				}
				catch (...)
				{
				}
			}
			// An enemy poking a teammate (ranged champions and abilities reach about five tiles) is everyone's fight.
			if (HELP_ALLIES)
			{
				const vec2 ep = e.position();
				for (const auto& a : allies_)
				{
					try
					{
						if (cheb(a.position(), pos) <= 8 && cheb(a.position(), ep) <= HELP_REACH)
							close = true;
					}
					catch (...)
					{
					}
				}
				if (close)
					break;
			}
			try
			{
				if (e.gem_count() >= 30)
				{
					close = true;
					break;
				}
			}
			catch (...)
			{
			}
		}
		if (!close)
			near_enemies.clear();
	}
	if (!near_enemies.empty() && engage(near_enemies))
	{
		why_ = "engage";
		return;
	}

	place_lock();
	if (WARDS)
		place_ward();
	const int gems = static_cast<int>(me.gem_count());
	// Opportunistic tribute when an altar is right here or a couple of steps away.
	if (gems > 0 && near_enemies.empty())
	{
		if (try_submit())
		{
			why_ = "submit";
			return;
		}
		for (std::size_t i = 0; i < w.altars.size(); ++i)
		{
			const vec2 a = w.altars[i];
			if (cheb(a, pos) <= 3 && !camp_fight_active())
			{
				if (break_lock(a))
				{
					why_ = "unlock";
					return;
				}
				if (nav::step_towards(w, me, w.altar_ring(static_cast<int>(i))))
				{
					why_ = "to-altar";
					try_submit();
					return;
				}
			}
		}
	}

	// Claiming a cleared group's reward: stay on its spawn tile until it pays out.
	if (claiming())
	{
		why_ = "claim";
		attack_nearby_camps();
		return;
	}

	if (gems == 0)
		banking_ = false;
	else
	{
		const auto [ai, d] = nearest_altar();
		if (ai < 0 && w.has_islands && gems >= BANK_SOLO && !at_spawn)
		{
			// On an island without an altar: recall home to bank.
			try
			{
				me.recall();
				why_ = "recall-bank";
				return;
			}
			catch (...)
			{
			}
		}
		if (ai >= 0 && should_bank(gems, d))
		{
			banking_ = true;
			altar_ = ai;
			why_ = "bank";
			bank(gems);
			return;
		}
	}

	// Top teams pop home to buy every 40-60 rounds: when passing near spawn with a few shards' worth of gold, buy now.
	if (SHOP_WALK_GOLD > 0 && me.gold_count() >= SHOP_WALK_GOLD && near_enemies.empty() && !at_spawn
		&& w.home_dist(pos) <= SHOP_WALK_DIST && !camp_fight_active())
	{
		if (nav::step_towards(w, me, w.ally_spawn))
		{
			why_ = "shop-walk";
			return;
		}
	}
	if (me.gold_count() >= SHOP_GOLD && gems == 0 && near_enemies.empty() && cheb(pos, w.ally_c) > 6
		&& !camp_fight_active())
	{
		try
		{
			me.recall();
			why_ = "shop-recall";
			return;
		}
		catch (...)
		{
		}
	}

	if (target_ >= 0)
	{
		why_ = "farm";
		farm(w.groups[target_]);
		return;
	}
	why_ = "gather";
	gather();
}

// ------------------------------------------------------------------ combat
float Bot::team_power_allies(const std::vector<cambc::champion>& units) const
{
	float total = 0, dps = 0;
	for (const auto& u : units)
	{
		total += hp_of(u);
		dps += est_dps(u);
	}
	return total * dps;
}

bool Bot::engage(const std::vector<cambc::champion>& near_enemies, bool force_retreat)
{
	std::vector<cambc::champion> vis;
	for (const auto& a : allies_)
	{
		try
		{
			if (cheb(a.position(), pos_) <= 8)
				vis.push_back(a);
		}
		catch (...)
		{
		}
	}
	const float ours = team_power_allies(vis);
	const float theirs = team_power_allies(near_enemies);
	const bool retreat = force_retreat || ours < theirs * FIGHT_RATIO;
	if (retreat)
		retreat_round_ = r_;
	else
		last_fight_ = r_;
	const bool aggressive = !retreat && ours >= theirs * ENGAGE_RATIO;
	return tactician_.turn(*w_, *me_, near_enemies, vis, camps_, retreat, aggressive);
}

// Our power (health x damage of visible allies within 6, us included) against that of enemies within 5.
bool Bot::should_stand(const std::vector<cambc::champion>& near_enemies) const
{
	if (STAND_RATIO <= 0 || near_enemies.empty())
		return false;
	std::vector<cambc::champion> ours, theirs;
	for (const auto& a : allies_)
	{
		try
		{
			if (cheb(a.position(), pos_) <= 6)
				ours.push_back(a);
		}
		catch (...)
		{
		}
	}
	for (const auto& e : near_enemies)
		if (cheb(e.position(), pos_) <= 5)
			theirs.push_back(e);
	if (theirs.empty())
		return false;
	return team_power_allies(ours) >= team_power_allies(theirs) * STAND_RATIO;
}

bool Bot::attack_best(const std::vector<cambc::champion>& enemies)
{
	const cambc::champion& me = *me_;
	if (!me.can_attack())
		return false;
	const bitboard at = me.attack_tiles();
	std::vector<cambc::champion> inr;
	for (const auto& e : enemies)
		if (at.is_set(e.position()))
			inr.push_back(e);
	if (inr.empty())
		return false;
	const auto t = *std::min_element(inr.begin(), inr.end(), [](const auto& a, const auto& b) { return hp_of(a) < hp_of(b); });
	return attack_unit(t, inr);
}

template <class U> bool Bot::attack_unit(const U& target, const std::vector<U>& pool)
{
	const cambc::champion& me = *me_;
	if (!me.can_attack())
		return false;
	const bitboard at = me.attack_tiles();
	U t = target;
	bool ok = hp_of(t) > 0;
	if (ok)
	{
		try
		{
			ok = at.is_set(t.position());
		}
		catch (...)
		{
			ok = false;
		}
	}
	if (!ok)
	{
		bool found = false;
		for (const auto& u : pool)
		{
			try
			{
				if (hp_of(u) > 0 && at.is_set(u.position()) && (!found || hp_of(u) < hp_of(t)))
				{
					t = u;
					found = true;
				}
			}
			catch (...)
			{
			}
		}
		if (!found)
			return false;
	}
	try
	{
		me.attack(t);
	}
	catch (...)
	{
		return false;
	}
	try_reset(t, pool);
	return true;
}

template <class U> void Bot::try_reset(U target, const std::vector<U>& pool)
{
	const cambc::champion& me = *me_;
	for (const auto& a : me.abilities())
	{
		if (!a.is_attack_reset() || !a.is_ready() || !me.can_cast())
			continue;
		try
		{
			a.use();
		}
		catch (...)
		{
			return;
		}
		if (hp_of(target) <= 0)
		{
			const bitboard at = me.attack_tiles();
			bool found = false;
			for (const auto& u : pool)
			{
				try
				{
					if (hp_of(u) > 0 && at.is_set(u.position()) && (!found || hp_of(u) < hp_of(target)))
					{
						target = u;
						found = true;
					}
				}
				catch (...)
				{
				}
			}
			if (!found)
				return;
		}
		try
		{
			me.attack(target);
		}
		catch (...)
		{
		}
		return;
	}
}

bool Bot::cast_heal_ally(const cambc::ability& a)
{
	const bitboard tiles = a.valid_tiles();
	const cambc::champion* best = nullptr;
	float best_frac = 0.8f;
	for (const auto& ally : allies_)
	{
		try
		{
			if (handle_of(ally) == handle_ || !tiles.is_set(ally.position()))
				continue;
			const float frac = ally.health() / std::max(ally.max_health(), 1.0f);
			if (frac < best_frac)
			{
				best = &ally;
				best_frac = frac;
			}
		}
		catch (...)
		{
		}
	}
	if (best == nullptr)
		return false;
	try
	{
		return a.use(*best);
	}
	catch (...)
	{
		return false;
	}
}

bool Bot::cast_on_camp(const cambc::camp& camp, const std::vector<cambc::camp>& live)
{
	const cambc::champion& me = *me_;
	if (!me.can_cast())
		return false;
	const auto abil = me.abilities();
	for (const auto& a : abil)
		if (a.is_heal() && a.is_ready() && cast_heal_ally(a))
			return true;
	for (const auto& a : abil)
	{
		if (!a.is_ready())
			continue;
		if (a.is_shield())
		{
			if (me.health() < me.max_health() * 0.8f)
			{
				try
				{
					if (a.use())
						return true;
				}
				catch (...)
				{
				}
			}
			continue;
		}
		if (a.is_dash())
		{
			// Damage dash (Heart of Ruin): land where the blast hits the most living camps.
			cambc::ability_dash info{};
			try
			{
				info = a.get<cambc::ability_dash>();
			}
			catch (...)
			{
				continue;
			}
			if (info.radius <= 0 || info.modifier <= 0)
				continue;
			const int radius = static_cast<int>(info.radius);
			bitboard tiles;
			try
			{
				tiles = a.valid_tiles() & ~nav::blocked_now(*w_, pos_);
			}
			catch (...)
			{
				continue;
			}
			int best_n = 0;
			vec2 dest{};
			for (const vec2 p : tiles.positions())
			{
				int n = 0;
				for (const auto& c : live)
					if (cheb(p, c.position()) <= radius)
						++n;
				if (n > best_n)
				{
					best_n = n;
					dest = p;
				}
			}
			if (best_n == 0)
				continue;
			try
			{
				if (a.use(dest))
					return true;
			}
			catch (...)
			{
			}
			continue;
		}
		if (a.is_damage() || a.is_aoe_damage() || a.is_cc())
		{
			if (!a.can_target_camps())
				continue;
			bitboard tiles;
			try
			{
				tiles = a.valid_tiles();
			}
			catch (...)
			{
				continue;
			}
			std::optional<cambc::camp> t;
			if (tiles.is_set(camp.position()))
				t = camp;
			else
			{
				for (const auto& c : live)
					if (tiles.is_set(c.position()) && (!t || hp_of(c) < hp_of(*t)))
						t = c;
				if (!t)
					continue;
			}
			try
			{
				if (a.use(*t))
					return true;
			}
			catch (...)
			{
			}
		}
	}
	return false;
}

// ------------------------------------------------------------------ economy
bool Bot::try_submit()
{
	for (const vec2 a : w_->altars)
		if (cheb(a, pos_) <= cambc::constants::submit_gems_range)
		{
			try
			{
				me_->submit_gems(a);
				return true;
			}
			catch (...)
			{
			}
		}
	return false;
}

// Remember what this champion was doing, cheaply, in case it dies before its next turn.
void Bot::record_state(int r)
{
	int ne = 0, nd = 99;
	for (const auto& e : enemies_)
	{
		try
		{
			const int d = cheb(e.position(), pos_);
			if (d <= 8)
				++ne;
			nd = std::min(nd, d);
		}
		catch (...)
		{
		}
	}
	int na = 0;
	for (const auto& a : allies_)
	{
		try
		{
			if (handle_of(a) != handle_ && cheb(a.position(), pos_) <= 6)
				++na;
		}
		catch (...)
		{
		}
	}
	int hpv = -1, gems = -1;
	try
	{
		hpv = static_cast<int>(me_->health());
		gems = static_cast<int>(me_->gem_count());
	}
	catch (...)
	{
	}
	last_state_ = { r, pos_.x, pos_.y, hpv, ne, nd, na, gems, mode_, target_, k_, why_ };
	have_state_ = true;
}

// Keep a ward beside the altar we are at: altars are where carriers get ambushed.
bool Bot::place_ward()
{
	const cambc::champion& me = *me_;
	try
	{
		if (me.ward_cooldown() > 0)
			return false;
	}
	catch (...)
	{
		return false;
	}
	for (const vec2 a : w_->altars)
	{
		if (cheb(a, pos_) > 3)
			continue;
		try
		{
			for (const vec2 wp : me.ward_positions())
				if (cheb(wp, a) <= 6)
					return false;
			const auto tiles = me.ward_placement_tiles().positions();
			if (tiles.empty())
				return false;
			vec2 spot = tiles.front();
			for (auto p : tiles)
				if (cheb(p, a) < cheb(spot, a))
					spot = p;
			return me.place_ward(spot);
		}
		catch (...)
		{
			return false;
		}
	}
	if (ROAM_WARD <= 0 || w_->ally_spawn.is_set(pos_))
		return false;
	// Roaming ward: keep one near us, on the side the enemy comes from (strong teams ward constantly; it is
	// our early warning, and what any champion sees goes into the shared enemy sighting).
	try
	{
		for (const vec2 wp : me.ward_positions())
			if (cheb(wp, pos_) <= ROAM_WARD)
				return false;
		const auto tiles = me.ward_placement_tiles().positions();
		if (tiles.empty())
			return false;
		vec2 spot = tiles.front();
		for (auto p : tiles)
			if (cheb(p, w_->enemy_c) < cheb(spot, w_->enemy_c))
				spot = p;
		return me.place_ward(spot);
	}
	catch (...)
	{
		return false;
	}
}

// Lock any altar within reach that does not already carry our lock.
bool Bot::place_lock()
{
	const cambc::champion& me = *me_;
	try
	{
		const auto& slot = me.consumables()[cambc::consumable_type::altar_lock];
		if (slot.count() == 0)
			return false;
		for (const vec2 a : w_->altars)
		{
			if (cheb(a, pos_) > 2)
				continue;
			try
			{
				if (cambc::vision::has_altar_lock(a) && cambc::vision::altar_lock_at(a).is_ally())
					continue;
				if (slot.use(a))
					return true;
			}
			catch (...)
			{
			}
		}
	}
	catch (...)
	{
	}
	return false;
}

std::optional<cambc::altar_lock> Bot::enemy_lock(vec2 altar) const
{
	try
	{
		for (const auto& lock : cambc::vision::altar_locks(team::enemy))
			if (lock.position() == altar)
				return lock;
	}
	catch (...)
	{
	}
	return std::nullopt;
}

// Attack an enemy altar lock on `altar` when in reach. Returns true if this turn was used.
bool Bot::break_lock(vec2 altar)
{
	const cambc::champion& me = *me_;
	if (cheb(altar, pos_) > 8)
		return false;
	const auto lock = enemy_lock(altar);
	if (!lock)
		return false;
	if (me.can_attack() && me.attack_tiles().is_set(altar))
	{
		try
		{
			me.attack(*lock);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}
	nav::step_towards(*w_, me, ring(altar, me.attack_range()) & ~w_->block);
	return true;
}

// Take carried gems to altar `altar_` (or escort near it when carrying none).
void Bot::bank(int gems)
{
	World& w = *w_;
	const cambc::champion& me = *me_;
	int ai = altar_;
	auto banned = [&](int i) {
		auto it = altar_ban_.find(i);
		return it != altar_ban_.end() && it->second >= r_;
	};
	auto nearest_unbanned = [&]() {
		int best = -1, bd = 0;
		for (std::size_t i = 0; i < w.altars.size(); ++i)
		{
			if (banned(static_cast<int>(i)) || w.altar_guarded(static_cast<int>(i), r_))
				continue;
			const int d = w.altar_dist(static_cast<int>(i), pos_);
			if (best < 0 || d < bd)
			{
				best = static_cast<int>(i);
				bd = d;
			}
		}
		return best;
	};
	if (gems > 0 && banned(ai))
	{
		const int alt = nearest_unbanned();
		if (alt >= 0)
			ai = alt;
	}
	vec2 a = w.altars[ai];
	if (gems > 0 && r_ - retreat_round_ <= 2)
	{
		// We just backed off from enemies: if they are holding this altar, use another.
		for (const auto& e : enemies_)
		{
			if (cheb(e.position(), a) <= 8)
			{
				altar_ban_[ai] = r_ + 40;
				const int alt = nearest_unbanned();
				if (alt >= 0)
				{
					ai = alt;
					a = w.altars[ai];
				}
				break;
			}
		}
	}
	if (gems > 0)
	{
		if (break_lock(a))
			return;
		if (try_submit())
		{
			bank_stuck_ = 0;
			return;
		}
		if (try_shortcut(ai))
			return;
		const bool ok = nav::step_towards(w, me, w.altar_ring(ai));
		if (ok || w.altar_ring(ai).is_set(pos_))
			bank_stuck_ = 0;
		else
		{
			++bank_stuck_;
			if (bank_stuck_ >= 2 && clear_path())
				return;
			if (bank_stuck_ >= 6)
			{
				altar_ban_[ai] = r_ + 40;
				bank_stuck_ = 0;
			}
		}
		attack_nearby_camps();
		return;
	}
	// Escort: stay near the altar without blocking its tribute tiles.
	if (attack_nearby_camps())
		return;
	const int d = cheb(pos_, a);
	if (d > 5 || d <= 2)
		nav::step_towards(w, me, ring(a, 5) & ~ring(a, 2) & ~w.block);
}

// Attack any camp in reach (they may be standing in the only way through).
bool Bot::clear_path()
{
	const cambc::champion& me = *me_;
	if (!me.can_attack())
		return false;
	const bitboard at = me.attack_tiles();
	std::vector<cambc::camp> cs;
	for (const auto& c : camps_)
		if (hp_of(c) > 0 && at.is_set(c.position()))
			cs.push_back(c);
	if (cs.empty())
		return false;
	const auto t = *std::min_element(cs.begin(), cs.end(), [](const auto& a, const auto& b) { return hp_of(a) < hp_of(b); });
	return attack_unit(t, cs);
}

bool Bot::attack_nearby_camps()
{
	std::vector<cambc::camp> live;
	for (const auto& c : camps_)
		if (hp_of(c) > 0 && camp_awake(c))
			live.push_back(c);
	if (live.empty())
		return false;
	const auto t = *std::min_element(live.begin(), live.end(), [](const auto& a, const auto& b) { return hp_of(a) < hp_of(b); });
	return attack_unit(t, live);
}

bool Bot::camp_fight_active() const
{
	for (const auto& c : camps_)
		if (hp_of(c) > 0 && camp_awake(c) && cheb(c.position(), pos_) <= 4)
			return true;
	return false;
}

// ------------------------------------------------------------------ farming
void Bot::farm(const GroupInfo& g)
{
	World& w = *w_;
	const cambc::champion& me = *me_;
	const vec2 pos = pos_;
	if (w.has_islands && (g.tiles & my_land()).is_empty())
	{
		why_ = "hop";
		if (hop_toward(g))
			return;
	}
	auto live = group_camps(g);
	int nearest_spawn = 1 << 20;
	for (auto sp : g.spawns)
		nearest_spawn = std::min(nearest_spawn, cheb(sp, pos));
	if (live.empty() && nearest_spawn <= 6 && (w.reward_ready[g.idx] || group_cleared(g)))
	{
		why_ = "capture";
		capture(g);
		return;
	}
	if (live.empty())
	{
		why_ = "walk";
		const bitboard& ap = approach(g);
		// Long groups: walk on to spawn tiles we cannot see yet before calling it done.
		const bitboard vis = cambc::vision::visible_tiles();
		const bitboard unseen = g.tiles & ~vis;
		if (!unseen.is_empty() && !w.reward_ready[g.idx])
		{
			const auto ups = unseen.positions();
			vec2 near = ups.front();
			for (auto q : ups)
				if (cheb(q, pos) < cheb(near, pos))
					near = q;
			bitboard goal = ring(near, 1) & ~w.block & ~g.tiles;
			if (goal.is_empty() || goal.is_set(pos))
				goal = ap;
			nav::step_towards(w, me, goal);
		}
		else
			nav::step_towards(w, me, ap);
		attack_nearby_camps();
		return;
	}
	// Target: attacking camps first, then the weakest.
	auto key = [](const cambc::camp& c) { return std::make_tuple(attacking_camp(c.kind()) ? 0 : 1, hp_of(c), handle_of(c)); };
	cambc::camp target = live.front();
	for (const auto& c : live)
		if (key(c) < key(target))
			target = c;
	if (CAMP_CAUTION && !camp_awake(target) && r_ - seen_enemy_local_ <= 2)
	{
		// Waking a camp with enemies about invites a third-party fight; wait, but not forever.
		bool enemy_near = false;
		for (const auto& e : enemies_)
		{
			const vec2 ep = e.position();
			if (cheb(ep, pos) <= 10 && !w.enemy_spawn.is_set(ep))
				enemy_near = true;
		}
		if (enemy_near)
		{
			if (caution_round_ != r_ - 1)
				caution_wait_ = 0;
			caution_round_ = r_;
			if (++caution_wait_ <= 15)
			{
				why_ = "caution";
				return;
			}
		}
	}
	if (ASSEMBLE_WAIT > 0 && !camp_awake(target) && attacking_camp(target.kind()) && waiting_for_squad(g))
	{
		why_ = "assemble";
		if (!approach(g).is_set(pos))
			nav::step_towards(w, me, approach(g));
		return;
	}
	const vec2 tpos = target.position();
	const bool in_range = me.attack_tiles().is_set(tpos);
	why_ = in_range ? "fight" : "approach";
	bool cast = false;
	if (in_range || me.attack_range() > 1)
		cast = cast_on_camp(target, live);
	if (!in_range && !cast)
		nav::step_towards(w, me, camp_goal(target));
	attack_unit(target, live);
}

// True while squad mates are still on their way and those already at group g could not safely take its camps
// (the same damage allowance as planning uses); gives up after ASSEMBLE_WAIT rounds.
bool Bot::waiting_for_squad(const GroupInfo& g)
{
	float dps = 0, hp = 0;
	bool coming = false;
	for (const auto* m : squad_members(k_, squad_))
	{
		if (cheb(m->pos, g.center) <= 7)
		{
			dps += m->dps;
			hp += m->hp;
		}
		else if (m->seen >= r_ - 3)
			coming = true;
	}
	if (!coming || g.danger / std::max(dps, 1.0f) <= hp * DANGER_CAP)
		return false;
	if (assemble_round_ != r_ - 1 || assemble_group_ != g.idx)
		assemble_wait_ = 0;
	assemble_round_ = r_;
	assemble_group_ = g.idx;
	return ++assemble_wait_ <= ASSEMBLE_WAIT;
}

// Tiles near a group by walking distance (not through walls), excluding its spawn tiles: standing on
// those would block its respawn.
const bitboard& Bot::approach(const GroupInfo& g)
{
	auto it = approach_cache_.find(g.idx);
	if (it != approach_cache_.end())
		return it->second;
	const World& w = *w_;
	bitboard b;
	for (auto s : g.spawns)
		b.draw_chebyshev(s, 2.0f);
	b &= ~w.block & ~g.tiles;
	cambc::distance_field f;
	bitboard visited;
	f.compute(w.block & ~g.tiles, g.tiles, visited, 3);
	bitboard good;
	for (const vec2 p : b.positions())
		if (f.get(p) <= 3)
			good.set(p);
	if (good.is_empty())
		good = b;
	return approach_cache_.emplace(g.idx, std::move(good)).first->second;
}

// Tiles to stand on: melee next to the camp, ranged at range but behind the tank.
bitboard Bot::camp_goal(const cambc::camp& target)
{
	const cambc::champion& me = *me_;
	const vec2 tpos = target.position();
	const int rng = me.attack_range();
	if (melee_)
		return ring(tpos, 1);
	bitboard goal = ring(tpos, rng) & ~ring(tpos, 1);
	if (attacking_camp(target.kind()) && !tank_engaged(tpos) && waited_ < 3)
	{
		// Wait just outside the camp's reach for a melee ally to take aggro (briefly: we may be standing
		// in the only doorway). Turn-start records: our own move may already have taken allies out of sight.
		bool tank_near = false;
		for (const auto& [h, m] : team_items_)
			if (h != handle_ && m->rng <= 1 && m->seen == r_ && cheb(m->pos, pos_) <= 8)
				tank_near = true;
		if (tank_near)
		{
			if (wait_round_ != r_ - 1)
				waited_ = 0;
			++waited_;
			wait_round_ = r_;
			const int reach = target.attack_range() + 1;
			return ring(tpos, reach + 1) & ~ring(tpos, reach - 1);
		}
	}
	if (wait_round_ != r_ - 1)
		waited_ = 0;
	return goal;
}

bool Bot::tank_engaged(vec2 tpos) const
{
	for (const auto& [h, m] : team_items_)
		if (m->seen == r_ && m->rng <= 1 && cheb(m->pos, tpos) <= 1)
			return true;
	return false;
}

bool Bot::group_cleared(const GroupInfo& g) const
{
	try
	{
		return std::holds_alternative<cambc::map::camp_group_reward_available>(g.group->state());
	}
	catch (...)
	{
		return false;
	}
}

// Visible camps within `radius` tiles.
std::vector<cambc::camp> Bot::near_camps(int radius) const
{
	std::vector<cambc::camp> out;
	const bitboard bb = camp_bb_ & ring(pos_, radius);
	if (bb.is_empty())
		return out;
	for (const vec2 p : bb.positions())
	{
		try
		{
			out.push_back(cambc::vision::camp_at(p));
		}
		catch (...)
		{
		}
	}
	return out;
}

// Visible living camps of group g.
std::vector<cambc::camp> Bot::group_camps(const GroupInfo& g) const
{
	std::vector<cambc::camp> out;
	const bitboard bb = g.tiles & camp_bb_;
	if (bb.is_empty())
		return out;
	for (const vec2 p : bb.positions())
	{
		try
		{
			auto c = cambc::vision::camp_at(p);
			if (hp_of(c) > 0)
				out.push_back(c);
		}
		catch (...)
		{
		}
	}
	return out;
}

// True while we stand on a spawn tile of a cleared group whose reward is still waiting.
bool Bot::claiming() const
{
	const int gi = w_->group_of_tile(pos_);
	if (gi < 0)
		return false;
	const GroupInfo& g = w_->groups[gi];
	if (!(w_->reward_ready[gi] || group_cleared(g)))
		return false;
	return (g.tiles & camp_bb_).is_empty();
}

// Stand on a free spawn tile of a cleared group until the reward is collected; one claimer is enough
// (rewards are split between everyone on the tiles).
void Bot::capture(const GroupInfo& g)
{
	if (g.tiles.is_set(pos_))
		return;
	if (!(cambc::vision::champion_tiles(team::ally) & g.tiles).is_empty())
	{
		attack_nearby_camps();
		return;
	}
	const bitboard free = g.tiles & ~cambc::vision::champion_tiles();
	if (free.is_empty())
	{
		attack_nearby_camps();
		return;
	}
	nav::step_towards(*w_, *me_, free);
}

// No target: wait near the group that respawns soonest.
void Bot::gather()
{
	World& w = *w_;
	int best = -1, best_t = 0;
	for (const auto& g : w.groups)
	{
		const int t = std::max(w.respawn_at[g.idx] - r_, cheb(g.center, pos_) * 3 / 2);
		if (best < 0 || t < best_t)
		{
			best = g.idx;
			best_t = t;
		}
	}
	if (best >= 0)
		nav::step_towards(w, *me_, approach(w.groups[best]));
}

// ------------------------------------------------------------------ islands
namespace
{
// Chebyshev dilation by r tiles.
bitboard dilate(bitboard b, int r)
{
	for (int i = 0; i < r; ++i)
	{
		const bitboard h = b | b.shift_right(1) | b.shift_right(-1);
		b = h | h.shift_up(1) | h.shift_up(-1);
	}
	return b;
}
} // namespace

bool Bot::has_crystal() const
{
	try
	{
		return me_->consumables()[cambc::consumable_type::tp_crystal].count() > 0;
	}
	catch (...)
	{
		return false;
	}
}

// Flash over walls towards altar ai when a teleport crystal lands at least SHORTCUT_GAIN steps closer (the
// three-round channel included) and no enemy is near enough to interrupt it.
bool Bot::try_shortcut(int ai)
{
	World& w = *w_;
	if (SHORTCUT_GAIN <= 0 || !w.altar_field[ai] || !has_crystal())
		return false;
	const auto& f = w.altar_linear(ai);
	const int W = cambc::map::width();
	const int here = f[pos_.y * W + pos_.x];
	const int need = here - SHORTCUT_GAIN - cambc::constants::tp_crystal_time;
	if (need <= 0)
		return false;
	for (const auto& e : enemies_)
		if (cheb(e.position(), pos_) <= 7)
			return false;
	const bitboard cand = ring(pos_, 3) & ~(w.block | cambc::vision::champion_tiles() | cambc::vision::camp_tiles());
	int best_d = need;
	std::optional<vec2> best;
	for (const vec2 t : cand.positions())
	{
		const int d = f[t.y * W + t.x];
		if (d < best_d)
		{
			best_d = d;
			best = t;
		}
	}
	if (!best)
		return false;
	try
	{
		if (me_->consumables()[cambc::consumable_type::tp_crystal].use(*best))
		{
			my_land_valid_ = false;
			why_ = "shortcut";
			return true;
		}
	}
	catch (...)
	{
	}
	return false;
}

// True if a crystal flash from some spawn tile saves SHORTCUT_GAIN steps to an altar (decided once the altar
// distance fields exist).
bool Bot::shortcut_map()
{
	if (SHORTCUT_GAIN <= 0)
		return false;
	if (shortcut_map_ >= 0)
		return shortcut_map_ == 1;
	World& w = *w_;
	for (const auto& f : w.altar_field)
		if (!f)
			return false; // not ready yet
	if (cambc::fuel_remaining() < 40'000)
		return false; // wait for a turn with fuel to spare
	shortcut_map_ = 0;
	const int W = cambc::map::width(), H = cambc::map::height();
	const auto spawn = w.ally_spawn.positions();
	for (std::size_t i = 0; i < w.altars.size(); ++i)
	{
		const auto& f = w.altar_linear(static_cast<int>(i));
		for (const vec2 s : spawn)
		{
			const int here = f[s.y * W + s.x];
			if (here >= 65535)
				continue;
			for (int y = std::max(0, s.y - 3); y <= std::min(H - 1, s.y + 3); ++y)
				for (int x = std::max(0, s.x - 3); x <= std::min(W - 1, s.x + 3); ++x)
					if (f[y * W + x] + SHORTCUT_GAIN + cambc::constants::tp_crystal_time < here)
					{
						shortcut_map_ = 1;
						return true;
					}
		}
	}
	return false;
}

// Tiles reachable on foot from where we stand (our spawn's land unless we have hopped off it).
const bitboard& Bot::my_land()
{
	World& w = *w_;
	if (w.home_land.is_set(pos_))
		return w.home_land;
	if (!my_land_valid_ || !my_land_.is_set(pos_))
	{
		cambc::distance_field f;
		bitboard root, visited;
		root.set(pos_);
		f.compute(w.block & ~root, root, visited);
		my_land_ = visited | root;
		my_land_valid_ = true;
	}
	return my_land_;
}

// Tiles reachable on foot from group g's camps.
const bitboard& Bot::land_of(const GroupInfo& g)
{
	auto it = land_cache_.find(g.idx);
	if (it != land_cache_.end())
		return it->second;
	cambc::distance_field f;
	bitboard visited;
	f.compute(w_->block & ~g.tiles, g.tiles, visited);
	return land_cache_.emplace(g.idx, visited | g.tiles).first->second;
}

// Cross towards group g's land: walk to a shore tile within hop range, then dash (gaps up to the dash's
// range) or teleport (crystal, up to 3). Without a direct hop, hop to the stepping tile nearest the target.
// Returns true if this turn was used.
bool Bot::hop_toward(const GroupInfo& g)
{
	World& w = *w_;
	const cambc::champion& me = *me_;
	int dash_range = 0;
	bool dash_ready = false;
	for (const auto& a : me.abilities())
	{
		if (!a.is_dash())
			continue;
		try
		{
			dash_range = static_cast<int>(a.get<cambc::ability_dash>().range);
			dash_ready = a.is_ready();
		}
		catch (...)
		{
		}
	}
	const bool crystal = has_crystal();
	const int reach = std::max(dash_range, crystal ? 3 : 0);
	if (reach < 2)
	{
		// No way across: home to buy a crystal (spawn shopping buys one on island maps).
		if (!w.ally_spawn.is_set(pos_) && me.gold_count() >= cambc::consumable_type::tp_crystal.cost())
		{
			try
			{
				me.recall();
				return true;
			}
			catch (...)
			{
			}
		}
		return false;
	}
	if (cambc::fuel_remaining() < w.field_cost() * 2 + 15'000)
		return true; // the land searches below cost up to two fields: wait a turn rather than run dry
	const bitboard& mine = my_land();
	const bitboard free = ~(w.block | cambc::vision::champion_tiles() | cambc::vision::camp_tiles());
	bitboard dest = land_of(g) & free;
	bitboard src = mine & dilate(dest, reach);
	if (src.is_empty())
	{
		// Stepping tiles: passable ground off our land within reach of it; take the one nearest the target.
		const bitboard stones = ~w.block & ~mine & dilate(mine, reach) & free;
		if (stones.is_empty())
			return false;
		const auto ps = stones.positions();
		vec2 best = ps.front();
		for (auto p : ps)
			if (cheb(p, g.center) < cheb(best, g.center))
				best = p;
		dest = bitboard();
		dest.set(best);
		src = mine & ring(best, reach);
	}
	if (!src.is_set(pos_))
	{
		nav::step_towards(w, me, src);
		return true;
	}
	// On the shore: land on the reachable tile nearest the target.
	const bitboard here = dest & ring(pos_, reach);
	if (here.is_empty())
		return false;
	const auto ps = here.positions();
	vec2 b = ps.front();
	for (auto p : ps)
		if (cheb(p, g.center) < cheb(b, g.center))
			b = p;
	const int d = cheb(b, pos_);
	try
	{
		if (dash_range >= d)
		{
			if (!dash_ready)
				return true; // wait for the dash
			for (const auto& a : me.abilities())
				if (a.is_dash() && a.use(b))
				{
					my_land_valid_ = false;
					return true;
				}
		}
		if (crystal && d <= 3 && me.consumables()[cambc::consumable_type::tp_crystal].use(b))
		{
			my_land_valid_ = false;
			return true;
		}
	}
	catch (...)
	{
	}
	return false;
}
