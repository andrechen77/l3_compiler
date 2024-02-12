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
	template<typename T, typename Variant>
	const T &unwrap_variant(const Variant &variant) {
		const T *result = std::get_if<T>(&variant);
		if (result) {
			return *result;
		} else {
			throw MatchFailError {};
		}
	}
	const Uptr<ComputationNode> &unwrap_node(const ComputationTree &tree) {
		return unwrap_variant<Uptr<ComputationNode>>(tree);
	}
	template<typename NodeType>
	const NodeType &unwrap_node_type(const Uptr<ComputationNode> &node) {
		NodeType *downcasted = dynamic_cast<NodeType *>(node.get());
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

	// "CTR" stands for "computation tree rule", and is kind of like a pegtl
	// parsing rule but instead of matching characters it matches parts of
	// a computation tree.

	// FUTURE perhaps each CTR should just be a function template, instead of
	// a struct template with a static `match` method. This would remove one
	// level of indentation and allow us to specify function signatures in the
	// template parameters, so you get slightly more helpful error messages
	// when you write an incorrect rule.

	namespace rules {
		struct VariableCtr {
			Variable *var;

			static VariableCtr match(const ComputationTree &target) {
				return { unwrap_variant<Variable *>(target) };
			}
		};

		struct InexplicableSCtr {
			ComputationTree value;

			static InexplicableSCtr match(const ComputationTree &target) {
				if (Variable *const *x = std::get_if<Variable *>(&target)) {
					return { *x };
				} else if (BasicBlock *const *x = std::get_if<BasicBlock *>(&target)) {
					return { *x };
				} else if (Function *const *x = std::get_if<Function *>(&target)) {
					return { *x };
				} else if (const int64_t *x = std::get_if<int64_t>(&target)) {
					return { *x };
				} else {
					throw MatchFailError {};
				}
			}
		};

		// Matches: a ComputationNode variant of ComputationTree if there is a destination variable
		// Captures: the Variable * in the node's destination field
		template<typename NodeCtr>
		struct DestCtr {
			Variable *dest;
			NodeCtr node;

			static DestCtr match(const ComputationTree &target) {
				const Uptr<ComputationNode> &node = unwrap_node(target);
				throw_unless(node->destination.has_value());
				return DestCtr { *node->destination, NodeCtr::match(target) };
			}
		};

		// Matches: a MoveComputation variant of ComputationTree
		// Captures: nothing
		template<typename SourceCtr>
		struct MoveCtr {
			SourceCtr source;

			static MoveCtr match(const ComputationTree &target) {
				const MoveComputation &move_node = unwrap_node_type<MoveComputation>(unwrap_node(target));
				return { SourceCtr::match(move_node.source) };
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

		struct MyTile : Tile {
			Variable *dest;
			ComputationTree source;

			using Structure = DestCtr<MoveCtr<InexplicableSCtr>>;
			MyTile(Structure s) :
				dest { s.dest },
				source { mv(s.node.source.value) }
			{}

			static const int munch = 1;
			static const int cost = 1;

			virtual Vec<std::string> to_l2_instructions() const override {
				return {
					to_l2_expr(this->dest) + " <- " + to_l2_expr(this->source)
				};
			}
			virtual Vec<L3::program::ComputationTree *> get_unmatched() const override {
				return {};
			}
		};
	}

	namespace tp = tile_patterns;

	template<typename TP>
	Opt<Uptr<TP>> attempt_tile_match(const ComputationTree &target) {
		try {
			return mkuptr<TP>(TP::Structure::match(target));
		} catch (MatchFailError &e) {
			return {};
		}
	}

	template<typename TP>
	void attempt_tile_match(const ComputationTree &tree, Opt<Uptr<Tile>> &out, int best_munch, int cost) {
		if (TP::munch > best_munch || (TP::munch == best_munch && TP::cost < cost)) {
			Opt<Uptr<TP>> result = attempt_tile_match<TP>(tree);
			if (result) {
				out = mv(*result);
			}
		}
	}
	template<typename... TPs>
	void attempt_tile_matches(const ComputationTree &tree, Opt<Uptr<Tile>> &out, int best_munch, int cost) {
		(attempt_tile_match<TPs>(tree, out, best_munch, cost), ...);
	}

	Opt<Uptr<Tile>> find_best_tile(const ComputationTree &tree) {
		Opt<Uptr<Tile>> best_match;
		attempt_tile_matches<
			tp::MyTile
		>(tree, best_match, 0, 0);
		return best_match;
	}

	Vec<Uptr<Tile>> tile_trees(Vec<Uptr<ComputationTree>> &trees) {
		// build a stack to hold pointers to the currently untiled trees.
		// the top of the stack is for trees that must be executed later
		Vec<Uptr<Tile>> tiles; // stored in REVERSE order of execution
		Vec<ComputationTree *> untiled_trees;
		for (const Uptr<ComputationTree> &tree : trees) {
			untiled_trees.push_back(tree.get());
		}
		while (!untiled_trees.empty()) {
			// try to tile the top tree
			ComputationTree *top_tree = untiled_trees.back();
			untiled_trees.pop_back();
			Opt<Uptr<Tile>> best_match = find_best_tile(*top_tree);
			if (!best_match) {
				std::cerr << "Couldn't find a tile for this tree! " << program::to_string(*top_tree) << "\n";
				// TODO reject if you can't find a tile
				continue;
				// exit(1);
			}
			for (ComputationTree *unmatched: (*best_match)->get_unmatched()) {
				untiled_trees.push_back(unmatched);
			}
			tiles.push_back(mv(*best_match));
		}
		std::reverse(tiles.begin(), tiles.end()); // now stored in FORWARD order
		return tiles;
	}
}
