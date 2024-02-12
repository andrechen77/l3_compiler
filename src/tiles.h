#pragma once
#include "program.h"
#include "std_alias.h"
#include <iostream>
#include <string>

namespace L3::code_gen::tiles {
	using namespace std_alias;

	// interface
	struct TilePattern {
		virtual std::string to_l2_instructions() const = 0;
		virtual Vec<L3::program::ComputationTree *> get_unmatched() const = 0;
	};

	// outputs a vector of matched tiles
	Vec<Uptr<TilePattern>> tile_trees(Vec<Uptr<L3::program::ComputationTree>> &trees);
}
