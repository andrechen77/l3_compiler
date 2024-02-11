#pragma once
#include "program.h"
#include "std_alias.h"
#include <iostream>

// TODO it would make more sense for tiles to be under the L3::code_gen
// namespace since it deals with the target language
namespace L3::code_gen::tiles {
	using namespace std_alias;

	void tile_trees(Vec<Uptr<L3::program::ComputationTree>> &trees, std::ostream &o);
}
