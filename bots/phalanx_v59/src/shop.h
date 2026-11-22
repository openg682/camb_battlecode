#pragma once
// Item builds: each entry is a shard bought five times, which combines into its emblem.
#include "common.h"

class Shopper
{
public:
	explicit Shopper(cambc::champion_type kind);
	void shop(const cambc::champion& me);

private:
	std::vector<cambc::item_type> build_;
	int stage_ = 0;
	int loop_from_ = 0; // first build entry repeated once the build is complete
	int bought_ = 0; // shards of the current stage bought so far
	bool failed_ = false;
};
