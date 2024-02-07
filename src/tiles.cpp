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

	// FUTURE perhaps each CTR should just be a function template, instead of
	// a struct template with a static `match` method. This would remove one
	// level of indentation and allow us to specify function signatures in the
	// template parameters, so you get slightly more helpful error messages
	// when you write an incorrect rule.

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

	namespace rules {
		// In each of the CTRs, the children must agree on what the CtrOutput is

		// Matches: A Variable * variant of ComputationTree
		// Captures: the Variable *
		template<typename CtrOutput, int index>
		struct VariableCtr {
			static bool match(ComputationTree &target, CtrOutput &o) {
				if (Variable **ptr = std::get_if<Variable *>(&target)) {
					if (!bind_capture<index>(o, *ptr)) return false;
					return true;
				}
				return false;
			}
		};

		// Matches: A Variable * or int64_t variant of ComputationTree
		// Captures: a copy of the ComputationTree
		template<typename CtrOutput, int index>
		struct InexplicableTCtr {
			static bool match(ComputationTree &target, CtrOutput &o) {
				if (std::holds_alternative<Variable *>(target)
					|| std::holds_alternative<int64_t>(target))
				{
					if (!bind_capture<index>(o, target)) return false;
					return true;
				}
				return false;
			}
		};

		// Matches: A Variable * or BasicBlock * or Function * or int64_t variant of ComputationTree
		// Captures: a copy of the ComputationTree
		template<typename CtrOutput, int index>
		struct InexplicableSCtr {
			static bool match(ComputationTree &target, CtrOutput &o) {
				if (std::holds_alternative<Variable *>(target)
					|| std::holds_alternative<BasicBlock *>(target)
					|| std::holds_alternative<Function *>(target)
					|| std::holds_alternative<int64_t>(target))
				{
					if (!bind_capture<index>(o, target)) return false;
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
				Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target);
				if (!node) return false;
				if (!bind_capture<index>(o, (*node)->destination)) return false;
				if (!NodeCtr::match(target, o)) return false;
				return true;
			}
		};

		// Matches: an empty Opt<ComputationTree>
		// Captures: nothing
		template<typename CtrOutput>
		struct NoneOptCtr {
			static bool match(Opt<ComputationTree> &target, CtrOutput &o) {
				return !target.has_value();
			}
		};

		// Matches: a non-empty Opt<ComputationTree>
		// Captures: nothing
		template<typename CtrOutput, typename InnerCtr>
		struct SomeOptCtr {
			static bool match(Opt<ComputationTree> &target, CtrOutput &o) {
				if (!target.has_value()) return false;
				return InnerCtr::match(*target, o);
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
				Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target);
				if (!node) return false;
				MoveComputation *move_node = dynamic_cast<MoveComputation *>(node->get());
				if (!move_node) return false;
				return SourceCtr::match(move_node->source, o);
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
		template<typename CtrOutput, int index, typename CalleeCtr>
		struct CallCtr {
			static bool match(ComputationTree &target, CtrOutput &o) {
				Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target);
				if (!node) return false;
				CallComputation *call_node = dynamic_cast<CallComputation *>(node->get());
				if (!call_node) return false;

				if (!CalleeCtr::match(call_node->function, o)) return false;

				// create the vector of arguments
				Vec<ComputationTree *> arg_tree_ptrs;
				for (ComputationTree &arg_tree : call_node->arguments) {
					arg_tree_ptrs.push_back(&arg_tree);
				}
				if (!bind_capture<index>(o, mv(arg_tree_ptrs))) return false;

				return true;
			}
		};

		// Matches: a LoadComputation variant of ComputationTree
		// Captures: nothing
		template<typename CtrOutput, typename AddressCtr>
		struct LoadCtr {
			static bool match(ComputationTree &target, CtrOutput &o) {
				Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target);
				if (!node) return false;
				LoadComputation *load_node = dynamic_cast<LoadComputation *>(node->get());
				if (!load_node) return false;
				return AddressCtr::match(load_node->address, o);
			}
		};

		// Matches: a StoreComputation variant of ComputationTree
		// Captures: nothing
		template<typename CtrOutput, typename AddressCtr, typename SourceCtr>
		struct StoreCtr {
			static bool match(ComputationTree &target, CtrOutput &o) {
				Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target);
				if (!node) return false;
				StoreComputation *store_node = dynamic_cast<StoreComputation *>(node->get());
				if (!store_node) return false;
				return AddressCtr::match(store_node->address, o) && SourceCtr::match(store_node->value, o);
			}
		};

		// Matches: a BranchComputation variant of ComputationTree
		// Captures: the BasicBlock * representing jump destination
		template<typename CtrOutput, int index, typename ConditionOptCtr>
		struct BranchCtr {
			static bool match(ComputationTree &target, CtrOutput &o) {
				Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target);
				if (!node) return false;
				BranchComputation *branch_node = dynamic_cast<BranchComputation *>(node->get());
				if (!branch_node) return false;
				if (!bind_capture<index>(o, branch_node->jmp_dest)) return false;
				return ConditionOptCtr::match(branch_node->condition, o);
			}
		};

		// Matches: a ReturnComputation variant of ComputationTree
		// Captures: nothing
		template<typename CtrOutput, typename ValueOptCtr>
		struct ReturnCtr {
			static bool match(ComputationTree &target, CtrOutput &o) {
				Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target);
				if (!node) return false;
				ReturnComputation *return_node = dynamic_cast<ReturnComputation *>(node->get());
				if (!return_node) return false;
				return ValueOptCtr::match(return_node->value, o);
			}
		};
	}

	// A TilePattern is a struct with:
	// - member type Captures describing the capture variables of the tile
	// - member type Rule with Rule::match(ComputationTree &) works
	// - static function std::string to_l2_function(const O &o)
	// - static member int cost

	namespace tile_patterns {
		using namespace rules;

		// interface
		struct TilePattern {
			virtual std::string to_l2_instructions() const = 0;
			virtual Vec<ComputationTree *> get_unmatched() const = 0;
		};

		struct Assignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<Variable *>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				MoveCtr<O,
					VariableCtr<O, 1>
				>
			>;
			static const int cost = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, src] = this->captures;
				if (!dest || !src) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				return "%" + (*dest)->get_name() + " <- %" + (*src)->get_name();
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
	};

	namespace tp = tile_patterns;

	template<typename TP>
	Opt<Uptr<TP>> attempt_tile_match(ComputationTree &tree) {
		Uptr<TP> result = mkuptr<TP>();
		if (TP::Rule::match(tree, result->captures)) {
			return result;
		} else {
			return {};
		}
	}

	template<typename TP>
	void attempt_tile_match(ComputationTree &tree, Vec<Uptr<tp::TilePattern>> &out) {
		Opt<Uptr<TP>> result = attempt_tile_match<TP>(tree);
		if (result) {
			out.push_back(mv(*result));
		}
	}

	template<typename... TPs>
	void attempt_tile_matches(ComputationTree &tree, Vec<Uptr<tp::TilePattern>> &out) {
		(attempt_tile_match<TPs>(tree, out), ...);
	}

	void tile_trees(Vec<Uptr<ComputationTree>> &trees, std::ostream &o) {
		// TODO actually tile the trees instead of this placeholder
		for (const Uptr<ComputationTree> &tree : trees) {
			o << "\t\t - " << program::to_string(*tree) << "\n";

			Vec<Uptr<tp::TilePattern>> matched_tiles;
			attempt_tile_matches<
				tp::Assignment
			>(*tree, matched_tiles);

			for (const Uptr<tp::TilePattern> &tile : matched_tiles) {
				o << "\t\t" << tile->to_l2_instructions() << "\n";
			}
		}
	}
}
