#include "nav.h"
#include <deque>
#include <tuple>

namespace
{
// Tiles where a move recently failed (hidden champions in bushes) and the round each mark expires.
std::vector<std::pair<vec2, int>> bumped;
int stuck = 0;
std::deque<vec2> recent; // tiles left in the last few moves

// The last goal's route field over static walls (terrain plus camps believed alive): reused while the goal
// and the camp knowledge stay the same, since a full field costs as much as a third of a turn's fuel.
struct RouteCache
{
	bool valid = false;
	bitboard goals;
	int wall_version = -1;
	cambc::distance_field field;
};
RouteCache route;
// A teleport crystal (10 gold) channels 3 rounds, then flashes up to 3 tiles — across walls and holes. Use it when
// the flash saves at least this many walking steps beyond the channel (top teams buy ~6 crystals a game; we bought 0.6).
constexpr int CRYSTAL_GAIN = 6;
} // namespace

namespace nav
{
bitboard blocked_now(const World& w, vec2 me_pos)
{
	bitboard b = w.block | cambc::vision::champion_tiles() | cambc::vision::camp_tiles() | cambc::vision::teleport_tiles();
	if (!bumped.empty())
	{
		const int r = static_cast<int>(cambc::round_number());
		std::erase_if(bumped, [r](const auto& e) { return e.second < r; });
		for (const auto& e : bumped)
			b.set(e.first);
	}
	b.set(me_pos, false);
	return b;
}

bool try_moves(const cambc::champion& me, const std::vector<vec2>& candidates)
{
	const int r = static_cast<int>(cambc::round_number());
	const std::size_t n = std::min<std::size_t>(candidates.size(), 3);
	for (std::size_t i = 0; i < n; ++i)
	{
		const vec2 p = candidates[i];
		try
		{
			if (me.move_absolute(p))
				return true;
		}
		catch (...)
		{
		}
		bumped.emplace_back(p, r + 3);
	}
	return false;
}

bool step_towards(World& w, const cambc::champion& me, const bitboard& goals, const bitboard* avoid)
{
	if (!me.can_move())
		return false;
	const vec2 origin = me.position();
	if (goals.is_set(origin))
	{
		stuck = 0;
		return false;
	}
	const int r = static_cast<int>(cambc::round_number());
	const bitboard blocked = blocked_now(w, origin);
	const auto speed = static_cast<std::uint16_t>(me.base_stats().base_move_speed);
	const auto steps = blocked.reachable_tiles(origin, speed).positions();
	std::vector<std::tuple<int, int, vec2>> options;
	// Fast path: the cached static route field, stepping only onto tiles free of units right now.
	{
		const bitboard& cw = w.camp_wall(r);
		if (!route.valid || route.wall_version != w.wall_version() || !(route.goals == goals))
		{
			const bitboard sw = (w.block | cw) & ~goals;
			const bitboard g = goals & ~w.block;
			route.valid = false;
			if (!g.is_empty())
			{
				bitboard visited;
				route.field.compute(sw, g, visited);
				route.goals = goals;
				route.wall_version = w.wall_version();
				route.valid = true;
			}
		}
		if (route.valid && crystal_ok)
		{
			const int here = route.field.get(origin);
			if (here > 0 && here < 0xFFFF && here >= CRYSTAL_GAIN + 6)
			{
				vec2 best{};
				int best_d = here - CRYSTAL_GAIN - cambc::constants::tp_crystal_time;
				bool found = false;
				for (int dy = -3; dy <= 3; ++dy)
					for (int dx = -3; dx <= 3; ++dx)
					{
						const vec2 p{ origin.x + dx, origin.y + dy };
						if (!cambc::map::is_in_bounds(p) || blocked.is_set(p))
							continue;
						const int d = route.field.get(p);
						if (d < best_d)
						{
							best_d = d;
							best = p;
							found = true;
						}
					}
				if (found)
				{
					try
					{
						if (me.consumables()[cambc::consumable_type::tp_crystal].use(best))
							return true;
					}
					catch (...)
					{
					}
				}
			}
		}
		if (route.valid)
		{
			const int here = route.field.get(origin);
			if (here > 0 && here < 0xFFFF)
				for (const vec2 p : steps)
				{
					if (p == origin || std::find(recent.begin(), recent.end(), p) != recent.end())
						continue;
					if (avoid != nullptr && avoid->is_set(p))
						continue;
					const int d = route.field.get(p);
					if (d < here)
						options.emplace_back(d, cheb(p, origin), p);
				}
		}
	}
	bitboard start;
	start.set(origin);
	// Routes avoid visible units and every camp believed alive (so they do not flip as camps go in and
	// out of sight), never step back onto a recently left tile, and fall back to planning through units,
	// then through camps.
	bitboard walls = (blocked | w.camp_wall(r)) & ~start;
	for (int attempt = 0; attempt < 3 && options.empty(); ++attempt)
	{
		// Each fallback search can cost a full-map field: keep enough fuel to finish the turn.
		if (cambc::fuel_remaining() < w.field_cost() + 8'000)
			break;
		if (attempt == 1)
			walls = (w.block | w.camp_wall(r) | cambc::vision::camp_tiles()) & ~start;
		else if (attempt == 2)
			walls = w.block & ~start;
		const bitboard g = goals & ~walls;
		if (g.is_empty())
			continue;
		cambc::distance_field field;
		bitboard visited;
		field.compute_to(walls, g, start, visited);
		const int here = field.get(origin);
		if (here == 0 || here >= 0xFFFF)
			continue;
		for (const vec2 p : steps)
		{
			if (p == origin || std::find(recent.begin(), recent.end(), p) != recent.end())
				continue;
			if (avoid != nullptr && avoid->is_set(p))
				continue;
			const int d = field.get(p);
			if (d < here)
				options.emplace_back(d, cheb(p, origin), p);
		}
		if (!options.empty())
			break;
	}
	if (options.empty())
	{
		if (++stuck >= 3)
			recent.clear();
		return false;
	}
	std::sort(options.begin(), options.end(), [](const auto& a, const auto& b) {
		return std::tie(std::get<0>(a), std::get<1>(a)) < std::tie(std::get<0>(b), std::get<1>(b));
	});
	std::vector<vec2> cands;
	for (const auto& o : options)
		cands.push_back(std::get<2>(o));
	const bool moved = try_moves(me, cands);
	stuck = moved ? 0 : stuck + 1;
	if (moved)
	{
		recent.push_back(origin);
		if (recent.size() > 3)
			recent.pop_front();
	}
	return moved;
}

bool step_away(World& w, const cambc::champion& me, const std::vector<vec2>& threats)
{
	if (!me.can_move() || threats.empty())
		return false;
	const vec2 origin = me.position();
	const bitboard blocked = blocked_now(w, origin);
	auto score_of = [&](vec2 p) {
		int dmin = 1 << 20;
		for (auto t : threats)
			dmin = std::min(dmin, cheb(p, t));
		return dmin * 4 - w.home_dist(p);
	};
	const int here = score_of(origin);
	std::vector<std::pair<int, vec2>> options;
	const auto speed = static_cast<std::uint16_t>(me.base_stats().base_move_speed);
	for (const vec2 p : blocked.reachable_tiles(origin, speed).positions())
	{
		if (p == origin)
			continue;
		const int s = score_of(p);
		if (s > here)
			options.emplace_back(s, p);
	}
	if (options.empty())
		return false;
	std::sort(options.begin(), options.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	std::vector<vec2> cands;
	for (const auto& o : options)
		cands.push_back(o.second);
	return try_moves(me, cands);
}
} // namespace nav
