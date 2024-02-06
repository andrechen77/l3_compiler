#include "tiles.h"
#include "std_alias.h"
#include <iostream>

namespace L3::program::tiles {
	// "CTR" stands for "computation tree rule", and is kind of like a pegtl
	// parsing rule but instead of matching characters it matches parts of
	// a computation tree.

	using CtrOutput = Vec<ComputationTree *>;

	template<int index>
	struct VariableCtr {
		static bool match(ComputationTree &target, CtrOutput &o) {
			if (std::holds_alternative<Variable *>(target)) {
				std::cout << "matched variable!\n";
				o[index] = &target;
				return true;
			}
			return false;
		}
	};

	template<typename SourceCtr, int index>
	struct MoveCtr {
		static bool match(ComputationTree &target, CtrOutput &o) {
			if (Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target)) {
				if (MoveComputation *move_node = dynamic_cast<MoveComputation *>(node->get())) {
					std::cout << "matched move tree, checking child\n";
					if (SourceCtr::match(move_node->source, o)) {
						o[index] = &target;
						return true;
					} else {
						return false;
					}
				}
			}
			return false;
		}
	};

	template<int num_captures, typename Ctr>
	Opt<CtrOutput> attempt_match(ComputationTree &tree) {
		// TODO microoptimization for RVO?
		CtrOutput o(num_captures); // initialize to nullptrs
		if (Ctr::match(tree, o)) {
			return o;
		} else {
			return {};
		}
	}

	void tiletest(Program &program) {
		// get a basic block to play with

		BasicBlock &block = *program.get_l3_functions()[0]->get_blocks()[7];

		Vec<Uptr<ComputationTree>> computation_trees;
		for (const Uptr<Instruction> &inst : block.get_raw_instructions()) {
			computation_trees.push_back(mkuptr<ComputationTree>(inst->to_computation_tree()));
		}
		for (const Uptr<ComputationTree> &tree : computation_trees) {
			std::cout << program::to_string(*tree) << std::endl;
			if (Opt<CtrOutput> maybe_match = attempt_match<2, MoveCtr<VariableCtr<1>, 0>>(*tree)) {
				CtrOutput &match = *maybe_match;
				std::cout << "tile match success!\n";
				std::cout << "0: " << program::to_string(*match[0]) << "\n";
				std::cout << "1: " << program::to_string(*match[1]) << "\n";
			}
			// Output o;
			// MoveCtr<Output, 0, VariableCtr<Output, 1>>::match(*tree, o);
		}

		std::cout << "yay" << std::endl;
	}
}
