#pragma once

#include "std_alias.h"
#include <string>
#include <string_view>
#include <iostream>
#include <typeinfo>

namespace L3::program {
	using namespace std_alias;

	template<typename Item> class ItemRef;
	class Variable;
	class InstructionLabel;
	class L3Function;
	class ExternalFunction;
	class NumberLiteral;
	class MemoryLocation;
	class BinaryOperation;
	class FunctionCall;

	// interface
	class ExprVisitor {
		public:
		virtual void visit(ItemRef<Variable> &expr) = 0;
		virtual void visit(ItemRef<InstructionLabel> &expr) = 0;
		virtual void visit(ItemRef<L3Function> &expr) = 0;
		virtual void visit(ItemRef<ExternalFunction> &expr) = 0;
		virtual void visit(NumberLiteral &expr) = 0;
		virtual void visit(MemoryLocation &expr) = 0;
		virtual void visit(BinaryOperation &expr) = 0;
		virtual void visit(FunctionCall &expr) = 0;
	};

	struct AggregateScope;

	// interface
	class Expr {
		public:
		// virtual Set<Variable *> get_vars_on_read() const { return {}; }
		// virtual Set<Variable *> get_vars_on_write(bool get_read_vars) const { return {}; }
		virtual void bind_to_scope(AggregateScope &agg_scope) = 0;
		virtual std::string to_string() const = 0;
		virtual void accept(ExprVisitor &v) = 0;
	};

	// instantiations must implement the virtual methods
	template<typename Item>
	class ItemRef : public Expr {
		std::string free_name; // the name originally given to the variable
		Item *referent_nullable;

		public:

		ItemRef(std::string free_name) :
			free_name { mv(free_name) },
			referent_nullable { nullptr }
		{}
		// ItemRef(Item *referent);

		// virtual Set<Item *> get_vars_on_read() const override;
		// virtual Set<Item *> get_vars_on_write(bool get_read_vars) const override;
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		void bind(Item *referent) {
			this->referent_nullable = referent;
		}
		Opt<Item *> get_referent() const {
			if (this->referent_nullable) {
				return this->referent_nullable;
			} else {
				return {};
			}
		}
		const std::string &get_ref_name() const {
			if (this->referent_nullable) {
				return this->referent_nullable->get_name();
			} else {
				return this->free_name;
			}
		}
		virtual std::string to_string() const override;
		virtual void accept(ExprVisitor &v) override { v.visit(*this); }
	};

	class NumberLiteral : public Expr {
		int64_t value;

		public:

		NumberLiteral(int64_t value) : value { value } {}
		NumberLiteral(std::string_view value_str);

		int64_t get_value() const { return this->value; }
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual std::string to_string() const override;
		virtual void accept(ExprVisitor &v) override { v.visit(*this); }
	};

	class Variable;

	class MemoryLocation : public Expr {
		Uptr<ItemRef<Variable>> base;

		public:

		MemoryLocation(Uptr<ItemRef<Variable>> &&base) : base { mv(base) } {}

		// virtual Set<Variable *> get_vars_on_read() const override;
		// virtual Set<Variable *> get_vars_on_write(bool get_read_vars) const override;
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual std::string to_string() const override;
		virtual void accept(ExprVisitor &v) override {v.visit(*this); }
	};

	enum struct Operator {
		lt,
		le,
		eq,
		ge,
		gt,
		plus,
		minus,
		times,
		bitwise_and,
		lshift,
		rshift
	};
	Operator str_to_op(std::string_view str);
	std::string to_string(Operator op);

	class BinaryOperation : public Expr {
		Uptr<Expr> lhs;
		Uptr<Expr> rhs;
		Operator op;

		public:

		BinaryOperation(Uptr<Expr> &&lhs, Uptr<Expr> &&rhs, Operator op) :
			lhs { mv(lhs) },
			rhs { mv(rhs) },
			op { op }
		{}

		// virtual Set<Variable *> get_vars_on_read() const override;
		// virtual Set<Variable *> get_vars_on_write(bool get_read_vars) const override;
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual std::string to_string() const override;
		virtual void accept(ExprVisitor &v) override;
	};

	class FunctionCall : public Expr {
		Uptr<Expr> callee;
		Vec<Uptr<Expr>> arguments;

		public:

		FunctionCall(Uptr<Expr> &&callee, Vec<Uptr<Expr>> &&arguments) :
			callee { mv(callee) }, arguments { mv(arguments) }
		{}

		// virtual Set<Variable *> get_vars_on_read() const override;
		// virtual Set<Variable *> get_vars_on_write(bool get_read_vars) const override;
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual std::string to_string() const override;
		virtual void accept(ExprVisitor &v) override;
	};

	class InstructionReturn;
	class InstructionAssignment;
	class InstructionLabel;
	class InstructionBranch;

	// interface
	class InstructionVisitor {
		public:
		virtual void visit(InstructionReturn &inst) = 0;
		virtual void visit(InstructionAssignment &inst) = 0;
		virtual void visit(InstructionLabel &inst) = 0;
		virtual void visit(InstructionBranch &inst) = 0;
	};

	// interface
	class Instruction {
		public:
		virtual void bind_to_scope(AggregateScope &agg_scope) = 0;
		virtual bool get_moves_control_flow() const = 0;
		virtual std::string to_string() const = 0;
		virtual void accept(InstructionVisitor &v) = 0;
	};

	class InstructionReturn : public Instruction {
		Opt<Uptr<Expr>> return_value;

		public:

		InstructionReturn(Uptr<Expr> &&return_value) : return_value { mv(return_value) } {}

		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual bool get_moves_control_flow() const { return true; };
		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
	};

	class InstructionAssignment : public Instruction {
		// the destination is optional only for the pure call instruction
		Opt<Uptr<Expr>> maybe_dest;
		Uptr<Expr> source;

		public:

		InstructionAssignment(Uptr<Expr> &&expr) : source { mv(expr) } {}
		InstructionAssignment(Uptr<Expr> &&destination, Uptr<Expr> &&source) :
			maybe_dest { Opt(mv(destination)) }, source { mv(source) }
		{}

		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual bool get_moves_control_flow() const override;
		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
	};

	class InstructionLabel : public Instruction {
		std::string label_name;

		public:

		InstructionLabel(std::string label_name) : label_name { mv(label_name) } {}

		const std::string &get_name() const { return this->label_name; }
		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual bool get_moves_control_flow() const { return false; };
		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
	};

	class InstructionBranch : public Instruction {
		Opt<Uptr<Expr>> condition;
		Uptr<ItemRef<InstructionLabel>> label; // should this be a pointer to a BasicBlock instead?

		public:

		InstructionBranch(Uptr<ItemRef<InstructionLabel>> &&label, Uptr<Expr> &&condition) :
			condition { mv(condition) }, label { mv(label) }
		{}
		InstructionBranch(Uptr<ItemRef<InstructionLabel>> &&label) :
			condition {}, label { mv(label) }
		{}

		virtual void bind_to_scope(AggregateScope &agg_scope) override;
		virtual bool get_moves_control_flow() const { return true; };
		virtual std::string to_string() const override;
		virtual void accept(InstructionVisitor &v) override { v.visit(*this); }
	};

	struct BasicBlock {
		Vec<Uptr<Instruction>> instructions;
		// sequential instructions
		// always ends with a call, branch, or return
		// labels can only appear in the beginning

		Vec<BasicBlock *> succ_blocks; // meaningless for now; ignore

		/* public:

		BasicBlock();

		Vec<Uptr<Instruction>> &get_instructions() { return this->instructions; }

		const Vec<BasicBlock *> &get_succ_blocks() const { return this->succ_blocks; } */
	};

	// A Scope represents a namespace of Items that the ItemRefs care about.
	// A Scope does not own any of the Items it maps to.
	// `(name, item)` pairs in this->dict represent Items defined in this scope
	// under `name`.
	// `(name, ItemRef *)` in free_referrers represents that that ItemRef has
	// refers to `name`, but that it is a free name (unbound to anything in this
	// scope)
	template<typename Item>
	class Scope {
		// If a Scope has a parent, then it cannot have any
		// free_refs; they must have been transferred to the parent.
		Opt<Scope *> parent;
		Map<std::string, Item *> dict;
		Map<std::string, Vec<ItemRef<Item> *>> free_refs;

		public:

		Scope() : parent {}, dict {}, free_refs {} {}

		Vec<Item *> get_all_items() const {
			Vec<Item *> result;
			if (this->parent) {
				result = mv(static_cast<const Scope *>(*this->parent)->get_all_items());
			}
			for (const auto &[name, item] : this->dict) {
				result.push_back(item);
			}
			return result;
		}

		/* std::vector<Item> get_all_items() {
			std::vector<Item> result;
			if (this->parent) {
				result = std::move((*this->parent)->get_all_items());
			}
			for (auto &[name, item] : this->dict) {
				result.push_back(item);
			}
			return result;
		} */

		// returns whether the ref was immediately bound or was left as free
		bool add_ref(ItemRef<Item> &item_ref) {
			std::string_view ref_name = item_ref.get_ref_name();

			Opt<Item *> maybe_item = this->get_item_maybe(ref_name);
			if (maybe_item) {
				// bind the ref to the item
				item_ref.bind(*maybe_item);
				return true;
			} else {
				// there is no definition of this name in the current scope
				this->push_free_ref(item_ref);
				return false;
			}
		}

		// Adds the specified item to this scope under the specified name,
		// resolving all free refs who were depending on that name. Dies if
		// there already exists an item under that name.
		void resolve_item(std::string name, Item *item) {
			auto existing_item_it = this->dict.find(name);
			if (existing_item_it != this->dict.end()) {
				std::cerr << "name conflict: " << name << std::endl;
				exit(-1);
			}

			const auto [item_it, _] = this->dict.insert(std::make_pair(name, item));
			auto free_refs_vec_it = this->free_refs.find(name);
			if (free_refs_vec_it != this->free_refs.end()) {
				for (ItemRef<Item> *item_ref_ptr : free_refs_vec_it->second) {
					item_ref_ptr->bind(item_it->second);
				}
				this->free_refs.erase(free_refs_vec_it);
			}
		}

		// In addition to using free names like normal, clients may also use
		// this method to define an Item at the same time that it is used.
		// (kinda like python variable declaration).
		// The below conditional inclusion trick doesn't work because
		// gcc-toolset-11 doesn't seem to respect SFINAE, so just allow all
		// instantiation sto use it and hope for the best.
		// template<typename T = std::enable_if_t<DefineOnUse>>
		/* Item get_item_or_create(const std::string_view &name) {
			std::optional<Item *> maybe_item_ptr = get_item_maybe(name);
			if (maybe_item_ptr) {
				return *maybe_item_ptr;
			} else {
				const auto [item_it, _] = this->dict.insert(std::make_pair(
					std::string(name),
					Item(name)
				));
				return &item_it->second;
			}
		} */

		std::optional<Item *> get_item_maybe(std::string_view name) {
			auto item_it = this->dict.find(name);
			if (item_it != this->dict.end()) {
				return std::make_optional<Item *>(item_it->second);
			} else {
				if (this->parent) {
					return (*this->parent)->get_item_maybe(name);
				} else {
					return {};
				}
			}
		}

		/* void remove_item(Item *item) {
			for (auto it = this->dict.begin(); it != this->dict.end(); ++it) {
				if (&it->second == item) {
					dict.erase(it);
					break;
				}
			}
		} */

		// Sets the given Scope as the parent of this Scope, transferring all
		// current and future free names to the parent. If this scope already
		// has a parent, dies.
		void set_parent(Scope &parent) {
			if (this->parent) {
				std::cerr << "this scope already has a parent oops\n";
				exit(1);
			}

			this->parent = std::make_optional<Scope *>(&parent);

			for (auto &[name, our_free_refs_vec] : this->free_refs) {
				for (ItemRef<Item> *our_free_ref : our_free_refs_vec) {
					// TODO optimization here is possible; instead of using the
					// public API of the parent we can just query the dictionary
					// directly
					(*this->parent)->add_ref(*our_free_ref);
				}
			}
			this->free_refs.clear();
		}

		// returns whether free refs exist in this scope for the given name
		Vec<ItemRef<Item> *> get_free_refs() const {
			std::vector<ItemRef<Item> *> result;
			for (auto &[name, free_refs_vec] : this->free_refs) {
				result.insert(result.end(), free_refs_vec.begin(), free_refs_vec.end());
			}
			return result;
		}

		// returns the free names exist in this scope
		Vec<std::string> get_free_names() const {
			Vec<std::string> result;
			for (auto &[name, free_refs_vec] : this->free_refs) {
				result.push_back(name);
			}
			return result;
		}

		// // binds all free names to the given item
		/* void fake_bind_frees(Item *item_ptr) {
			for (auto &[name, free_refs_vec] : this->free_refs) {
				for (ItemRef *item_ref_ptr : free_refs_vec) {
					// TODO we should be allowed to print this
					// std::cerr << "fake-bound free name: " << item_ref_ptr->get_ref_name() << "\n";
					item_ref_ptr->bind(item_ptr);
				}
			}
			this->free_refs.clear();
		} */

		private:

		// Given an item_ref, exposes it as a ref with a free name. This may
		// be caught by the parent Scope and resolved, or the parent might
		// also expose it as a free ref recursively.
		void push_free_ref(ItemRef<Item> &item_ref) {
			std::string_view ref_name = item_ref.get_ref_name();
			if (this->parent) {
				(*this->parent)->add_ref(item_ref);
			} else {
				this->free_refs[std::string(ref_name)].push_back(&item_ref);
			}
		}
	};

	class L3Function;
	class ExternalFunction;

	struct AggregateScope {
		Scope<Variable> variable_scope;
		Scope<InstructionLabel> label_scope;
		Scope<L3Function> l3_function_scope;
		Scope<ExternalFunction> external_function_scope;

		void set_parent(AggregateScope &parent);
	};

	class Variable {
		std::string name;

		public:

		Variable(std::string name) : name { mv(name) } {}

		const std::string &get_name() const { return this->name; }
		std::string to_string() const;
	};

	// interface
	class Function {
		public:
		virtual const std::string &get_name() const = 0;
		virtual bool verify_argument_num(int num) const = 0;
		// virtual bool get_never_returns() const = 0;
		virtual std::string to_string() const = 0;
	};

	class L3Function : public Function {
		std::string name;
		Vec<Uptr<BasicBlock>> blocks;
		Vec<Uptr<Variable>> vars;
		Vec<Variable *> parameter_vars;
		AggregateScope agg_scope;

		explicit L3Function(
			std::string name,
			Vec<Uptr<BasicBlock>> &&blocks,
			Vec<Uptr<Variable>> &&vars,
			Vec<Variable *> parameter_vars
		) :
			name { mv(name) },
			blocks { mv(blocks) },
			vars { mv(vars) },
			parameter_vars { mv(parameter_vars) }
		{}

		public:

		virtual const std::string &get_name() const override { return this->name; }
		virtual bool verify_argument_num(int num) const override;
		// virtual bool get_never_returns() const override;
		virtual std::string to_string() const override;

		class Builder {
			std::string name;
			Vec<Uptr<BasicBlock>> blocks;
			Vec<Variable *> parameter_vars;

			AggregateScope agg_scope;
			Scope<BasicBlock> block_scope;

			Uptr<BasicBlock> current_block;
			// whether it's possible for the last block in the blocks list to
			// have execution fall through to the current block
			bool last_block_falls_through;

			public:
			Builder();
			Pair<Uptr<L3Function>, AggregateScope> get_result();
			void add_name(std::string name);
			void add_next_instruction(Uptr<Instruction> &&inst);
			// TODO add parameter vars

			private:
			void store_current_block();
		};
	};

	class ExternalFunction : public Function {
		std::string name;
		Vec<int> valid_num_arguments;

		public:

		virtual const std::string &get_name() const override { return this->name; }
		virtual bool verify_argument_num(int num) const override;
		// virtual bool get_never_returns() const override;
		virtual std::string to_string() const override;
	};

	class Program {
		Vec<Uptr<L3Function>> l3_functions;
		Vec<Uptr<ExternalFunction>> external_functions;
		Uptr<ItemRef<L3Function>> main_function_ref;

		explicit Program(
			Vec<Uptr<L3Function>> &&l3_functions,
			Vec<Uptr<ExternalFunction>> &&external_functions,
			Uptr<ItemRef<L3Function>> &&main_function_ref
		) :
			l3_functions { mv(l3_functions) },
			external_functions { mv(external_functions) },
			main_function_ref { mv(main_function_ref) }
		{}

		public:

		std::string to_string() const;

		class Builder {
			Vec<Uptr<L3Function>> l3_functions;
			Uptr<ItemRef<L3Function>> main_function_ref;
			Vec<Uptr<ExternalFunction>> external_functions;
			AggregateScope agg_scope;

			public:
			Builder();
			Uptr<Program> get_result();
			void add_l3_function(Uptr<L3Function> &&function, AggregateScope &fun_scope);
		};
	};
}