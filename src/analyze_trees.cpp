#include "analyze_trees.h"
#include "std_alias.h"

namespace L3::program::analyze {
	using namespace std_alias;

	void generate_data_flow(L3Function &l3_function) {
		Vec<Uptr<BasicBlock>> &basic_blocks = l3_function.get_blocks();

		// generate computation trees for this block
		for (Uptr<BasicBlock> &block : basic_blocks) {
			block->generate_computation_trees();
		}

		// update the in and out sets for all the blocks until a fixed point
		// is reached
		bool sets_changed;
		do {
			sets_changed = false;
			for (Uptr<BasicBlock> &block : basic_blocks) {
				if (block->update_in_out_sets()) {
					sets_changed = true;
				}
			}
		} while (sets_changed);
	}

	// basically take the completed program and generate computation trees
	// for each instruction, then update all the basic blocks to have proper
	// in and out sets
	void generate_data_flow(Program &program) {
		for (Uptr<L3Function> &l3_function : program.get_l3_functions()) {
			generate_data_flow(*l3_function);
		}
	}

	// TODO
	// assumes that data flow has already been generated
	// merges trees whenever possible
	// probably the way to do this is to start at the last tree, keeping a
	// running map of variables to the most recently encountered tree that
	// reads from that variable and could merge on it. if you encounter
	// an eligible candidate that writes to a variable, then merge those trees
	void merge_trees(BasicBlock &block) {

	}

	// TODO
	// assumes that data flow has already been generated for the program;
	// merges trees in all the basic blocks
	void merge_trees(Program &program) {
		std::cerr << "merging happens here\n";
	}
}