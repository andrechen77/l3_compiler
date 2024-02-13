#include "tiles.h"
#include "std_alias.h"
#include "target_arch.h"
#include "utils.h"
#include <iostream>
#include <algorithm>

namespace L3::code_gen::tiles {
	// TODO add more tiles for CISC instructions

	using namespace L3::program;

	/*
	struct MyTile {
		using Structre = DestCtr<
			MoveCtr<
				BinaryCtr<
					VariableCtr,
					AnyCtr
				>
			>
		>;
	}
	*/

	struct MatchFailError {};
	// the following are functions throwing MatchFailError used for matching
	// purposes
	template<typename NodeType>
	const NodeType &unwrap_node_type(const ComputationNode &node) {
		const NodeType *downcasted = dynamic_cast<const NodeType *>(&node);
		if (downcasted) {
			return *downcasted;
		} else {
			throw MatchFailError {};
		}
	}
	void throw_unless(bool success) {
		if (!success) {
			throw MatchFailError {};
		}
	}

	template<typename... CnSubclasses>
	bool is_dynamic_type(const ComputationNode &s) {
		return (dynamic_cast<const CnSubclasses *>(&s) || ...);
	}

	// "CTR" stands for "computation tree rule", and is kind of like a pegtl
	// parsing rule but instead of matching characters it matches parts of
	// a computation tree.

	// FUTURE perhaps each CTR should just be a function template, instead of
	// a struct template with a static `match` method. This would remove one
	// level of indentation and allow us to specify function signatures in the
	// template parameters, so you get slightly more helpful error messages
	// when you write an incorrect rule.

	namespace rules {
		// "Matches" describes what kind of node will be matched by the rule.
		// "Captures" describes the elements of the resulting struct that are
		// taken from the target. this does not include any children CTRs that
		// are passed in the template parameters.

		// Matches: a NoOpCn or an atomic computation node
		struct NoOpCtr {
			static NoOpCtr match(const ComputationNode &target) {
				throw_unless(is_dynamic_type<
					NoOpCn, VariableCn, FunctionCn, NumberCn, LabelCn
				>(target));
				return {};
			}
		};

		// Matches: any node
		// Captures: the node matched
		// This rule allows tiles to capture parts of the tree that they
		// don't technically match, so that they can return those parts of the
		// tree to be matched by other tiles
		struct AnyCtr {
			const ComputationNode *node;

			static AnyCtr match(const ComputationNode &target) {
				return { &target };
			}
		};

		// TODO inexplicableS and inexplicableT should probably return their own
		// variants so that it doesn't seem like Uptr<ComputationNode> is a
		// possibility

		// Matches: A computation node that can be expressed as an "T"
		// in L2 (i.e. a variable or number literal)
		// Captures: the matched node
		struct InexplicableTCtr {
			const ComputationNode *node;

			static InexplicableTCtr match(const ComputationNode &target) {
				throw_unless(target.destination.has_value() || is_dynamic_type<NumberCn>(target));
				return { &target };
			}
		};

		// Matches: A computation node that can be expressed as an "S"
		// in L2 (i.e. a variable, number literal, label, or function name)
		// Captures: the matched node
		struct InexplicableSCtr {
			const ComputationNode *node;

			static InexplicableSCtr match(const ComputationNode &target) {
				throw_unless(target.destination.has_value()
					|| is_dynamic_type<NumberCn, LabelCn, FunctionCn>(target));
				return { &target };
			}
		};

		// Matches: A computation node that can be expressed as a variable
		// in L2 and satisfies the passed-in CTR.
		// Captures: the destination variable
		template<typename NodeCtr>
		struct VariableCtr {
			Variable *var;
			NodeCtr node;

			static VariableCtr match(const ComputationNode &target) {
				throw_unless(target.destination.has_value());
				return { *target.destination, NodeCtr::match(target) };
			}
		};

		// Matches: a MoveCn
		template<typename SourceCtr>
		struct MoveCtr {
			SourceCtr source;

			static MoveCtr match(const ComputationNode &target) {
				const MoveCn &move_node = unwrap_node_type<MoveCn>(target);
				return { SourceCtr::match(*move_node.source) };
			}
		};

		// Matches: a BinaryCn
		// Captures: the Operator used
		template<typename LhsCtr, typename RhsCtr>
		struct BinaryCtr {
			Operator op;
			LhsCtr lhs;
			RhsCtr rhs;

			static BinaryCtr match(const ComputationNode &target) {
				const BinaryCn &bin_node = unwrap_node_type<BinaryCn>(target);
				return {
					bin_node.op,
					LhsCtr::match(*bin_node.lhs),
					RhsCtr::match(*bin_node.rhs)
				};
			}
		};
	}

	// To be used for matching, a Tile subclass must have:
	// - member type Structure where Structure::match(const ComputationTree &) works (can throw)
	// - a constructor that takes a Structure and returns a Tile (can throw)
	// - static member int cost
	// - static member int munch

	namespace tile_patterns {
		using namespace rules;
		using L3::code_gen::target_arch::to_l2_expr;

		struct NoOp : Tile {
			using Structure = NoOpCtr;
			NoOp(Structure s) {}

			static const int munch = 0;
			static const int cost = 0;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return {};
			}
		};

		struct PureAssignment : Tile {
			Variable *dest;
			const ComputationNode *source;

			using Structure = VariableCtr<MoveCtr<InexplicableSCtr>>;
			PureAssignment(Structure s) :
				dest { s.var },
				source { s.node.source.node }
			{}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					to_l2_expr(this->dest) + " <- " + to_l2_expr(*this->source)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->source };
			}
		};

		struct BinaryArithmeticAssignment : Tile {
			Variable *dest;
			Operator op;
			const ComputationNode *lhs;
			const ComputationNode *rhs;

			using Structure = VariableCtr<
				BinaryCtr<
					InexplicableTCtr,
					InexplicableTCtr
				>
			>;
			BinaryArithmeticAssignment(Structure s) :
				dest { s.var },
				op { s.node.op },
				lhs { s.node.lhs.node },
				rhs { s.node.rhs.node }
			{
				throw_unless(
					this->op == Operator::plus
					|| this->op == Operator::minus
					|| this->op == Operator::times
					|| this->op == Operator::bitwise_and
					|| this->op == Operator::lshift
					|| this->op == Operator::rshift
				);
			}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					"%_ <- " + to_l2_expr(*this->lhs),
					"%_ " + program::to_string(this->op) + "= " + to_l2_expr(*this->rhs),
					to_l2_expr(this->dest) + " <- %_"
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->lhs, this->rhs };
			}
		};

		struct BinaryCompareAssignment : Tile {
			Variable *dest;
			Operator op;
			const ComputationNode *lhs;
			const ComputationNode *rhs;

			using Structure = VariableCtr<
				BinaryCtr<
					InexplicableTCtr,
					InexplicableTCtr
				>
			>;
			BinaryCompareAssignment(Structure s) :
				dest { s.var },
				op { s.node.op },
				lhs { s.node.lhs.node },
				rhs { s.node.rhs.node }
			{
				throw_unless(
					this->op == Operator::lt
					|| this->op == Operator::le
					|| this->op == Operator::eq
					|| this->op == Operator::ge
					|| this->op == Operator::gt
				);
			}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				// if we use gt or ge, mirror the operator and swap the operands
				const ComputationNode *lhs_ptr = this->lhs;
				const ComputationNode *rhs_ptr = this->rhs;
				Operator l2_op = this->op;
				switch (this->op) {
					case Operator::gt:
						l2_op = Operator::lt;
						lhs_ptr = this->rhs;
						rhs_ptr = this->lhs;
						break;
					case Operator::ge:
						l2_op = Operator::le;
						lhs_ptr = this->rhs;
						rhs_ptr = this->lhs;
						break;
					// default cause is to do nothing
				}

				return {
					to_l2_expr(this->dest) + " <- "
					+ to_l2_expr(*lhs_ptr) + " "
					+ program::to_string(l2_op) + " "
					+ to_l2_expr(*rhs_ptr)
				};
			}
			virtual Vec<const L3::program::ComputationNode *> get_unmatched() const override {
				return { this->lhs, this->rhs };
			}
		};
	}

	namespace tp = tile_patterns;

	template<typename TP>
	Opt<Uptr<TP>> attempt_tile_match(const ComputationNode &target) {
		try {
			return mkuptr<TP>(TP::Structure::match(target));
		} catch (MatchFailError &e) {
			return {};
		}
	}

	template<typename TP>
	void attempt_tile_match(const ComputationNode &tree, Opt<Uptr<Tile>> &out, int &best_munch, int &best_cost) {
		if (TP::munch > best_munch || (TP::munch == best_munch && TP::cost <= best_cost)) {
			Opt<Uptr<TP>> result = attempt_tile_match<TP>(tree);
			if (result) {
				out = mv(*result);
				best_munch = TP::munch;
				best_cost = TP::cost;
			}
		}
	}
	template<typename... TPs>
	void attempt_tile_matches(const ComputationNode &tree, Opt<Uptr<Tile>> &out, int &best_munch, int &best_cost) {
		(attempt_tile_match<TPs>(tree, out, best_munch, best_cost), ...);
	}

	Opt<Uptr<Tile>> find_best_tile(const ComputationNode &tree) {
		Opt<Uptr<Tile>> best_match;
		int best_munch = 0;
		int best_cost = 0;
		attempt_tile_matches<
			tp::NoOp,
			tp::PureAssignment,
			tp::BinaryArithmeticAssignment,
			tp::BinaryCompareAssignment
		>(tree, best_match, best_munch, best_cost);
		return best_match;
	}

	Vec<Uptr<Tile>> tile_trees(const Vec<ComputationTreeBox> &tree_boxes) {
		// build a stack to hold pointers to the currently untiled trees.
		// the top of the stack is for trees that must be executed later
		Vec<Uptr<Tile>> tiles; // stored in REVERSE order of execution
		Vec<const ComputationNode *> untiled_trees;
		for (const ComputationTreeBox &tree_box : tree_boxes) {
			untiled_trees.push_back(tree_box.get_tree().get());
		}
		while (!untiled_trees.empty()) {
			// try to tile the top tree
			const ComputationNode *top_tree = untiled_trees.back();
			untiled_trees.pop_back();
			Opt<Uptr<Tile>> best_match = find_best_tile(*top_tree);
			if (!best_match) {
				std::cerr << "Couldn't find a tile for this tree! " << program::to_string(*top_tree) << "\n";
				// TODO reject if you can't find a tile
				continue;
				// exit(1);
			}
			for (const ComputationNode *unmatched: (*best_match)->get_unmatched()) {
				untiled_trees.push_back(unmatched);
			}
			tiles.push_back(mv(*best_match));
		}
		std::reverse(tiles.begin(), tiles.end()); // now stored in FORWARD order
		return tiles;
	}
}
