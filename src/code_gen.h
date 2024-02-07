#pragma once
#include "program.h"
#include <iostream>

namespace L3::code_gen {
	void generate_l3_function_code(const L3::program::L3Function &l3_function, std::ostream &o);

	void generate_program_code(const L3::program::Program &program, std::ostream &o);
}
