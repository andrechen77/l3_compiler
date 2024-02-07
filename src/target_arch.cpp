#include "target_arch.h"
#include <assert.h>

namespace L3::code_gen::target_arch {
	std::string get_argument_loading_instruction(const std::string &arg_name, int argument_index, int num_args) {
		assert(argument_index >= 0 && num_args > argument_index);
		static const std::string register_args[] = {
			"rdi", "rsi", "rdx", "rcx", "r8", "r9"
		};
		if (argument_index < 6) {
			return "%" + arg_name + " <- " + register_args[argument_index];
		}

		int64_t rsp_offset = WORD_SIZE * (num_args - argument_index - 1);
		return "%" + arg_name + " <- stack-arg " + std::to_string(rsp_offset);
	}
}