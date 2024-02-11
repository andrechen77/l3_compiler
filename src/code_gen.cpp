#include "code_gen.h"
#include "std_alias.h"
#include "tiles.h"
#include "target_arch.h"

namespace L3::code_gen {
	using namespace std_alias;
	using namespace L3::program;

	Vec<Uptr<ComputationTree>> calculate_computation_trees(const BasicBlock &block) {
		Vec<Uptr<ComputationTree>> result;
		for (const Uptr<Instruction> &inst : block.get_raw_instructions()) {
			result.push_back(mkuptr<ComputationTree>(inst->to_computation_tree()));
		}
		return result;
	}

	void generate_l3_function_code(const L3Function &l3_function, std::ostream &o) {
		// function header
		o << "\t(@" << l3_function.get_name()
			<< " " << l3_function.get_parameter_vars().size() << "\n";

		// assign parameter registers to variables
		const Vec<Variable *> &parameter_vars = l3_function.get_parameter_vars();
		for (int i = 0; i < parameter_vars.size(); ++i) {
			o << "\t\t" << target_arch::get_argument_loading_instruction(
				target_arch::to_l2_expr(parameter_vars[i]),
				i,
				parameter_vars.size()
			) << "\n";
		}

		// print each block
		for (const Uptr<BasicBlock> &block : l3_function.get_blocks()) {
			if (block->get_name().size() > 0) {
				o << "\t\t:" << block->get_name() << "\n";
			}
			Vec<Uptr<ComputationTree>> computation_trees = calculate_computation_trees(*block);
			tiles::tile_trees(computation_trees, o);
		}

		// close
		o << "\t)\n";
	}

	void generate_program_code(Program &program, std::ostream &o) {
		target_arch::mangle_label_names(program);

		o << "(@" << (*program.get_main_function_ref().get_referent())->get_name() << "\n";
		for (const Uptr<L3Function> &function : program.get_l3_functions()) {
			generate_l3_function_code(*function, o);
		}
		o << ")\n";
	}
}