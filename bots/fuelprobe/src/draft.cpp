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
} // namespace

void play_draft()
{
	const auto taken = cambc::draft::current_draft(cambc::team::ally);
	for (const auto pick : PRIORITY)
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
