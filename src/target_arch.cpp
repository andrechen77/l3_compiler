#include "target_arch.h"
#include <assert.h>

namespace L3::code_gen::target_arch {
	static const std::string register_args[] = {
		"rdi", "rsi", "rdx", "rcx", "r8", "r9"
	};

	std::string get_argument_loading_instruction(const std::string &l2_syntax, int argument_index, int num_args) {
		assert(argument_index >= 0 && num_args > argument_index);
		if (argument_index < 6) {
			return l2_syntax + " <- " + register_args[argument_index];
		}

		int64_t rsp_offset = WORD_SIZE * (num_args - argument_index - 1);
		return l2_syntax + " <- stack-arg " + std::to_string(rsp_offset);
	}

	std::string get_argument_prepping_instruction(const std::string &l2_syntax, int argument_index) {
		assert(argument_index >= 0);
		if (argument_index < 6) {
			return register_args[argument_index] + " <- " + l2_syntax;
		}

		int64_t rsp_offset = -WORD_SIZE * (argument_index + 2);
		return "mem rsp " + std::to_string(rsp_offset) + " <- " + l2_syntax;
	}

	std::string to_l2_expr(const Variable *var){
		return "%_" + var->get_name();
	}
	std::string to_l2_expr(const BasicBlock *block){
		return ":" + block->get_name();
	}
	std::string to_l2_expr(const Function *function){
		if (dynamic_cast<const L3Function *>(function)) {
			return "@" + function->get_name();
		} else {
			return function->get_name();
		}
	}
	std::string to_l2_expr(int64_t number){
		return std::to_string(number);
	}
	std::string to_l2_expr(const ComputationTree &tree) {
		if (Variable *const *variable = std::get_if<Variable *>(&tree)) {
			return to_l2_expr(*variable);
		} else if (BasicBlock *const *block = std::get_if<BasicBlock *>(&tree)) {
			return to_l2_expr(*block);
		} else if (Function *const *function = std::get_if<Function *>(&tree)) {
			return to_l2_expr(*function);
		} else if (const int64_t *number = std::get_if<int64_t>(&tree)) {
			return to_l2_expr(*number);
		} else {
			std::cout << "Error: I don't know how to convert this type of tree into L2 syntax.\n";
			exit(1);
		}
	}

	void mangle_label_names(Program &program) {
		for (Uptr<L3Function> &l3_function : program.get_l3_functions()) {
			for (Uptr<BasicBlock> &block : l3_function->get_blocks()) {
				if (block->get_name().size() > 0) {
					block->mangle_name("_" + l3_function->get_name() + block->get_name());
				}
			}
		}
	}
}