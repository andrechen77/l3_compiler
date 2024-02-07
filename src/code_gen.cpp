#include "code_gen.h"
#include "std_alias.h"
#include "tiles.h"

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

	void generate_l3_function_code(const L3::program::L3Function &l3_function, std::ostream &o) {
		o << "\t(@" << l3_function.get_name()
			<< " " << l3_function.get_parameter_vars().size() << "\n";
		for (const Uptr<BasicBlock> &block : l3_function.get_blocks()) {
			Vec<Uptr<ComputationTree>> computation_trees = calculate_computation_trees(*block);
			tiles::tile_trees(computation_trees, o);
		}
		o << "\t)\n";
	}

	void generate_program_code(const Program &program, std::ostream &o) {
		o << "(@" << (*program.get_main_function_ref().get_referent())->get_name() << "\n";
		for (const Uptr<L3Function> &function : program.get_l3_functions()) {
			generate_l3_function_code(*function, o);
		}
		o << ")\n";
	}
}