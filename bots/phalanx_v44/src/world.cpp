#include "world.h"

static vec2 centroid(const std::vector<vec2>& pts)
{
	if (pts.empty())
		return { 0, 0 };
	long sx = 0, sy = 0;
	for (auto p : pts)
	{
		sx += p.x;
		sy += p.y;
	}
	return { static_cast<std::int32_t>(sx / static_cast<long>(pts.size())),
		static_cast<std::int32_t>(sy / static_cast<long>(pts.size())) };
}

World::World()
{
	W = cambc::map::width();
	H = cambc::map::height();
	walls = cambc::map::walls();
	holes = cambc::map::holes();
	altar_bb = cambc::map::altars();
	ally_spawn = cambc::map::spawn_region(team::ally);
	enemy_spawn = cambc::map::spawn_region(team::enemy);
	block = walls | holes | altar_bb | enemy_spawn;
	altars = altar_bb.positions();
	ally_c = centroid(ally_spawn.positions());
	enemy_c = centroid(enemy_spawn.positions());

	tile_group.assign(static_cast<std::size_t>(W) * H, -1);
	auto all = cambc::map::camp_groups();
	groups.resize(all.size());
	const int capture = cambc::constants::capture_time;
	// Camp stats by type: one design lookup per type instead of one per camp.
	std::array<std::optional<std::pair<float, float>>, 8> type_stats;
	for (std::size_t i = 0; i < all.size(); ++i)
	{
		GroupInfo& g = groups[i];
		g.idx = static_cast<int>(i);
		g.group.emplace(std::move(all[i]));
		for (const auto& c : g.group->camps())
		{
			const vec2 sp = c.spawn_position();
			g.spawns.push_back(sp);
			g.tiles.set(sp);
			tile_group[sp.x + sp.y * W] = static_cast<std::int16_t>(i);
			const auto k = c.kind();
			if (attacking_camp(k))
				++g.attackers;
			auto& ts = type_stats[static_cast<std::size_t>(k) & 7];
			if (!ts)
			{
				const auto st = c.base_stats();
				ts = std::make_pair(st.base_health, st.base_strength);
			}
			g.hp += ts->first;
			g.dps += ts->second;
			if (k == cambc::camp_type::monolith)
				g.has_monolith = true;
		}
		g.center = centroid(g.spawns);
		// Three or more hard-hitting camps: they wake together.
		g.heavy = g.attackers >= 3 && g.dps >= 150;
		g.gold = static_cast<int>(g.group->gold_value());
		g.gems = static_cast<int>(g.group->gem_value());
		g.respawn = g.group->max_respawn_timer();
		g.ally_dist = cheb(g.center, ally_c);
		g.enemy_dist = cheb(g.center, enemy_c);
		g.value = g.gems + GOLD_WEIGHT * g.gold;
		g.danger = g.dps * g.hp;
		g.base_cost = capture + 2 + (g.enemy_dist < g.ally_dist ? 6 : 0);
		cell_groups[cell_key(g.center.x >> 3, g.center.y >> 3)].push_back(g.idx);
	}
	// Gem-starved maps (a few lone clusters, the rest gold): nobody reaches the gem target, so the game goes to
	// the round limit and banked gems decide it. Value every gem far above gold there.
	float gem_rate = 0;
	for (const auto& g : groups)
		if (!g.has_monolith)
			gem_rate += static_cast<float>(g.gems) / static_cast<float>(std::max(g.respawn, 1));
	// Gold-glut maps (long lines of gold clusters between small gem groups): gold stops being worth chasing.
	float gold_rate = 0;
	for (const auto& g : groups)
		gold_rate += static_cast<float>(g.gold) / static_cast<float>(std::max(g.respawn, 1));
	if (GLUT_RATIO > 0 && gold_rate > GLUT_RATIO * std::max(gem_rate, 0.1f))
		for (auto& g : groups)
			g.value = g.gems + GOLD_WEIGHT * GLUT_GOLD_SCALE * g.gold;
	if (SCARCE_GEM_RATE > 0 && gem_rate < SCARCE_GEM_RATE)
		for (auto& g : groups)
			g.value = g.gems * SCARCE_GEM_MULT + GOLD_WEIGHT * g.gold;
	altar_ring_.resize(altars.size());
	altar_field.resize(altars.size());
	group_altar_min.assign(groups.size(), 30);
	respawn_at.assign(groups.size(), 0);
	reward_ready.assign(groups.size(), 0);
}

bool World::precompute(std::uint64_t reserve)
{
	const std::uint64_t need = reserve + field_cost();
	if (!spawn_field)
	{
		if (cambc::fuel_remaining() <= need)
			return false;
		cambc::distance_field f;
		bitboard visited;
		f.compute(block & ~ally_spawn, ally_spawn, visited);
		spawn_field.emplace(std::move(f));
		home_land = visited | ally_spawn;
		for (const auto& g : groups)
			if ((g.tiles & home_land).is_empty())
				has_islands = true;
	}
	for (std::size_t i = 0; i < altars.size(); ++i)
	{
		if (altar_field[i])
			continue;
		if (cambc::fuel_remaining() <= need)
			return false;
		cambc::distance_field f;
		bitboard visited;
		const bitboard& ring_i = altar_ring(static_cast<int>(i));
		f.compute(block & ~ring_i, ring_i, visited);
		altar_field[i].emplace(std::move(f));
	}
	if (!altar_table_done)
	{
		for (auto& g : groups)
		{
			int best = 9999;
			for (std::size_t i = 0; i < altars.size(); ++i)
			{
				const int d = altar_field[i]->get(g.spawns.front());
				if (d < best)
					best = d;
			}
			group_altar_min[g.idx] = best;
		}
		altar_table_done = true;
	}
	return true;
}

const bitboard& World::altar_ring(int i)
{
	auto& r = altar_ring_[i];
	if (!r)
	{
		bitboard b;
		b.draw_chebyshev(altars[i], static_cast<float>(cambc::constants::submit_gems_range));
		r = b & ~block;
	}
	return *r;
}

bool World::altar_guarded(int i, int r)
{
	if (guard_.size() != altars.size())
		guard_.resize(altars.size());
	Guard& g = guard_[i];
	if (!g.done)
	{
		for (const vec2 p : altar_ring(i).positions())
		{
			const int gi = group_of_tile(p);
			if (gi < 0)
				g.open = true;
			else if (std::find(g.groups.begin(), g.groups.end(), gi) == g.groups.end())
				g.groups.push_back(gi);
		}
		g.done = true;
	}
	if (g.open)
		return false;
	for (const int gi : g.groups)
		if (respawn_at[gi] > r)
			return false; // a guard is dead: its tile is free to stand on
	return true;
}

void World::find_guards()
{
	guard_group.assign(groups.size(), 0);
	for (std::size_t i = 0; i < altars.size(); ++i)
	{
		altar_guarded(static_cast<int>(i), 0);
		const Guard& g = guard_[i];
		if (!g.open)
			for (const int gi : g.groups)
				guard_group[gi] = 1;
	}
}

int World::home_dist(vec2 p) const
{
	if (!spawn_field)
		return cheb(ally_c, p) * 3 / 2;
	const int d = spawn_field->get(p);
	return d >= 65535 ? 9999 : d;
}

const std::vector<std::uint16_t>& World::altar_linear(int i)
{
	if (altar_lin_.size() != altars.size())
		altar_lin_.resize(altars.size());
	auto& v = altar_lin_[i];
	if (v.empty() && altar_field[i])
		v = altar_field[i]->export_linear_buffer();
	return v;
}

int World::altar_dist(int i, vec2 p) const
{
	if (!altar_field[i])
		return cheb(altars[i], p) * 3 / 2;
	const int d = altar_field[i]->get(p);
	return d >= 65535 ? 9999 : d;
}

const bitboard& World::camp_wall(int r)
{
	const std::size_t n = groups.size();
	if (wall_key_.size() != n)
		wall_key_.assign(n, 2);
	bool same = wall_valid_;
	for (std::size_t i = 0; i < n && same; ++i)
		if (wall_key_[i] != (respawn_at[i] > r ? 1 : 0))
			same = false;
	if (!same)
	{
		wall_.clear();
		for (std::size_t i = 0; i < n; ++i)
		{
			const char dead = respawn_at[i] > r ? 1 : 0;
			wall_key_[i] = dead;
			if (!dead)
				wall_ |= groups[i].tiles;
		}
		wall_valid_ = true;
		++wall_version_;
	}
	return wall_;
}

void World::groups_near(vec2 p, std::vector<int>& out) const
{
	out.clear();
	const int cx = p.x >> 3, cy = p.y >> 3;
	for (int dx = -1; dx <= 1; ++dx)
		for (int dy = -1; dy <= 1; ++dy)
		{
			auto it = cell_groups.find(cell_key(cx + dx, cy + dy));
			if (it != cell_groups.end())
				out.insert(out.end(), it->second.begin(), it->second.end());
		}
}
