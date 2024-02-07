#include "tiles.h"
#include "std_alias.h"
#include "target_arch.h"
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

		// Matches: a NoOpComputation variant of a ComputationTree, or one of the non-Node leaves
		// Captures: nothing
		template<typename CtrOutput>
		struct EmptyCtr {
			static bool match(ComputationTree &target, CtrOutput &o) {
				Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target);
				if (node) {
					if (!dynamic_cast<NoOpComputation *>(node->get())) return false;
					return true;
				} else {
					return true;
				}
			}
		};

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

		// Matches: A Function * variant of ComputationTree
		// Captures: the Function *
		template<typename CtrOutput, int index>
		struct FunctionCtr {
			static bool match(ComputationTree &target, CtrOutput &o) {
				if (Function **ptr = std::get_if<Function *>(&target)) {
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

		// Matches: a ComputationNode variant of ComputationTree if there is a destination variable
		// Captures: the Variable * in the node's destination field
		template<typename CtrOutput, int index, typename NodeCtr>
		struct DestCtr {
			static bool match(ComputationTree &target, CtrOutput &o) {
				Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target);
				if (!node) return false;
				if (!(*node)->destination) return false;
				if (!bind_capture<index>(o, *(*node)->destination)) return false;
				if (!NodeCtr::match(target, o)) return false;
				return true;
			}
		};

		// Matches: a ComputationNode variant of ComputationTree
		// Captures: the Opt<Variable *> in the node's destination field
		template<typename CtrOutput, int index, typename NodeCtr>
		struct MaybeDestCtr {
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

		// Matches: a BinaryComputation variant of ComputationTree with the specified operator
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

		// Matches: a BinaryComputation variant of ComputationTree if it uses an arithmetic operator
		// Captures: the Operator used
		template<typename CtrOutput, int index, typename LhsCtr, typename RhsCtr>
		struct BinaryArithCtr {
			static bool match(ComputationTree &target, CtrOutput &o) {
				Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&target);
				if (!node) return false;
				BinaryComputation *bin_node = dynamic_cast<BinaryComputation *>(node->get());
				if (!bin_node) return false;
				if (!bind_capture<index>(o, bin_node->op)) return false;
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
	// - static member int munch

	namespace tile_patterns {
		using namespace rules;
		using namespace L3::code_gen::target_arch; // TODO just fix the namespaces

		// interface
		struct TilePattern {
			virtual std::string to_l2_instructions() const = 0;
			virtual Vec<ComputationTree *> get_unmatched() const = 0;
		};

		struct NoOp : TilePattern {
			using Captures = std::tuple<>;
			using O = Captures;
			using Rule = EmptyCtr<O>;
			static const int cost = 1;
			static const int munch = 2;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				return "\n";
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};

		struct PureAssignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				MoveCtr<O,
					InexplicableSCtr<O, 1>
				>
			>;
			static const int cost = 1;
			static const int munch = 2;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, src] = this->captures;
				if (!dest || !src) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				return to_l2_expr(*dest) + " <- " + to_l2_expr(*src) + "\n";
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};

		/* struct BinaryPlusAssignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<ComputationTree>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				BinaryCtr<O, program::Operator::plus,
					InexplicableTCtr<O, 1>,
					InexplicableTCtr<O, 2>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, lhs, rhs] = this->captures;
				if (!dest || !lhs || !rhs) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				std::string str = to_l2_expr(*dest) + " <- " + to_l2_expr(*lhs) + "\n";
				str += to_l2_expr(*dest) + " += " + to_l2_expr(*rhs) + "\n";
				return str;
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
		struct BinaryMinusAssignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<ComputationTree>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				BinaryCtr<O, program::Operator::minus,
					InexplicableTCtr<O, 1>,
					InexplicableTCtr<O, 2>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;
			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, lhs, rhs] = this->captures;
				if (!dest || !lhs || !rhs) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				std::string str = to_l2_expr(*dest) + " <- " + to_l2_expr(*lhs) + "\n";
				str += to_l2_expr(*dest) + " -= " + to_l2_expr(*rhs) + "\n";
				return str;
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
		struct BinaryTimesAssignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<ComputationTree>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				BinaryCtr<O, program::Operator::times,
					InexplicableTCtr<O, 1>,
					InexplicableTCtr<O, 2>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, lhs, rhs] = this->captures;
				if (!dest || !lhs || !rhs) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				std::string str = to_l2_expr(*dest) + " <- " + to_l2_expr(*lhs) + "\n";
				str += to_l2_expr(*dest) + " *= " + to_l2_expr(*rhs) + "\n";
				return str;
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
		struct BinaryBitwiseAndAssignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<ComputationTree>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				BinaryCtr<O, program::Operator::bitwise_and,
					InexplicableTCtr<O, 1>,
					InexplicableTCtr<O, 2>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, lhs, rhs] = this->captures;
				if (!dest || !lhs || !rhs) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				std::string str = to_l2_expr(*dest) + " <- " + to_l2_expr(*lhs) + "\n";
				str += to_l2_expr(*dest) + " &= " + to_l2_expr(*rhs) + "\n";
				return str;
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
		struct BinaryShiftRightAssignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<ComputationTree>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				BinaryCtr<O, program::Operator::rshift,
					InexplicableTCtr<O, 1>,
					InexplicableTCtr<O, 2>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, lhs, rhs] = this->captures;
				if (!dest || !lhs || !rhs) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				std::string str = to_l2_expr(*dest) + " <- " + to_l2_expr(*lhs) + "\n";
				str += to_l2_expr(*dest) + " >>= " + to_l2_expr(*rhs) + "\n";
				return str;
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
		struct BinaryShiftLeftAssignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<ComputationTree>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				BinaryCtr<O, program::Operator::lshift,
					InexplicableTCtr<O, 1>,
					InexplicableTCtr<O, 2>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, lhs, rhs] = this->captures;
				if (!dest || !lhs || !rhs) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				std::string str = to_l2_expr(*dest) + " <- " + to_l2_expr(*lhs) + "\n";
				str += to_l2_expr(*dest) + " <<= " + to_l2_expr(*rhs) + "\n";
				return str;
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		}; */
		struct BinaryArithmeticAssignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<Operator>,
				Opt<ComputationTree>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				BinaryArithCtr<O, 1,
					InexplicableTCtr<O, 2>,
					InexplicableTCtr<O, 3>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, op, lhs, rhs] = this->captures;
				if (!dest || !op || !lhs || !rhs) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				std::string str;
				str += "%_ <- " + to_l2_expr(*lhs) + "\n";
				str += "%_ " + program::to_string(*op) + "= " + to_l2_expr(*rhs) + "\n";
				str += to_l2_expr(*dest) + " <- %_";
				return str;
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};

		struct BinaryLessThanAssignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<ComputationTree>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				BinaryCtr<O, program::Operator::lt,
					InexplicableTCtr<O, 1>,
					InexplicableTCtr<O, 2>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, lhs, rhs] = this->captures;
				if (!dest || !lhs || !rhs) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				return "%" + (*dest)->get_name() + " <- " + to_l2_expr(*lhs) + " < " + to_l2_expr(*rhs) + "\n";
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
		struct BinaryLessThanEqualAssignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<ComputationTree>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				BinaryCtr<O, program::Operator::le,
					InexplicableTCtr<O, 1>,
					InexplicableTCtr<O, 2>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, lhs, rhs] = this->captures;
				if (!dest || !lhs || !rhs) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				return "%" + (*dest)->get_name() + " <- " + to_l2_expr(*lhs) + " <= " + to_l2_expr(*rhs) + "\n";
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
		struct BinaryEqualAssignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<ComputationTree>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				BinaryCtr<O, program::Operator::eq,
					InexplicableTCtr<O, 1>,
					InexplicableTCtr<O, 2>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, lhs, rhs] = this->captures;
				if (!dest || !lhs || !rhs) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				return "%" + (*dest)->get_name() + " <- " + to_l2_expr(*lhs) + " = " + to_l2_expr(*rhs) + "\n";
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
		struct BinaryGreaterThanAssignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<ComputationTree>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				BinaryCtr<O, program::Operator::gt,
					InexplicableTCtr<O, 1>,
					InexplicableTCtr<O, 2>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, lhs, rhs] = this->captures;
				if (!dest || !lhs || !rhs) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				return "%" + (*dest)->get_name() + " <- " + to_l2_expr(*rhs) + " < " + to_l2_expr(*lhs) + "\n";
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
		struct BinaryGreaterThanEqualAssignment : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<ComputationTree>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				BinaryCtr<O, program::Operator::ge,
					InexplicableTCtr<O, 1>,
					InexplicableTCtr<O, 2>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, lhs, rhs] = this->captures;
				if (!dest || !lhs || !rhs) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				return "%" + (*dest)->get_name() + " <- " + to_l2_expr(*rhs) + " <= " + to_l2_expr(*lhs) + "\n";
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};

		struct LoadMemory : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<Variable *>
			>;
			using O = Captures;
			using Rule = DestCtr<O, 0,
				LoadCtr<O,
					VariableCtr<O, 1>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, source] = this->captures;
				if (!dest || !source) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				return to_l2_expr(*dest) + " <- " + " mem " + to_l2_expr(*source) + " 0\n";
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};

		struct StoreMemory : TilePattern {
			using Captures = std::tuple<
				Opt<Variable *>,
				Opt<Variable *>
			>;
			using O = Captures;
			using Rule = StoreCtr<O, VariableCtr<O, 0>, VariableCtr<O, 1>>;

			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, source] = this->captures;
				if (!dest || !source) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				return "mem " + to_l2_expr(*dest) + " 0 <- " + to_l2_expr(*source) + "\n";
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};

		struct BranchUnconditional : TilePattern {
			using Captures = std::tuple<
				Opt<BasicBlock *>
			>;
			using O = Captures;
			using Rule = BranchCtr<O, 0, NoneOptCtr<O>>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest] = this->captures;
				if (!dest) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				return "goto " + to_l2_expr(*dest);
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
		struct BranchConditional : TilePattern {
			using Captures = std::tuple<
				Opt<BasicBlock *>,
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = BranchCtr<O, 0,
				SomeOptCtr<O,
					InexplicableTCtr<O, 1>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[dest, condition] = this->captures;
				if (!dest) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				return "cjump 0 < " + to_l2_expr(*condition) + " " + to_l2_expr(*dest);
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
		struct ReturnNothing : TilePattern {
			using Captures = std::tuple<
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = ReturnCtr<O, NoneOptCtr<O>>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				return "return\n";
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
		struct ReturnSomething : TilePattern {
			using Captures = std::tuple<
				Opt<ComputationTree>
			>;
			using O = Captures;
			using Rule = ReturnCtr<O,
				SomeOptCtr<O,
					InexplicableTCtr<O, 0>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[return_val] = this->captures;
				if (!return_val) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				std::string str= "rax <- " + to_l2_expr(*return_val) + "\n";
				str += "return\n";
				return str;
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				return {};
			}
		};

		int num_returns = 0; // number of returns we've seen so far
		// FUTURE ugh please do anything else

		struct CallVal : TilePattern {
			using Captures = std::tuple<
				Opt<Opt<Variable *>>,
				Opt<Function *>,
				Opt<Vec<ComputationTree *>>
			>;
			using O = Captures;
			using Rule = MaybeDestCtr<O, 0,
				CallCtr<O, 2,
					FunctionCtr<O, 1>
				>
			>;
			static const int cost = 1;
			static const int munch = 1;

			Captures captures;

			virtual std::string to_l2_instructions() const override {
				const auto &[maybe_dest, maybe_callee, maybe_args] = this->captures;
				if (!maybe_dest || !maybe_callee || !maybe_args) {
					std::cerr << "Error: attempting to translate incomplete tile.";
					exit(1);
				}
				const Opt<Variable *> dest = *maybe_dest;
				const Function *callee = *maybe_callee;
				const Vec<ComputationTree *> &args = *maybe_args;

				std::string result;
				for (int i = 0; i < args.size(); ++i) {
					result += get_argument_prepping_instruction(to_l2_expr(*args[i]), i) + "\n";
				}
				bool is_l3 = dynamic_cast<const L3Function *>(callee);
				std::string return_label;
				if (is_l3) {
					return_label = ":ret" + std::to_string(num_returns);
					num_returns += 1;
					result += "mem rsp -8 <- " + return_label + "\n";
				}
				result += "call " + to_l2_expr(callee) + " " + std::to_string(args.size()) + "\n";
				if (is_l3) {
					result += return_label + "\n";
				}
				if (dest) {
					result += to_l2_expr(*dest) + " <- rax\n";
				}
				return result;
			}
			virtual Vec<ComputationTree *> get_unmatched() const override {
				const auto &[dest, callee, args] = this->captures;
				if (!args) {
					std::cerr << "Error: incomplete tile.";
					exit(1);
				}
				return *args;
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
	void attempt_tile_match(ComputationTree &tree, Opt<Uptr<tp::TilePattern>> &out, int best_munch, int cost) {
		if (TP::munch > best_munch || (TP::munch == best_munch && TP::cost < cost)) {
			Opt<Uptr<TP>> result = attempt_tile_match<TP>(tree);
			if (result) {
				out = mv(*result);
			}
		}
	}
	template<typename... TPs>
	void attempt_tile_matches(ComputationTree &tree, Opt<Uptr<tp::TilePattern>> &out, int best_munch, int cost) {
		(attempt_tile_match<TPs>(tree, out, best_munch, cost), ...);
	}

	Opt<Uptr<tp::TilePattern>> find_best_tile(ComputationTree &tree) {
		Opt<Uptr<tp::TilePattern>> best_match;
		attempt_tile_matches<
			tp::NoOp,
			tp::PureAssignment,
			// tp::BinaryPlusAssignment,
			// tp::BinaryMinusAssignment,
			// tp::BinaryTimesAssignment,
			// tp::BinaryBitwiseAndAssignment,
			// tp::BinaryShiftLeftAssignment,
			// tp::BinaryShiftRightAssignment,
			tp::BinaryArithmeticAssignment,
			tp::BinaryLessThanAssignment,
			tp::BinaryLessThanEqualAssignment,
			tp::BinaryEqualAssignment,
			tp::BinaryGreaterThanEqualAssignment,
			tp::BinaryGreaterThanAssignment,
			tp::LoadMemory,
			tp::StoreMemory,
			tp::BranchConditional,
			tp::BranchUnconditional,
			tp::ReturnNothing,
			tp::ReturnSomething,
			tp::CallVal
		>(tree, best_match, 0, 0);
		return best_match;
	}

	void tile_trees(Vec<Uptr<ComputationTree>> &trees, std::ostream &o) {
		// TODO actually tile the trees instead of this placeholder
		/* for (const Uptr<ComputationTree> &tree : trees) {
			o << "\t\t - " << program::to_string(*tree) << "\n";

			Opt<Uptr<tp::TilePattern>> best_match = find_best_tile(*tree);
			if (best_match) {
				o << (*best_match)->to_l2_instructions() << "\n";
			} else {
				o << "no match for this one \n";
			}
		}
		std::cerr << "okay real algorithm now \n"; */

		// algorithm starts here
		// build a stack to hold pointers to the currently untiled trees.
		// the top of the stack is for trees that must be executed later
		Vec<Uptr<tp::TilePattern>> tiles; // stored in REVERSE order of execution
		Vec<ComputationTree *> untiled_trees;
		for (const Uptr<ComputationTree> &tree : trees) {
			untiled_trees.push_back(tree.get());
		}
		while (!untiled_trees.empty()) {
			// try to tile the top tree
			ComputationTree *top_tree = untiled_trees.back();
			untiled_trees.pop_back();
			Opt<Uptr<tp::TilePattern>> best_match = find_best_tile(*top_tree);
			if (!best_match) {
				std::cerr << "Couldn't find a tile for this tree!\n";
				continue;
			}
			for (ComputationTree *unmatched: (*best_match)->get_unmatched()) {
				untiled_trees.push_back(unmatched);
			}
			tiles.push_back(mv(*best_match));
		}
		for (auto it = tiles.rbegin(); it != tiles.rend(); ++it) {
			o << (*it)->to_l2_instructions() << "\n";
		}
	}
}
