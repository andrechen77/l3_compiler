#pragma once
#include "program.h"
#include <iostream>

// TODO it would make more sense for tiles to be under the L3::code_gen
// namespace since it deals with the target language
namespace L3::program::tiles {
	void tile_trees(Vec<Uptr<ComputationTree>> &trees, std::ostream &o);
}
