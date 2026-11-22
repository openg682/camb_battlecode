#include "combat.h"
#include "nav.h"

namespace
{
constexpr float FOCUS_REACH_W = 250.0f;	  // focus bonus per ally (us included) able to hit the target next turn
// A basic attack ends our movement for the turn: when retreating, step away first and attack only if still in reach.
constexpr bool FLEE_FIRST = true;
constexpr bool RETREAT_TO_ALLIES = false; // fall back onto nearby teammates rather than walking home alone
// Ranged champions kite: when a melee enemy could hit us next turn, step out of its reach first (keeping a target in
// range) and shoot after the step - attacking first would end our movement and leave us in its reach.
constexpr bool KITE = true;
// Burst safety for ranged champions: never end a move where the foes that can reach the tile next turn could
// deal our current health (basic attack within its reach, plus a spell of ~2x focus within 4 tiles).
constexpr bool BURST_SAFE = true;
constexpr float BURST_SPELL = 2.0f;	 // assumed spell damage per point of focus
constexpr int SPELL_REACH = 4;		 // spell range (3) plus one step
constexpr float LETHAL_SHARE = 0.9f; // a tile is lethal when the burst reaches this share of our health
constexpr float LETHAL_W = 2000.0f;
constexpr float KITE_W = 4.0f; // extra weight on melee threat when choosing the kiting tile

bool alive(const cambc::champion& u)
{
	try
	{
		return u.is_visible() && u.health() > 0;
	}
	catch (...)
	{
		return false;
	}
}

std::pair<float, float> split_threat(vec2 t, const std::vector<Foe>& foes, const std::vector<CampThreat>& camps)
{
	float m = 0, r = 0;
	for (const auto& c : camps)
		if (cheb(t, c.pos) <= c.rng)
			r += c.dps;
	for (const auto& f : foes)
		if (cheb(t, f.pos) <= f.reach())
		{
			if (f.rng <= 1)
				m += f.dps;
			else
				r += f.dps;
		}
	return { m, r };
}
} // namespace

float Tactician::melee_threat(vec2 t) const
{
	float s = 0;
	for (const auto& f : foes_)
		if (f.rng <= 1 && !f.stunned && cheb(t, f.pos) <= f.reach())
			s += f.dps;
	return s;
}

bool Tactician::kite_tile_exists() const
{
	const float here = melee_threat(pos_);
	if (here <= 0)
		return false;
	const bitboard blocked = nav::blocked_now(*w_, pos_);
	const auto speed = static_cast<std::uint16_t>(me_->base_stats().base_move_speed);
	for (const vec2 t : blocked.reachable_tiles(pos_, speed).positions())
	{
		if (t == pos_ || melee_threat(t) >= here)
			continue;
		for (const auto& f : foes_)
			if (cheb(t, f.pos) <= rng_)
				return true;
	}
	return false;
}

float Tactician::burst_at(vec2 t) const
{
	float s = 0;
	for (const auto& f : foes_)
	{
		if (f.stunned)
			continue;
		const int d = cheb(t, f.pos);
		if (d <= f.reach())
			s += f.dps / 1.1f;
		if (d <= SPELL_REACH)
			s += f.burst;
	}
	return s;
}

bool Tactician::lethal(vec2 t) const
{
	return burst_at(t) >= me_->health() * LETHAL_SHARE;
}

float Tactician::threat_at(vec2 t) const
{
	float s = 0;
	for (const auto& f : foes_)
		if (cheb(t, f.pos) <= f.reach())
			s += f.dps;
	return s;
}

int Tactician::find(std::uint32_t handle) const
{
	for (std::size_t i = 0; i < foes_.size(); ++i)
		if (foes_[i].handle == handle)
			return static_cast<int>(i);
	return -1;
}

void Tactician::prune()
{
	std::erase_if(foes_, [](const Foe& f) { return !alive(f.u); });
}

// Team-consistent focus: low health, gem carriers, close to our group.
int Tactician::pick_focus() const
{
	int best = -1;
	float best_s = 0;
	for (std::size_t i = 0; i < foes_.size(); ++i)
	{
		const Foe& f = foes_[i];
		int reach = 0;
		for (const vec2 a : ally_pos_)
			if (cheb(a, f.pos) <= 4)
				++reach;
		const float s = f.hp - FOCUS_REACH_W * static_cast<float>(reach) + 60.0f * cheb(f.pos, anchor_) - (f.gems > 0 ? 250.0f : 0.0f) - (f.stunned ? 150.0f : 0.0f)
			- (f.linked ? 300.0f : 0.0f);
		if (best < 0 || s < best_s || (s == best_s && f.handle < foes_[best].handle))
		{
			best = static_cast<int>(i);
			best_s = s;
		}
	}
	return best;
}

int Tactician::killable_in_range() const
{
	if (!me_->can_attack())
		return -1;
	int best = -1;
	for (std::size_t i = 0; i < foes_.size(); ++i)
	{
		const Foe& f = foes_[i];
		if (f.hp <= str_ && in_range(f))
			if (best < 0 || f.gems > foes_[best].gems || (f.gems == foes_[best].gems && f.dps > foes_[best].dps))
				best = static_cast<int>(i);
	}
	return best;
}

int Tactician::best_in_range(int focus) const
{
	if (focus >= 0 && alive(foes_[focus].u) && in_range(foes_[focus]))
		return focus;
	int best = -1;
	for (std::size_t i = 0; i < foes_.size(); ++i)
	{
		const Foe& f = foes_[i];
		if (!in_range(f) || !alive(f.u))
			continue;
		if (best < 0 || f.hp < foes_[best].hp || (f.hp == foes_[best].hp && f.handle < foes_[best].handle))
			best = static_cast<int>(i);
	}
	return best;
}

bool Tactician::basic(Foe& f)
{
	try
	{
		me_->attack(f.u);
	}
	catch (...)
	{
		return false;
	}
	f.hp = alive(f.u) ? hp_of(f.u) : 0.0f;
	return true;
}

void Tactician::attack_reset(std::uint32_t focus_handle)
{
	if (!me_->can_cast())
		return;
	for (const auto& a : me_->abilities())
	{
		if (!a.is_attack_reset() || !a.is_ready())
			continue;
		const int t = best_in_range(find(focus_handle));
		if (t < 0)
			return;
		try
		{
			a.use();
		}
		catch (...)
		{
			return;
		}
		basic(foes_[t]);
		return;
	}
}

bool Tactician::cast_heal(const cambc::ability& a, const std::vector<cambc::champion>& allies)
{
	const bitboard tiles = a.valid_tiles();
	const cambc::champion* best = nullptr;
	float best_frac = 0.75f;
	for (const auto& ally : allies)
	{
		try
		{
			if (handle_of(ally) == handle_of(*me_) || !tiles.is_set(ally.position()))
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

bool Tactician::cast_dash(const cambc::ability& a, int focus, bool retreat)
{
	const bitboard blocked = nav::blocked_now(*w_, pos_);
	const bitboard tiles = a.valid_tiles() & ~blocked;
	if (tiles.is_empty())
		return false;
	vec2 dest{};
	if (retreat)
	{
		if (me_->health() > me_->max_health() * 0.4f || threat_at(pos_) == 0)
			return false;
		bool found = false;
		float best_t = 0;
		int best_h = 0;
		for (const vec2 p : tiles.positions())
		{
			const float t = threat_at(p);
			const int h = w_->home_dist(p);
			if (!found || t < best_t || (t == best_t && h < best_h))
			{
				found = true;
				dest = p;
				best_t = t;
				best_h = h;
			}
		}
		if (!found || threat_at(dest) >= threat_at(pos_))
			return false;
	}
	else
	{
		if (focus < 0 || in_range(foes_[focus]) || foes_[focus].hp > str_ * 4)
			return false;
		bool found = false;
		float best_t = 0;
		for (const vec2 p : tiles.positions())
		{
			if (cheb(p, foes_[focus].pos) > rng_)
				continue;
			const float t = threat_at(p);
			if (!found || t < best_t)
			{
				found = true;
				dest = p;
				best_t = t;
			}
		}
		if (!found)
			return false;
	}
	try
	{
		return a.use(dest);
	}
	catch (...)
	{
		return false;
	}
}

bool Tactician::cast(int& focus, const std::vector<cambc::champion>& allies, bool retreat)
{
	if (!me_->can_cast())
		return false;
	const auto abil = me_->abilities();
	const float threat_here = threat_at(pos_);
	for (const auto& a : abil)
	{
		if (!a.is_ready())
			continue;
		if (a.is_heal())
		{
			if (cast_heal(a, allies))
				return true;
		}
		else if (a.is_shield())
		{
			if (threat_here > 0 && me_->health() < me_->max_health() * 0.9f)
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
		}
	}
	// Offensive casts forbid moving this turn. Ranged casters skip them when a melee enemy could step
	// next to them and they would rather kite.
	bool melee_close = false;
	for (const auto& f : foes_)
		if (f.rng <= 1 && cheb(f.pos, pos_) <= 2 && !f.stunned)
			melee_close = true;
	for (const auto& a : abil)
	{
		if (!a.is_ready())
			continue;
		const bool is_cc = a.is_cc(), is_dmg = a.is_damage(), is_aoe = a.is_aoe_damage();
		if (!(is_cc || is_dmg || is_aoe))
			continue;
		const auto& info = a.info();
		const bool exclusive = info ? std::visit([](const auto& v) { return v.exclusive_with_movement; }, *info) : false;
		if (exclusive && rng_ > 1 && melee_close && !retreat)
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
		std::vector<int> cands;
		for (std::size_t i = 0; i < foes_.size(); ++i)
			if (tiles.is_set(foes_[i].pos) && alive(foes_[i].u))
				cands.push_back(static_cast<int>(i));
		if (cands.empty())
			continue;
		int t = -1;
		if (is_cc)
		{
			// Stun whoever threatens most; a stunned enemy cannot act or flee.
			float best = -1;
			for (int i : cands)
			{
				const Foe& f = foes_[i];
				if (f.stunned)
					continue;
				const float s = f.dps * (f.hp < 300 ? 2.0f : 1.0f);
				if (t < 0 || s > best || (s == best && f.handle < foes_[t].handle))
				{
					t = i;
					best = s;
				}
			}
			if (t < 0)
				continue;
		}
		else if (is_aoe)
		{
			int best_n = -1;
			for (int i : cands)
			{
				int n = 0;
				for (int j : cands)
					if (cheb(foes_[j].pos, foes_[i].pos) <= 1)
						++n;
				if (t < 0 || n > best_n || (n == best_n && foes_[i].hp < foes_[t].hp))
				{
					t = i;
					best_n = n;
				}
			}
		}
		else
		{
			if (std::find(cands.begin(), cands.end(), focus) != cands.end())
				t = focus;
			else
				for (int i : cands)
					if (t < 0 || foes_[i].hp < foes_[t].hp)
						t = i;
		}
		try
		{
			if (a.use(foes_[t].u))
			{
				foes_[t].hp = alive(foes_[t].u) ? hp_of(foes_[t].u) : 0.0f;
				if (is_cc)
					foes_[t].stunned = true;
				return true;
			}
		}
		catch (...)
		{
		}
	}
	// Damage dash (Heart of Ruin): land where the blast stuns the most enemies.
	if (!retreat)
		for (const auto& a : abil)
			if (a.is_dash() && a.is_ready() && cast_blast_dash(a, focus))
				return true;
	// Dash: close on a low target, or escape when retreating.
	for (const auto& a : abil)
		if (a.is_dash() && a.is_ready() && cast_dash(a, focus, retreat))
			return true;
	return false;
}

bool Tactician::cast_blast_dash(const cambc::ability& a, int focus)
{
	cambc::ability_dash info{};
	try
	{
		info = a.get<cambc::ability_dash>();
	}
	catch (...)
	{
		return false;
	}
	if (info.radius <= 0 || info.modifier <= 0)
		return false;
	const float dmg = info.modifier * me_->focus();
	const int radius = static_cast<int>(info.radius);
	const bitboard tiles = a.valid_tiles() & ~nav::blocked_now(*w_, pos_);
	if (tiles.is_empty())
		return false;
	bool found = false;
	vec2 dest{};
	float best = 0;
	for (const vec2 p : tiles.positions())
	{
		float s = 0;
		for (std::size_t i = 0; i < foes_.size(); ++i)
		{
			const Foe& f = foes_[i];
			if (cheb(p, f.pos) > radius)
				continue;
			s += 1.0f + (f.hp <= dmg ? 2.0f : 0.0f) + (static_cast<int>(i) == focus ? 0.5f : 0.0f) + (f.stunned ? -0.5f : 0.0f);
		}
		if (s <= 0)
			continue;
		const auto [mt, rt] = split_threat(p, foes_, camp_threats_);
		s -= (mt + rt) / 2000.0f; // mild preference for safer landings
		if (!found || s > best)
		{
			found = true;
			dest = p;
			best = s;
		}
	}
	if (!found)
		return false;
	try
	{
		if (!a.use(dest))
			return false;
	}
	catch (...)
	{
		return false;
	}
	for (auto& f : foes_)
		if (cheb(dest, f.pos) <= radius)
		{
			f.hp = alive(f.u) ? hp_of(f.u) : 0.0f;
			f.stunned = true;
		}
	pos_ = me_->position();
	return true;
}

bool Tactician::move(int focus, bool attacked, bool retreat, bool aggressive)
{
	const vec2 origin = pos_;
	const bitboard blocked = nav::blocked_now(*w_, origin);
	const auto speed = static_cast<std::uint16_t>(me_->base_stats().base_move_speed);
	auto tiles = blocked.reachable_tiles(origin, speed).positions();
	if (std::find(tiles.begin(), tiles.end(), origin) == tiles.end())
		tiles.push_back(origin);
	const bool ranged = rng_ > 1;
	const bool low = !standing && me_->health() < me_->max_health() * 0.35f;
	const Foe* fo = focus >= 0 ? &foes_[focus] : nullptr;
	bool have = false;
	vec2 best{};
	float best_s = 0;
	for (const vec2 t : tiles)
	{
		const auto [mt, rt] = split_threat(t, foes_, camp_threats_);
		float s = 0;
		if (retreat || low)
		{
			if (RETREAT_TO_ALLIES && anchor_others_)
				s -= (mt + rt) * 3 + 8.0f * cheb(t, *anchor_others_);
			else
				s -= (mt + rt) * 3 + w_->home_dist(t) * 4.0f;
		}
		else
		{
			if (!attacked)
			{
				bool any_in = false;
				for (const auto& f : foes_)
					if (cheb(t, f.pos) <= rng_)
						any_in = true;
				if (fo != nullptr && cheb(t, fo->pos) <= rng_)
					s += 400;
				else if (any_in)
					s += 330;
				else if (fo != nullptr)
					s -= (aggressive ? 40.0f : 15.0f) * cheb(t, fo->pos);
			}
			else if (!ranged && fo != nullptr)
			{
				// Melee stays glued to its target after hitting it.
				const int d = cheb(t, fo->pos);
				s += d <= 1 ? 100.0f : -10.0f * d;
			}
			if (aggressive)
				s -= mt * (ranged ? 1.5f : 0.1f) + rt * 0.2f; // trading blows is fine when we are stronger
			else
				s -= (mt + rt) * (ranged ? 1.0f : 0.3f);
			if (kite_)
				s -= mt * KITE_W;
			if (BURST_SAFE && ranged && lethal(t))
				s -= LETHAL_W;
			s -= 60.0f * std::max(0, cheb(t, anchor_) - 3); // never run ahead of the group
		}
		s -= 0.01f * (t.x * 7 + t.y); // tie-break
		if (!have || s > best_s)
		{
			have = true;
			best = t;
			best_s = s;
		}
	}
	if (!have || best == origin)
		return false;
	return nav::try_moves(*me_, { best });
}

bool Tactician::turn(World& w, const cambc::champion& me, const std::vector<cambc::champion>& enemies,
	const std::vector<cambc::champion>& allies, const std::vector<cambc::camp>& camps, bool retreat, bool aggressive)
{
	w_ = &w;
	me_ = &me;
	pos_ = me.position();
	rng_ = me.attack_range();
	str_ = me.strength();
	foes_.clear();
	for (const auto& e : enemies)
	{
		if (!alive(e))
			continue;
		Foe f{ e };
		try
		{
			f.pos = e.position();
			f.hp = e.health();
			f.dps = est_dps(e);
			try
			{
				f.burst = e.focus() * BURST_SPELL;
			}
			catch (...)
			{
				f.burst = 100.0f;
			}
			f.rng = e.attack_range();
			f.handle = handle_of(e);
			const auto kind = e.champion_name();
			f.linked = kind == cambc::champion_type::cassi || kind == cambc::champion_type::issac; // killing either kills both
		}
		catch (...)
		{
			continue;
		}
		try
		{
			f.gems = e.gem_count();
		}
		catch (...)
		{
			f.gems = 0;
		}
		try
		{
			f.stunned = e.buffs().has_stun();
		}
		catch (...)
		{
		}
		foes_.push_back(f);
	}
	if (foes_.empty())
		return false;
	camp_threats_.clear();
	for (const auto& c : camps)
	{
		try
		{
			if (attacking_camp(c.kind()) && c.is_awake() && c.health() > 0 && cheb(c.position(), pos_) <= 5)
				camp_threats_.push_back({ c.position(), c.strength(), c.attack_range() });
		}
		catch (...)
		{
		}
	}
	long sx = 0, sy = 0, ox = 0, oy = 0;
	int n = 0, no = 0;
	const std::uint32_t mine = handle_of(me);
	for (const auto& a : allies)
	{
		try
		{
			const vec2 p = a.position();
			sx += p.x;
			sy += p.y;
			++n;
			if (handle_of(a) != mine)
			{
				ox += p.x;
				oy += p.y;
				++no;
			}
		}
		catch (...)
		{
		}
	}
	ally_pos_.clear();
	for (const auto& a : allies)
	{
		try
		{
			ally_pos_.push_back(a.position());
		}
		catch (...)
		{
		}
	}
	anchor_ = n ? vec2{ static_cast<std::int32_t>(sx / n), static_cast<std::int32_t>(sy / n) } : pos_;
	anchor_others_.reset();
	if (no)
		anchor_others_ = vec2{ static_cast<std::int32_t>(ox / no), static_cast<std::int32_t>(oy / no) };
	int focus = pick_focus();
	std::uint32_t fh = foes_[focus].handle;

	bool attacked = false;
	// 1. A kill in range right now always comes first.
	const int kill = killable_in_range();
	if (kill >= 0)
	{
		const std::uint32_t kh = foes_[kill].handle;
		attacked = basic(foes_[kill]);
		prune();
		if (foes_.empty())
			return true;
		focus = find(fh);
		if (fh == kh || focus < 0)
		{
			focus = pick_focus();
			fh = foes_[focus].handle;
		}
	}

	// 2. Heal / shield / stun / nuke if worth standing still for.
	cast(focus, allies, retreat);
	prune();
	if (foes_.empty())
		return true;
	focus = find(fh);
	if (focus < 0)
	{
		focus = pick_focus();
		fh = foes_[focus].handle;
	}

	// 3. Attack before moving when the focus (or anyone) is already in range (not when fleeing or kiting).
	kite_ = KITE && !attacked && rng_ > 1 && !retreat && me.can_move() && (kite_tile_exists() || (BURST_SAFE && lethal(pos_)));
	if (!attacked && me.can_attack() && !(FLEE_FIRST && retreat) && !kite_)
	{
		const int t = best_in_range(focus);
		if (t >= 0)
		{
			const std::uint32_t th = foes_[t].handle;
			attacked = basic(foes_[t]);
			prune();
			focus = find(fh);
			if (th == fh && focus < 0 && !foes_.empty())
			{
				focus = pick_focus();
				fh = foes_[focus].handle;
			}
		}
	}

	// 4. Move to the best tile.
	if (me.can_move() && !foes_.empty())
	{
		move(focus, attacked, retreat, aggressive);
		pos_ = me.position();
	}

	// 5. Attack after moving if we have not yet.
	if (!attacked && me.can_attack() && !foes_.empty())
	{
		const int t = best_in_range(find(fh));
		if (t >= 0)
			attacked = basic(foes_[t]);
	}
	if (attacked)
		attack_reset(fh);
	return true;
}
