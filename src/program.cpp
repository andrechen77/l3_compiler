#include "program.h"
#include "std_alias.h"
#include "utils.h"
#include <assert.h>
#include <map>
#include <optional>
#include <algorithm>

namespace L3::program {
	using namespace std_alias;

	Set<Variable *> pick_variables(ComputationTree &cmpTree){
		if (Variable **var = std::get_if<Variable *>(&cmpTree)) {
			return {*var};
		}
		else if (Uptr<ComputationNode> *var = std::get_if<Uptr<ComputationNode>>(&cmpTree)){
			return {(*var)->get_var_source()};
		}
		else {
			return {};
		}
	}

	template<> std::string ItemRef<Variable>::to_string() const {
		std::string result = "%" + this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}
	template<> void ItemRef<Variable>::bind_to_scope(AggregateScope &agg_scope) {
		agg_scope.variable_scope.add_ref(*this);
	}
	template<> ComputationTree ItemRef<Variable>::to_computation_tree() const {
		if (!this->referent_nullable) {
			std::cerr << "Error: can't convert free variable name to computation tree.\n";
			exit(1);
		}
		return this->referent_nullable;
	}
	template<> std::string ItemRef<BasicBlock>::to_string() const {
		std::string result = ":" + this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}
	template<> void ItemRef<BasicBlock>::bind_to_scope(AggregateScope &agg_scope) {
		agg_scope.label_scope.add_ref(*this);
	}
	template<> ComputationTree ItemRef<BasicBlock>::to_computation_tree() const {
		if (!this->referent_nullable) {
			std::cerr << "Error: can't convert free label name to computation tree.\n";
			exit(1);
		}
		return this->referent_nullable;
	}
	template<> std::string ItemRef<L3Function>::to_string() const {
		std::string result = "@" + this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}
	template<> void ItemRef<L3Function>::bind_to_scope(AggregateScope &agg_scope) {
		agg_scope.l3_function_scope.add_ref(*this);
	}
	template<> ComputationTree ItemRef<L3Function>::to_computation_tree() const {
		if (!this->referent_nullable) {
			std::cerr << "Error: can't convert free L3 function name to computation tree.\n";
			exit(1);
		}
		return this->referent_nullable;
	}
	template<> std::string ItemRef<ExternalFunction>::to_string() const {
		std::string result = this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}
	template<> void ItemRef<ExternalFunction>::bind_to_scope(AggregateScope &agg_scope) {
		agg_scope.external_function_scope.add_ref(*this);
	}
	template<> ComputationTree ItemRef<ExternalFunction>::to_computation_tree() const {
		if (!this->referent_nullable) {
			std::cerr << "Error: can't convert free external function name to computation tree.\n";
			exit(1);
		}
		return this->referent_nullable;
	}

	NumberLiteral::NumberLiteral(std::string_view value_str) :
		value { utils::string_view_to_int<int64_t>(value_str) }
	{}
	void NumberLiteral::bind_to_scope(AggregateScope &agg_scope) {
		// empty bc literals make no reference to names
	}
	ComputationTree NumberLiteral::to_computation_tree() const {
		return this->value;
	}
	std::string NumberLiteral::to_string() const {
		return std::to_string(this->value);
	}

	void MemoryLocation::bind_to_scope(AggregateScope &agg_scope) {
		this->base->bind_to_scope(agg_scope);
	}
	ComputationTree MemoryLocation::to_computation_tree() const {
		return mkuptr<LoadComputation>(
			Opt<Variable *>(),
			this->base->to_computation_tree()
		);
	}
	std::string MemoryLocation::to_string() const {
		return "load " + this->base->to_string();
	}

	Operator str_to_op(std::string_view str) {
		static const Map<std::string, Operator> map {
			{ "<", Operator::lt },
			{ "<=", Operator::le },
			{ "=", Operator::eq },
			{ ">=", Operator::ge },
			{ ">", Operator::gt },
			{ "+", Operator::plus },
			{ "-", Operator::minus },
			{ "*", Operator::times },
			{ "&", Operator::bitwise_and },
			{ "<<", Operator::lshift },
			{ ">>", Operator::rshift }
		};
		return map.find(str)->second;
	}
	std::string to_string(Operator op) {
		static const std::string map[] = {
			"<", "<=", "=", ">=", ">", "+", "-", "*", "&", "<<", ">>"
		};
		return map[static_cast<int>(op)];
	}

	void BinaryOperation::bind_to_scope(AggregateScope &agg_scope) {
		this->lhs->bind_to_scope(agg_scope);
		this->rhs->bind_to_scope(agg_scope);
	}
	ComputationTree BinaryOperation::to_computation_tree() const {
		return mkuptr<BinaryComputation>(
			Opt<Variable *>(),
			this->op,
			mv(this->lhs->to_computation_tree()),
			mv(this->rhs->to_computation_tree())
		);
	}
	std::string BinaryOperation::to_string() const {
		return this->lhs->to_string()
			+ " " + program::to_string(this->op)
			+ " " + this->rhs->to_string();
	}

	void FunctionCall::bind_to_scope(AggregateScope &agg_scope) {
		this->callee->bind_to_scope(agg_scope);
		for (Uptr<Expr> &arg : this->arguments) {
			arg->bind_to_scope(agg_scope);
		}
	}
	ComputationTree FunctionCall::to_computation_tree() const {
		Vec<ComputationTree> arguments;
		for (const Uptr<Expr> &argument : this->arguments) {
			arguments.emplace_back(argument->to_computation_tree());
		}
		return mkuptr<CallComputation>(
			Opt<Variable *>(),
			mv(this->callee->to_computation_tree()),
			mv(arguments)
		);
	}
	std::string FunctionCall::to_string() const {
		std::string result = "call " + this->callee->to_string() + "(";
		for (const Uptr<Expr> &argument : this->arguments) {
			result += argument->to_string() + ", ";
		}
		result += ")";
		return result;
	}

	void InstructionReturn::bind_to_scope(AggregateScope &agg_scope) {
		if (this->return_value) {
			(*this->return_value)->bind_to_scope(agg_scope);
		}
	}
	Uptr<ComputationNode> InstructionReturn::to_computation_tree() const {
		if (this->return_value) {
			return mkuptr<ReturnComputation>(
				(*this->return_value)->to_computation_tree()
			);
		} else {
			return mkuptr<ReturnComputation>();
		}
	}
	std::string InstructionReturn::to_string() const {
		std::string result = "return";
		if (this->return_value) {
			result += " " + (*this->return_value)->to_string();
		}
		return result;
	}

	void InstructionAssignment::bind_to_scope(AggregateScope &agg_scope) {
		if (this->maybe_dest) {
			(*this->maybe_dest)->bind_to_scope(agg_scope);
		}
		this->source->bind_to_scope(agg_scope);
	}
	Uptr<ComputationNode> InstructionAssignment::to_computation_tree() const {
		ComputationTree tree = this->source->to_computation_tree();
		// put a destination on the top node; or make a MoveComputation if
		// the tree is actually a leaf
		if (Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&tree)) {
			if (this->maybe_dest) {
				(*node)->destination = (*this->maybe_dest)->get_referent().value(); // .value() to assert that the destination variable must be bound if it exists
			} // if there is no destination, just leave it blank
			return mv(*node);
		} else {
			// make a MoveComputation
			return mkuptr<MoveComputation>(
				(*this->maybe_dest)->get_referent().value(), // .value() to assert that there must be a destination; otherwise what's the point (all possibility of side effects was handled in the branch checking if the tree was a ComputationNode)
				mv(tree)
			);
		}
	}
	Instruction::ControlFlowResult InstructionAssignment::get_control_flow() const {
		// FUTURE only the source value can have a call
		// this works for now but will fail if we have more complex
		// subexpressions such as calls inside other expressions.
		// thankfully I don't think we will run into that problem
		bool has_call = dynamic_cast<FunctionCall *>(this->source.get());
		return { true, has_call, Opt<ItemRef<BasicBlock> *>() };
	}
	std::string InstructionAssignment::to_string() const {
		std::string result;
		if (this->maybe_dest) {
			result += (*this->maybe_dest)->to_string() + " <- ";
		}
		result += this->source->to_string();
		return result;
	}

	void InstructionStore::bind_to_scope(AggregateScope &agg_scope) {
		this->base->bind_to_scope(agg_scope);
		this->source->bind_to_scope(agg_scope);
	}
	Uptr<ComputationNode> InstructionStore::to_computation_tree() const {
		return mkuptr<StoreComputation>(
			this->base->to_computation_tree(),
			this->source->to_computation_tree()
		);
	}
	Instruction::ControlFlowResult InstructionStore::get_control_flow() const {
		// FUTURE the grammar prohibits a store instruction from having any kind
		// of source expression other than a variable, so we know for sure that
		// there cannot be a call or anything. For now, simply returning false
		// will work
		return { true, false, Opt<ItemRef<BasicBlock> *>() };
	}
	std::string InstructionStore::to_string() const {
		return "store " + this->base->to_string() + " <- " + this->source->to_string();
	}

	void InstructionLabel::bind_to_scope(AggregateScope &agg_scope) {}
	Uptr<ComputationNode> InstructionLabel::to_computation_tree() const {
		// InstructionLabels don't do anything, so output a no-op tree
		return mkuptr<NoOpComputation>();
	}
	std::string InstructionLabel::to_string() const {
		return ":" + this->label_name;
	}

	void InstructionBranch::bind_to_scope(AggregateScope &agg_scope) {
		if (this->condition) {
			(*this->condition)->bind_to_scope(agg_scope);
		}
		this->label->bind_to_scope(agg_scope);
	}
	Instruction::ControlFlowResult InstructionBranch::get_control_flow() const {
		return {
			this->condition.has_value(), // a conditional branch might fall through
			false, // a branch instruction gets no promise of return
			this->label.get()
		};
	}
	Uptr<ComputationNode> InstructionBranch::to_computation_tree() const {
		Opt<ComputationTree> condition_tree;
		if (this->condition) {
			condition_tree = (*this->condition)->to_computation_tree();
		}
		return mkuptr<BranchComputation>(
			this->label->get_referent().value(),
			mv(condition_tree)
		);
	}
	std::string InstructionBranch::to_string() const {
		std::string result = "br ";
		if (this->condition) {
			result += (*this->condition)->to_string() + " ";
		}
		result += this->label->to_string();
		return result;
	}

	Vec<ComputationTree *> get_merge_target(ComputationTree &tree, Variable *target) {
		if (Variable **var = std::get_if<Variable *>(&tree)) {
			if (*var == target){
				return { &tree };
			}
		} else if (Uptr<ComputationNode> *var = std::get_if<Uptr<ComputationNode>>(&tree)) {
			return (*var)->get_merge_target(target);
		}
		return {};
	}
	std::string to_string(const ComputationTree &tree) {
		if (Variable *const *variable = std::get_if<Variable *>(&tree)) {
			return program::to_string(*variable);
		} else if (BasicBlock *const *block = std::get_if<BasicBlock *>(&tree)) {
			return program::to_string(*block);
		} else if (Function *const *function = std::get_if<Function *>(&tree)) {
			return program::to_string(*function);
		} else if (const int64_t *value = std::get_if<int64_t>(&tree)) {
			return std::to_string(*value);
		} else if (const Uptr<ComputationNode> *node = std::get_if<Uptr<ComputationNode>>(&tree)) {
			return (*node)->to_string();
		} else {
			std::cout << "Error: I don't know how to convert this type of tree into a string.\n";
			exit(1);
		}
	}

	std::string ComputationNode::to_string() const {
		return "CT Node ("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") {}";
	}
	Opt<Variable *> ComputationNode::get_var_dest() {
		return this->destination;
	}
	Vec<ComputationTree *> get_merge_target(Variable *target) {
		return {};
	}
	std::string NoOpComputation::to_string() const {
		return "CT NoOp ("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") {}";
	}
	Set<Variable *> NoOpComputation::get_var_source() {
		return {};
	}
	Vec<ComputationTree *> NoOpComputation::get_merge_target(Variable *) {
		return {};
	}
	std::string MoveComputation::to_string() const {
		return "CT Move ("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") { source: "
			+ program::to_string(source)
			+ " }";
	}
	Set<Variable*> MoveComputation::get_var_source(){
		return pick_variables(this->source);
	}
	Vec<ComputationTree *> MoveComputation::get_merge_target(Variable *target) {
		return program::get_merge_target(this->source, target);
	}
	std::string BinaryComputation::to_string() const {
		return "CT Binary ("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") { op: "
			+ program::to_string(this->op)
			+ ", lhs: "
			+ program::to_string(this->lhs)
			+ ", rhs: "
			+ program::to_string(this->rhs)
			+ " }";
	}
	Set<Variable *> BinaryComputation::get_var_source(){
		Set<Variable *> sol;
		sol.merge(pick_variables(this->lhs));
		sol.merge(pick_variables(this->rhs));
		return sol;
	}
	Vec<ComputationTree *> BinaryComputation::get_merge_target(Variable *target){
		Vec<ComputationTree *> sol = program::get_merge_target(this->lhs, target);
		Vec<ComputationTree *> rhs_sol = program::get_merge_target(this->rhs, target);
		sol += rhs_sol;
		return sol;
	}
	std::string CallComputation::to_string() const {
		std::string result = "CT Call ("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") { function: "
			+ program::to_string(this->function)
			+ ", args: [";
		for (const ComputationTree &tree : this->arguments) {
			result += program::to_string(tree) + ", ";
		}
		result += "] }";
		return result;
	}
	Set<Variable*> CallComputation::get_var_source(){
		Set<Variable *> sol;
		for (ComputationTree &cmptree: arguments) {
			sol.merge(pick_variables(cmptree));
		}
		return sol;
	}
	Vec<ComputationTree*> CallComputation::get_merge_target(Variable *target) {
		Vec<ComputationTree *> sol;
		for (ComputationTree &cmptree: this->arguments) {
			sol += program::get_merge_target(cmptree, target);
		}
		return sol;
	}
	std::string LoadComputation::to_string() const {
		return "CT Load ("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") { address: "
			+ program::to_string(this->address)
			+ " }";
	}
	Set<Variable*> LoadComputation::get_var_source(){
		return pick_variables(this->address);
	}
	Vec<ComputationTree *> LoadComputation::get_merge_target(Variable *target) {
		return program::get_merge_target(this->address, target);
	}
	std::string StoreComputation::to_string() const {
		return "CT Store ("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") { address: "
			+ program::to_string(this->address)
			+ ", value: "
			+ program::to_string(this->value)
			+ " }";
	}
	Set<Variable*> StoreComputation::get_var_source() {
		Set<Variable *> sol;
		sol.merge(pick_variables(this->address));
		sol.merge(pick_variables(this->value));
		return sol;
	}
	Vec<ComputationTree *> StoreComputation::get_merge_target(Variable *target) {
		Vec<ComputationTree *> sol = program::get_merge_target(this->address, target);
		Vec<ComputationTree *> value_sol = program::get_merge_target(this->value, target);
		sol += value_sol;
		return sol;
	}
	std::string BranchComputation::to_string() const {
		return "CT Branch ("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") { jmp_dest: "
			+ program::to_string(this->jmp_dest)
			+ ", condition: "
			+ utils::to_string<ComputationTree, program::to_string>(this->condition)
			+ " }";
	}
	Set<Variable*> BranchComputation::get_var_source() {
		if (this->condition.has_value()){
			return pick_variables(*this->condition);
		}
		return Set<Variable *>();
	}
	Vec<ComputationTree *> BranchComputation::get_merge_target(Variable *target) {
		if (this->condition.has_value()){
			return program::get_merge_target(*this->condition, target);
		}
		return {};
	}
	std::string ReturnComputation::to_string() const {
		return "CT Return ("
			+ utils::to_string<Variable *, program::to_string>(this->destination)
			+ ") { value: "
			+ utils::to_string<ComputationTree, program::to_string>(this->value)
			+ " }";
	}
	Set<Variable *> ReturnComputation::get_var_source(){
		if (this->value.has_value()){
			return pick_variables(*this->value);
		}
		return Set<Variable *>();
	}
	Vec<ComputationTree *> ReturnComputation::get_merge_target(Variable *target) {
		if (this->value.has_value()){
			return program::get_merge_target(*this->value, target);
		}
		return {};
	}

	ComputationTreeBox::ComputationTreeBox(const Instruction &inst) :
		root_nullable { inst.to_computation_tree() },
		vars_read { this->root_nullable->get_var_source() },
		var_written { this->root_nullable->get_var_dest() }
	{}
	const bool ComputationTreeBox::has_load() const {
		return this->root_nullable->has_load;
	}
	const bool ComputationTreeBox::has_store() const {
		return this->root_nullable->has_store;
	}

	bool ComputationTreeBox::merge(ComputationTreeBox &other) {
		if (!other.get_var_written()) {
			std::cerr << "can't merge these two trees because the child tree has no destination.\n";
			exit(1);
		}
		Variable *var = *other.get_var_written();
		Vec<ComputationTree *> merge_targets = this->root_nullable->get_merge_target(var);
		if (merge_targets.size() != 1) {
			return false;
		}

		if (other.root_nullable->has_load) {
			this->root_nullable->has_load = true;
		}
		if (other.root_nullable->has_store) {
			this->root_nullable->has_store = true;
		}

		ComputationTree *merge_target = merge_targets[0];
		if (MoveComputation *move_other = dynamic_cast<MoveComputation *>(other.root_nullable.get())) {
			*merge_target = mv(move_other->source);
		} else {
			*merge_target = mv(other.root_nullable);
		}
		other.root_nullable = nullptr; // just in case
		return true;
	}

	BasicBlock::BasicBlock() {} // default-initialize everything
	// implementations for BasicBlock::generate_computation_trees and
	// update_in_out_sets are in analyze_trees.cpp
	std::string BasicBlock::to_string() const {
		std::string result = "-----\n";
		result += "in: ";
		for (Variable *var : this->var_liveness.in_set) {
			result += var->get_name() + ", ";
		}
		result += "\nout: ";
		for (Variable *var : this->var_liveness.out_set) {
			result += var->get_name() + ", ";
		}
		result += "\ntrees:\n";
		for (const ComputationTreeBox &tree_box : this->tree_boxes) {
			result += tree_box.get_tree()->to_string() + "\n";
		}
		return result;
	}
	BasicBlock::Builder::Builder() :
		fetus { Uptr<BasicBlock>(new BasicBlock()) },
		succ_block_refs {},
		must_end { false },
		falls_through { true }
	{}
	Uptr<BasicBlock> BasicBlock::Builder::get_result(BasicBlock *successor_nullable) {
		if (this->succ_block_refs.has_value()) {
			Opt<BasicBlock *> another_successor = (*this->succ_block_refs)->get_referent();
			if (another_successor) {
				this->fetus->succ_blocks.push_back(*another_successor);
			} else {
				std::cerr << "Error: control flow goes to unknown label: " << (*this->succ_block_refs)->to_string() << "\n";
				exit(1);
			}
		}
		if (this->falls_through && successor_nullable) {
			this->fetus->succ_blocks.push_back(successor_nullable);
		} // TODO shouldn't it be an error to fall through without a successor? bc we must end with a return?

		return mv(this->fetus);
	}
	Pair<BasicBlock *, Opt<std::string>> BasicBlock::Builder::get_fetus_and_name() {
		return {
			this->fetus.get(),
			this->fetus->get_name().size() > 0 ? this->fetus->get_name() : Opt<std::string>()
		};
	}
	bool BasicBlock::Builder::add_next_instruction(Uptr<Instruction> &&inst) {
		if (this->must_end) {
			return false;
		}

		if (InstructionLabel *inst_label = dynamic_cast<InstructionLabel *>(inst.get())) {
			if (this->fetus->raw_instructions.empty()) {
				this->fetus->name = inst_label->get_name();
			} else {
				return false;
			}
		}
		auto [falls_through, yields_control, jmp_dest] = inst->get_control_flow();
		this->falls_through = falls_through;
		if (!falls_through || yields_control) {
			this->must_end = true;
		}
		if (jmp_dest) {
			this->must_end = true;
			this->succ_block_refs = *jmp_dest;
		}
		this->fetus->raw_instructions.push_back(mv(inst));
		return true;
	}
	std::string to_string(BasicBlock *const &block) {
		return block->get_name();
	}

	void AggregateScope::set_parent(AggregateScope &parent) {
		this->variable_scope.set_parent(parent.variable_scope);
		this->label_scope.set_parent(parent.label_scope);
		this->l3_function_scope.set_parent(parent.l3_function_scope);
		this->external_function_scope.set_parent(parent.external_function_scope);
	}

	std::string Variable::to_string() const {
		return "%" + this->name;
	}

	std::string to_string(Variable *const &variable) {
		return variable->to_string();
	}

	bool L3Function::verify_argument_num(int num) const {
		return num == this->parameter_vars.size();
	}
	std::string L3Function::to_string() const {
		std::string result = "define @" + this->name + "(";
		for (const Variable *var : this->parameter_vars) {
			result += "%" + var->get_name() + ", ";
		}
		result += ") {\n";
		for (const Uptr<BasicBlock> &block : this->blocks) {
			result += block->to_string();
		}
		result += "}";
		return result;
	}
	L3Function::Builder::Builder()
		// default-construct everything
	{
		this->block_builders.emplace_back(); // start with at least one block
	}
	Pair<Uptr<L3Function>, AggregateScope> L3Function::Builder::get_result() {
		// bind all the blocks to the scope
		for (BasicBlock::Builder &builder : this->block_builders) {
			auto [block_ptr, maybe_name] = builder.get_fetus_and_name();
			if (maybe_name) {
				this->agg_scope.label_scope.resolve_item(mv(*maybe_name), block_ptr);
			}
		}

		// at this point, all the BasicBlock::Builders should be completed
		// get everything out of the block builders
		Vec<Uptr<BasicBlock>> blocks;
		BasicBlock *next_block_nullable = nullptr;
		for (auto it = this->block_builders.rbegin(); it != this->block_builders.rend(); ++it) {
			Uptr<BasicBlock> current_block = it->get_result(next_block_nullable);
			BasicBlock *temp = current_block.get();
			blocks.push_back(mv(current_block));
			next_block_nullable = temp;
		}
		std::reverse(blocks.begin(), blocks.end());

		// bind all unbound variables to new variable items
		for (std::string name : this->agg_scope.variable_scope.get_free_names()) {
			Uptr<Variable> var_ptr = mkuptr<Variable>(name);
			this->agg_scope.variable_scope.resolve_item(mv(name), var_ptr.get());
			this->vars.emplace_back(mv(var_ptr));
		}

		// return the result
		return std::make_pair(
			Uptr<L3Function>(new L3Function( // using constructor instead of make_unique because L3Function's private constructor
				mv(this->name),
				mv(blocks),
				mv(this->vars),
				mv(this->parameter_vars)
			)),
			mv(this->agg_scope)
		);
	}
	void L3Function::Builder::add_name(std::string name) {
		this->name = mv(name);
	}
	void L3Function::Builder::add_next_instruction(Uptr<Instruction> &&inst) {
		inst->bind_to_scope(this->agg_scope);
		bool success = this->block_builders.back().add_next_instruction(mv(inst));
		if (!success) {
			// The contract of BasicBlock::Builder stipulates that a failure
			// to add an instruction won't move out of the passed-in pointer,
			// so `inst` is still valid :D
			this->block_builders.emplace_back();
			success = this->block_builders.back().add_next_instruction(mv(inst));
			assert(success);
		}
	}
	void L3Function::Builder::add_parameter(std::string var_name) {
		Uptr<Variable> var_ptr = mkuptr<Variable>(var_name);
		this->agg_scope.variable_scope.resolve_item(mv(var_name), var_ptr.get());
		this->parameter_vars.push_back(var_ptr.get());
		this->vars.emplace_back(mv(var_ptr));
	}

	bool ExternalFunction::verify_argument_num(int num) const {
		for (int valid_num : this->valid_num_arguments) {
			if (num == valid_num) {
				return true;
			}
		}
		return false;
	}
	std::string ExternalFunction::to_string() const {
		return "[[function std::" + this->name + "]]";
	}

	std::string to_string(Function *const &function) {
		return "[[function " + function->get_name() + "]]";
	}

	std::string Program::to_string() const {
		std::string result;
		for (const Uptr<L3Function> &function : this->l3_functions) {
			result += function->to_string() + "\n";
		}
		return result;

	}
	Program::Builder::Builder() :
		// default-construct everything else
		main_function_ref { mkuptr<ItemRef<L3Function>>("main") }
	{
		for (Uptr<ExternalFunction> &function_ptr : generate_std_functions()) {
			this->agg_scope.external_function_scope.resolve_item(
				function_ptr->get_name(),
				function_ptr.get()
			);
			this->external_functions.emplace_back(mv(function_ptr));
		}
		this->agg_scope.l3_function_scope.add_ref(*this->main_function_ref);
	}
	Uptr<Program> Program::Builder::get_result() {
		// TODO verify no free names

		// return the result
		return Uptr<Program>(new Program(
			mv(this->l3_functions),
			mv(this->external_functions),
			mv(this->main_function_ref)
		));
	}
	void Program::Builder::add_l3_function(Uptr<L3Function> &&function, AggregateScope &fun_scope) {
		fun_scope.set_parent(this->agg_scope);
		this->agg_scope.l3_function_scope.resolve_item(function->get_name(), function.get());
		this->l3_functions.push_back(mv(function));
	}

	Vec<Uptr<ExternalFunction>> generate_std_functions() {
		Vec<Uptr<ExternalFunction>> result;
		result.push_back(mkuptr<ExternalFunction>("input", Vec<int> { 0 }));
		result.push_back(mkuptr<ExternalFunction>("print", Vec<int> { 1 }));
		result.push_back(mkuptr<ExternalFunction>("allocate", Vec<int> { 2 }));
		result.push_back(mkuptr<ExternalFunction>("tuple-error", Vec<int> { 3 }));
		result.push_back(mkuptr<ExternalFunction>("tensor-error", Vec<int> { 1, 3, 4 }));
		return result;
	}
}
