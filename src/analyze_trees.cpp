#include "analyze_trees.h"
#include "std_alias.h"

namespace L3::program {
	using namespace std_alias;

	void BasicBlock::generate_computation_trees() {
		// generate the computation trees
		for (const Uptr<Instruction> &inst : this->raw_instructions) {
			this->tree_boxes.push_back(mkuptr<ComputationTreeBox>(*inst));
		}

		// generate the gen and kill set
		// the algorithm starts at the end of the block
		VarLiveness &l = this->var_liveness;
		for (auto it = this->tree_boxes.rbegin(); it != this->tree_boxes.rend(); ++it) {
			l.kill_set += (*it)->get_variables_written();
			l.gen_set -= (*it)->get_variables_written();
			l.gen_set += (*it)->get_variables_read();
		}

		// set the initial value of the in and out sets to satisfy the liveness equations
		l.in_set = l.gen_set;
		l.out_set = {}; // should already be empty but just in case
	}
	bool BasicBlock::update_in_out_sets() {
		VarLiveness &l = this->var_liveness;
		bool sets_changed = false;

		// out[i] = UNION (s in successors(i)) {in[s]}
		Set<Variable *> new_out_set;
		for (BasicBlock *succ : this->succ_blocks) {
			new_out_set += succ->var_liveness.in_set;
		}
		if (l.out_set != new_out_set) {
			sets_changed = true;
			l.out_set = mv(new_out_set);
		}

		// in[i] = gen[i] UNION (out[i] MINUS kill[i])
		// Everything currently in in[i] is either there because it was
		// in gen[i] or because it's in out[i]. out[i] is the only thing
		// that might have changed. Assuming that this equation is
		// upheld in the previous iteration, these operations will
		// satisfy the formula again.
		// - remove all existing elements that are not in out[i] nor gen[i]
		// - add all elements that are in out[i] but not kill[i]
		Set<Variable *> new_in_set;
		for (Variable *var : l.in_set) {
			if (l.out_set.find(var) != l.out_set.end()
				|| l.gen_set.find(var) != l.gen_set.end())
			{
				new_in_set.insert(var);
			}
		}
		for (Variable *var : l.out_set) {
			if (l.kill_set.find(var) == l.kill_set.end()) {
				new_in_set.insert(var);
			}
		}
		if (l.in_set != new_in_set) {
			sets_changed = true;
			l.in_set = mv(new_in_set);
		}

		return sets_changed;
	}
}

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