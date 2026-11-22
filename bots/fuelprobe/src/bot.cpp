// Map dump: camp groups on the first turn, then the tile grid ten rows per turn, from one champion only.
#include <cambc.h>
#include "bot.h"
#include <iostream>
#include <string>

using cambc::team;
using cambc::vec2;

void Bot::buy_items() {}

namespace
{
int next_row = -1; // -1 before the header, -2 once every row is printed

char camp_char(cambc::camp_type k)
{
	using K = cambc::camp_type;
	switch (k)
	{
	case K::wolf: return 'W';
	case K::elk: return 'E';
	case K::gem_cluster: return 'g';
	case K::gold_cluster: return '$';
	case K::monolith: return 'M';
	case K::golem: return 'G';
	}
	return '?';
}

const char* camp_label(cambc::camp_type k)
{
	using K = cambc::camp_type;
	switch (k)
	{
	case K::wolf: return "WOLF";
	case K::elk: return "ELK";
	case K::gem_cluster: return "GEM_CLUSTER";
	case K::gold_cluster: return "GOLD_CLUSTER";
	case K::monolith: return "MONOLITH";
	case K::golem: return "GOLEM";
	}
	return "?";
}

std::string pos_str(vec2 p) { return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")"; }
} // namespace

void Bot::play_turn()
{
	const auto& me = cambc::self();
	if (!me.is_alive())
		return;
	const auto mine = static_cast<std::uint32_t>(static_cast<cambc::handle_champion_t>(me));
	for (const auto& a : cambc::champions(team::ally))
		if (static_cast<std::uint32_t>(static_cast<cambc::handle_champion_t>(a)) < mine)
			return;
	const int W = cambc::map::width(), H = cambc::map::height();
	static std::string grid;
	if (next_row == -2)
		return;
	if (next_row == -1)
	{
		grid.assign(static_cast<std::size_t>(W * H), '.');
		auto put = [&](const cambc::bitboard& b, char ch)
		{
			for (auto t : b.positions())
				grid[static_cast<std::size_t>(t.y * W + t.x)] = ch;
		};
		put(cambc::map::bushes(), '"');
		put(cambc::map::walls(), '#');
		put(cambc::map::holes(), 'O');
		put(cambc::map::altars(), 'A');
		put(cambc::map::spawn_region(team::ally), '1');
		put(cambc::map::spawn_region(team::enemy), '2');
		std::cout << "MAP " << W << "x" << H << " sym=" << static_cast<int>(cambc::map::symmetry()) << '\n';
		std::string spawn = "ALLYSPAWN [";
		int n = 0;
		for (auto t : cambc::map::spawn_region(team::ally).positions())
		{
			if (n++ == 0)
				spawn += pos_str(t);
		}
		std::cout << spawn << "] " << n << '\n';
		for (const auto& g : cambc::map::camp_groups())
		{
			std::string line = "GROUP gold=" + std::to_string(g.gold_value()) + " gems=" + std::to_string(g.gem_value())
				+ " respawn=" + std::to_string(g.max_respawn_timer()) + " camps=[";
			bool first = true;
			for (const auto& c : g.camps())
			{
				grid[static_cast<std::size_t>(c.spawn_position().y * W + c.spawn_position().x)] = camp_char(c.kind());
				line += (first ? "" : ", ");
				line += "('" + std::string(camp_label(c.kind())) + "', " + pos_str(c.spawn_position()) + ")";
				first = false;
			}
			std::cout << line << "]\n";
		}
		next_row = H - 1;
		return;
	}
	for (int k = 0; k < 10 && next_row >= 0; ++k, --next_row)
	{
		std::string line = "ROW ";
		const std::string num = std::to_string(next_row);
		line += std::string(num.size() < 3 ? 3 - num.size() : 0, ' ') + num + " ";
		line.append(grid, static_cast<std::size_t>(next_row * W), static_cast<std::size_t>(W));
		std::cout << line << '\n';
	}
	if (next_row < 0)
		next_row = -2;
}
