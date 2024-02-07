#pragma once

#include "std_alias.h"
#include <string>

namespace L3::code_gen::target_arch {
	using namespace std_alias;

	const int64_t WORD_SIZE = 8; // in bytes

	// follows the L2 calling convention
	std::string get_argument_loading_instruction(const std::string &arg_name, int argument_index, int num_args);
}
