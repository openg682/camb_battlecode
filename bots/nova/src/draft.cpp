// Draft: take the highest-ranked champion still available.
#include <cambc.h>
#include "draft.h"
#include <algorithm>
#include <array>

namespace
{
using D = cambc::champion_draft;
// Re-tuned for the 3.0 rebalance: Vesper Vale (tougher, +33% gold) replaces the nerfed Stellar.
constexpr std::array PRIORITY{ D::vesper_vale, D::ryth, D::marcella, D::tavi, D::willow, D::varg, D::vendetta,
	D::heart_of_ruin, D::ren, D::prism, D::stellar, D::cassi_and_issac };
// Engine 2 alternatives (Vendetta: strength 70 against 45, kills reset its cooldowns).
constexpr std::array PRIORITY_V1{ D::vesper_vale, D::ryth, D::marcella, D::vendetta, D::tavi, D::willow, D::varg,
	D::heart_of_ruin, D::ren, D::prism, D::stellar, D::cassi_and_issac };
constexpr std::array PRIORITY_V2{ D::vendetta, D::vesper_vale, D::ryth, D::marcella, D::tavi, D::willow, D::varg,
	D::heart_of_ruin, D::ren, D::prism, D::stellar, D::cassi_and_issac };
constexpr std::array PRIORITY_V3{ D::vesper_vale, D::vendetta, D::marcella, D::heart_of_ruin, D::ryth, D::tavi,
	D::willow, D::varg, D::ren, D::prism, D::stellar, D::cassi_and_issac };
// Engine 3 (Varg strength 45 -> 50; Vesper and Marcella nerfed): Varg instead of Willow / of Tavi; Ryth first.
constexpr std::array PRIORITY_V4{ D::vesper_vale, D::ryth, D::marcella, D::tavi, D::varg, D::willow, D::vendetta,
	D::heart_of_ruin, D::ren, D::prism, D::stellar, D::cassi_and_issac };
constexpr std::array PRIORITY_V5{ D::vesper_vale, D::ryth, D::marcella, D::varg, D::willow, D::tavi, D::vendetta,
	D::heart_of_ruin, D::ren, D::prism, D::stellar, D::cassi_and_issac };
constexpr std::array PRIORITY_V6{ D::ryth, D::vesper_vale, D::marcella, D::tavi, D::willow, D::varg, D::vendetta,
	D::heart_of_ruin, D::ren, D::prism, D::stellar, D::cassi_and_issac };
// The top teams' engine-3 drafts: Intercontinental Fish (pair, Heart, Vendetta, Willow, Vesper/Marcella) and
// Mite (Heart, Vendetta, pair, Vesper, Marcella).
constexpr std::array PRIORITY_V8{ D::cassi_and_issac, D::heart_of_ruin, D::vendetta, D::vesper_vale, D::willow, D::marcella,
	D::ryth, D::tavi, D::varg, D::ren, D::prism, D::stellar };
constexpr std::array PRIORITY_V9{ D::vesper_vale, D::heart_of_ruin, D::vendetta, D::cassi_and_issac, D::marcella, D::ryth,
	D::tavi, D::willow, D::varg, D::ren, D::prism, D::stellar };
// 0 = PRIORITY, 1-6 = the alternatives above.
constexpr int DRAFT_VARIANT = 0;
// Cassi and Issac come as a pair (one pick, two champions): move that pick to this place in the order
// (-1 keeps it last).
constexpr int PAIR_SLOT = -1;

// Three-a-side maps: Vesper (strength 35 after two nerfs) dies most there; opponents often field the Cassi+Issac
// pair (four bodies against three). 0 = same order as larger maps.
constexpr int SMALL_DRAFT = 2;
constexpr std::array SMALL_S1{ D::cassi_and_issac, D::ryth, D::marcella };
constexpr std::array SMALL_S2{ D::ryth, D::marcella, D::tavi };
constexpr std::array SMALL_S3{ D::ryth, D::cassi_and_issac, D::marcella };

std::array<D, PRIORITY.size()> order()
{
	auto o = DRAFT_VARIANT == 1 ? PRIORITY_V1 : DRAFT_VARIANT == 2 ? PRIORITY_V2 : DRAFT_VARIANT == 3 ? PRIORITY_V3
		: DRAFT_VARIANT == 4 ? PRIORITY_V4 : DRAFT_VARIANT == 5 ? PRIORITY_V5 : DRAFT_VARIANT == 6 ? PRIORITY_V6 : DRAFT_VARIANT == 8 ? PRIORITY_V8 : DRAFT_VARIANT == 9 ? PRIORITY_V9 : PRIORITY;
	std::uint32_t size = 5;
	try
	{
		size = cambc::draft::team_size();
	}
	catch (...)
	{
	}
	if (SMALL_DRAFT > 0 && size <= 3)
	{
		const auto& small = SMALL_DRAFT == 1 ? SMALL_S1 : SMALL_DRAFT == 2 ? SMALL_S2 : SMALL_S3;
		for (std::size_t k = 0; k < small.size(); ++k)
		{
			auto it = std::find(o.begin(), o.end(), small[k]);
			std::rotate(o.begin() + static_cast<long>(k), it, it + 1);
		}
	}
	if (PAIR_SLOT >= 0)
	{
		auto it = std::find(o.begin(), o.end(), D::cassi_and_issac);
		std::rotate(o.begin() + PAIR_SLOT, it, it + 1);
	}
	return o;
}
} // namespace

void play_draft()
{
	const auto taken = cambc::draft::current_draft(cambc::team::ally);
	for (const auto pick : order())
	{
		if (std::find(taken.begin(), taken.end(), pick) != taken.end())
			continue;
		try
		{
			cambc::draft::pick(pick);
			return;
		}
		catch (...)
		{
		}
	}
}
