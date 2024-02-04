#include "program.h"
#include "std_alias.h"
#include "utils.h"
#include <assert.h>
#include <map>

namespace L3::program {
	using namespace std_alias;

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
	template<> std::string ItemRef<InstructionLabel>::to_string() const {
		std::string result = ":" + this->get_ref_name();
		if (!this->referent_nullable) {
			result += "?";
		}
		return result;
	}
	template<> void ItemRef<InstructionLabel>::bind_to_scope(AggregateScope &agg_scope) {
		agg_scope.label_scope.add_ref(*this);
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

	NumberLiteral::NumberLiteral(std::string_view value_str) :
		value { utils::string_view_to_int<int64_t>(value_str) }
	{}
	void NumberLiteral::bind_to_scope(AggregateScope &agg_scope) {
		// empty bc literals make no reference to names
	}
	std::string NumberLiteral::to_string() const {
		return std::to_string(this->value);
	}

	void MemoryLocation::bind_to_scope(AggregateScope &agg_scope) {
		this->base->bind_to_scope(agg_scope);
	}
	std::string MemoryLocation::to_string() const {
		return "mem " + this->base->to_string();
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
	bool InstructionAssignment::get_moves_control_flow() const {
		// TODO
		return false;
	}
	std::string InstructionAssignment::to_string() const {
		std::string result;
		if (this->maybe_dest) {
			result += (*this->maybe_dest)->to_string() + " <- ";
		}
		result += this->source->to_string();
		return result;
	}

	void InstructionLabel::bind_to_scope(AggregateScope &agg_scope) {
		agg_scope.label_scope.resolve_item(this->label_name, this);
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
	std::string InstructionBranch::to_string() const {
		std::string result = "br ";
		if (this->condition) {
			result += (*this->condition)->to_string() + " ";
		}
		result += this->label->to_string();
		return result;
	}

	void AggregateScope::set_parent(AggregateScope &parent) {
		this->variable_scope.set_parent(parent.variable_scope);
		this->label_scope.set_parent(parent.label_scope);
		this->l3_function_scope.set_parent(parent.l3_function_scope);
		this->external_function_scope.set_parent(parent.external_function_scope);
	}

	std::string Variable::to_string() const {
		return this->name;
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
			for (const Uptr<Instruction> &inst : block->instructions) {
				result += "\t" + inst->to_string() + "\n";
			}
		}
		result += "}";
		return result;
	}
	L3Function::Builder::Builder() :
		// default-construct everything else
		current_block { mkuptr<BasicBlock>() },
		last_block_falls_through { false }
	{}
	Pair<Uptr<L3Function>, AggregateScope> L3Function::Builder::get_result() {
		// store the current block
		if (!this->current_block->instructions.empty()) {
			this->store_current_block();
		}

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
				mv(this->blocks),
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
		/* if (this->last_block_falls_through) {
			this->last_block_falls_through = false;
			this->blocks.back()->succ_blocks.push_back((*this->current_block).get());
		} */
		inst->bind_to_scope(this->agg_scope);
		if (InstructionLabel *inst_label = dynamic_cast<InstructionLabel *>(inst.get()); inst_label) {
			this->store_current_block();
		}
		bool moves_control_flow = inst->get_moves_control_flow();
		this->current_block->instructions.push_back(mv(inst));
		if (moves_control_flow) {
			this->store_current_block();
		}
		// TODO handle chaining basic blocks together
	}
	void L3Function::Builder::add_parameter(std::string var_name) {
		Uptr<Variable> var_ptr = mkuptr<Variable>(var_name);
		this->agg_scope.variable_scope.resolve_item(mv(var_name), var_ptr.get());
		this->parameter_vars.push_back(var_ptr.get());
		this->vars.emplace_back(mv(var_ptr));
	}
	void L3Function::Builder::store_current_block() {
		this->blocks.push_back(mv(this->current_block));
		this->current_block = mkuptr<BasicBlock>();
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
		return "[function std::" + this->name + "]";
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
		// TODO add external functions
		// Vec<Uptr<ExternalFunction>> external_functions;
		/* for (std::string name : this->agg_scope.external_function_scope.get_free_names()) {
			Uptr<Variable> var_ptr = mkuptr<Variable>(name);
			this->agg_scope.variable_scope.resolve_item(mv(name), var_ptr.get());
			vars.emplace_back(mv(var_ptr));
		} */
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
}
