#pragma once

#include "std_alias.h"
#include "program.h"
#include <string>

namespace L3::code_gen::target_arch {
	using namespace std_alias;
	using namespace L3::program;

	const int64_t WORD_SIZE = 8; // in bytes

	// follows the L2 calling convention
	std::string get_argument_loading_instruction(const std::string &arg_name, int argument_index, int num_args); // TODO fix to take the pre-generated l2 syntax instead of the arg name
	std::string get_argument_prepping_instruction(const std::string &arg_name, int argument_index);

	std::string to_l2_expr(const Variable *var);
	std::string to_l2_expr(const BasicBlock *block);
	std::string to_l2_expr(const Function *function);
	std::string to_l2_expr(int64_t number);
	std::string to_l2_expr(const ComputationTree &tree);
}
