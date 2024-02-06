#include "tiles.h"
#include "std_alias.h"
#include "utils.h"
#include <iostream>

namespace L3::program::tiles {
	/*
	struct MyTile {
		using Ctr = DestCtr<Out,
			0
			MoveCtr<Out,
				BinaryCtr<Out,
					VariableCtr<Out, 0>,
					AnyCtr<Out, 1>
				>
			>
		>;
	}
	*/

	// "CTR" stands for "computation tree rule", and is kind of like a pegtl
	// parsing rule but instead of matching characters it matches parts of
	// a computation tree.

	// A CtrOutput should be an std::tuple of optional elements representing
	// what has been currently matched

	// Attempts to bind the capture at the specified index to the specified
	// value. Always succeeeds if the existing value is none (i.e. that capture
	// variable hasn't been bound yet). If the existing value is something, it
	//  must compare equal to the new value (this is to enforce
	// constraints such as "match this structure but also make sure these two
	// leaves point to the same Variable"). If the index is negative, does not
	// do the capture and returns success. If the index is out of bound, fails.
	// Returns success.
	template<int index, typename CtrOutput, typename Value>
	bool bind_capture(CtrOutput &o, Value &&value) {
		if constexpr (index < 0) {
			return true;
		} else {
			if (std::get<index>(o)) {
				// check if the existing value is equal
				return *std::get<index>(o) == value;
			} else {
				// add the new value
				std::get<index>(o) = mv(value);
				return true;
			}
		}
	}

	// In each of the CTRs, the children must agree on what the CtrOutput is

	// Matches: A Variable * variant of ComputationTree
	// Captures: the Variable *
	template<typename CtrOutput, int index>
	struct VariableCtr {
		static bool match(ComputationTree &target, CtrOutput &o) {
			if (Variable **ptr = std::get_if<Variable *>(&target)) {
				std::cerr << "matched variable!\n";
				if (!bind_capture<index>(o, *ptr)) return false;
				return true;
			}
			return false;
		}
	};

	// Matches: any ComputationNode variant of ComputationTree
	// Captures: the Opt<Variable *> in the node's destination field
	template<typename CtrOutput, int index, typename NodeCtr>
	struct DestCtr {
		static bool match(ComputationTree &target, CtrOutput &o) {
			if (Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target)) {
				if (!bind_capture<index>(o, (*node)->destination)) return false;
				if (!NodeCtr::match(target, o)) return false;
				return true;
			}
			return false;
		}
	};

	// Matches: any ComputationTree
	// Captures: a pointer to that ComputationTree
	// This is used to stop the pattern from matching any deeper along this
	// branch, leaving the rest of the tree to be matched by other tiles.
	template<typename CtrOutput, int index>
	struct AnyCtr {
		static bool match(ComputationTree &target, CtrOutput &o) {
			if (!bind_capture<index>(o, &target)) return false;
			return true;
		}
	};

	// Matches: a MoveComputation variant of ComputationTree
	// Captures: nothing
	template<typename CtrOutput, typename SourceCtr>
	struct MoveCtr {
		static bool match(ComputationTree &target, CtrOutput &o) {
			if (Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target)) {
				if (MoveComputation *move_node = dynamic_cast<MoveComputation *>(node->get())) {
					std::cerr << "matched move tree, checking child\n";
					if (SourceCtr::match(move_node->source, o)) {
						std::cerr << "child worked\n";
						return true;
					} else {
						return false;
					}
				}
			}
			return false;
		}
	};

	// Matches: a BinaryComputation variant of ComputationTree
	// Captures: nothing
	template<typename CtrOutput, Operator op, typename LhsCtr, typename RhsCtr>
	struct BinaryCtr {
		static bool match(ComputationTree &target, CtrOutput &o) {
			Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target);
			if (!node) return false;
			BinaryComputation *bin_node = dynamic_cast<BinaryComputation *>(node->get());
			if (!bin_node) return false;
			if (bin_node->op != op) return false;
			return LhsCtr::match(bin_node->lhs, o) && RhsCtr::match(bin_node->rhs, o);
		}
	};

	// Matches: a CallComputation variant of ComputationTree
	// Captures: a vector with pointers to all the argument ComputationTrees
	template<typename CtrOutput, typename CalleeCtr, int index>
	struct CallCtr {
		static bool match(ComputationTree &target, CtrOutput &o) {
			if (Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target)) {
				if (CallComputation *call_node = dynamic_cast<CallComputation *>(node->get())) {
					if (!CalleeCtr::match(call_node->function, o)) return false;
					// create the vector of arguments
					Vec<ComputationTree *> arg_tree_ptrs;
					for (ComputationTree &arg_tree : call_node->arguments) {
						arg_tree_ptrs.push_back(&arg_tree);
					}
					if (!bind_capture<index>(o, mv(arg_tree_ptrs))) return false;
					return true;
				}
			}
			return false;
		}
	};

	template<typename CtrOutput, typename Ctr>
	Opt<CtrOutput> attempt_match(ComputationTree &tree) {
		// TODO microoptimization for RVO?
		CtrOutput o;
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
			using O = std::tuple<Opt<Opt<Variable *>>, Opt<ComputationTree *>, Opt<ComputationTree *>>;
			using Pattern = DestCtr<O, 0, BinaryCtr<O, Operator::plus, AnyCtr<O, 1>, AnyCtr<O, 2>>>;
			if (Opt<O> maybe_match = attempt_match<O, Pattern>(*tree)) {
				O &match = *maybe_match;
				std::cout << "tile match success!\n";
				std::cout << "0: " << program::to_string(**std::get<0>(match)) << "\n";
				std::cout << "1: " << program::to_string(**std::get<1>(match)) << "\n";
				std::cout << "2: " << program::to_string(**std::get<2>(match)) << "\n";
			}
		}

		std::cout << "yay" << std::endl;
	}
}
