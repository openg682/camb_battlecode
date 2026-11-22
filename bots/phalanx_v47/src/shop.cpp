#include "shop.h"

namespace
{
using I = cambc::item_type;
using C = cambc::champion_type;

// Builds copied from the top ladder teams: stat-dense Bruiser emblems (7 stat points) and Power
// emblems (6) rather than passive emblems (5 points plus a small passive).
std::vector<I> dps_build() { return { I::power_shard_of_strength, I::bruiser_shard_of_strength, I::power_shard_of_strength,
	I::bruiser_shard_of_health, I::power_shard_of_strength, I::bruiser_shard_of_strength }; }
std::vector<I> bruiser_build() { return { I::bruiser_shard_of_strength, I::bruiser_shard_of_health, I::bruiser_shard_of_strength,
	I::power_shard_of_strength, I::bruiser_shard_of_strength, I::bruiser_shard_of_health }; }
std::vector<I> hybrid_build() { return { I::bruiser_shard_of_strength, I::bruiser_shard_of_focus, I::bruiser_shard_of_health,
	I::bruiser_shard_of_focus, I::bruiser_shard_of_strength, I::bruiser_shard_of_focus }; }
// Top teams' Marcella: Bruiser Health as much as Bruiser Focus (she dies less and still nukes).
constexpr bool MARCELLA_TANKY = true;
std::vector<I> hybrid_tanky_build() { return { I::bruiser_shard_of_focus, I::bruiser_shard_of_health, I::bruiser_shard_of_focus,
	I::bruiser_shard_of_health, I::bruiser_shard_of_strength, I::bruiser_shard_of_health }; }
std::vector<I> caster_build() { return { I::bruiser_shard_of_focus, I::bruiser_shard_of_health, I::bruiser_shard_of_focus,
	I::power_shard_of_focus, I::bruiser_shard_of_focus, I::bruiser_shard_of_health }; }
std::vector<I> tank_build() { return { I::bruiser_shard_of_health, I::bruiser_shard_of_strength, I::bruiser_shard_of_health,
	I::durability_shard_of_health, I::bruiser_shard_of_strength, I::bruiser_shard_of_health }; }
// Engine 2 openers bought before the stat build: 0 none, 1 Jungle (30% less camp damage), 2 Jungle then
// Durability health (10% less damage). Only items that exist in both the 3.0.5 and 4.0.0 SDKs.
constexpr int OPENER = 0;

I jungle_of(I main)
{
	if (main == I::bruiser_shard_of_focus)
		return I::jungle_shard_of_focus;
	if (main == I::bruiser_shard_of_health)
		return I::jungle_shard_of_health;
	return I::jungle_shard_of_strength;
}

} // namespace

Shopper::Shopper(cambc::champion_type kind)
{
	switch (kind)
	{
	case C::ryth:
	case C::ren: build_ = tank_build(); break;
	case C::marcella: build_ = MARCELLA_TANKY ? hybrid_tanky_build() : hybrid_build(); break;
	case C::varg:
	case C::willow:
	case C::vesper_vale: build_ = dps_build(); break;
	case C::heart_of_ruin:
	case C::stellar:
	case C::prism:
	case C::cassi: build_ = caster_build(); break;
	default: build_ = bruiser_build(); break;
	}
	// The main stat is the most common shard of the build.
	I main;
	if (std::count(build_.begin(), build_.end(), I::bruiser_shard_of_focus) >= 3)
		main = I::bruiser_shard_of_focus;
	else if (std::count(build_.begin(), build_.end(), I::bruiser_shard_of_health) >= 3)
		main = I::bruiser_shard_of_health;
	else
		main = I::bruiser_shard_of_strength;
	std::vector<I> opener;
	if (OPENER >= 1)
		opener.push_back(jungle_of(main));
	if (OPENER == 2)
		opener.push_back(I::durability_shard_of_health);
	build_.insert(build_.begin(), opener.begin(), opener.end());
	loop_from_ = static_cast<int>(opener.size());
}

void Shopper::shop(const cambc::champion& me)
{
	// Carry an altar lock: our locks block only enemy tributes.
	try
	{
		const auto lock = cambc::consumable_type::altar_lock;
		if (me.consumables()[lock].count() == 0 && me.gold_count() >= lock.cost())
			me.buy(lock);
	}
	catch (...)
	{
	}
	if (failed_)
		return;
	for (int i = 0; i < 8; ++i)
	{
		// After the whole build, repeat its stat part (the opener's passives are bought once).
		const int n_build = static_cast<int>(build_.size());
		const int at = stage_ < n_build ? stage_ : loop_from_ + (stage_ - n_build) % (n_build - loop_from_);
		const auto shard = build_[static_cast<std::size_t>(at)];
		const std::uint32_t gold = me.gold_count();
		const std::uint32_t cost = shard.cost();
		const std::uint32_t n = std::min<std::uint32_t>(static_cast<std::uint32_t>(5 - bought_), cost ? gold / cost : 0);
		if (n == 0)
			return;
		try
		{
			me.buy(shard, n);
		}
		catch (const std::exception& e)
		{
			std::cout << "shop failed: " << e.what() << '\n';
			failed_ = true;
			return;
		}
		if (me.gold_count() >= gold)
			return; // purchase did not spend gold
		bought_ += static_cast<int>(n);
		if (bought_ >= 5)
		{
			bought_ = 0;
			++stage_;
		}
	}
}
