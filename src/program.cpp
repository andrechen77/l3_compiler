#include "program.h"
#include "std_alias.h"
#include "utils.h"
#include <assert.h>
#include <map>

namespace L3::program {
	using namespace std_alias;

	template<> std::string ItemRef<Variable>::to_string() const {
		return "%" + this->get_ref_name();
	}
	template<> std::string ItemRef<InstructionLabel>::to_string() const {
		return ":" + this->get_ref_name();
	}
	template<> std::string ItemRef<L3Function>::to_string() const {
		return "@" + this->get_ref_name();
	}
	template<> std::string ItemRef<ExternalFunction>::to_string() const {
		return this->get_ref_name();
	}

	NumberLiteral::NumberLiteral(std::string_view value_str) :
		value { utils::string_view_to_int<int64_t>(value_str) }
	{}
	void NumberLiteral::bind_to_scope(AggregateScope &agg_scope) {
		// TODO
	}
	std::string NumberLiteral::to_string() const {
		return std::to_string(this->value);
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

	std::string BinaryOperation::to_string() const {
		return this->lhs->to_string()
			+ " " + program::to_string(this->op)
			+ " " + this->rhs->to_string();
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
		// TODO
	}
	std::string InstructionReturn::to_string() const {
		std::string result = "return";
		if (this->return_value) {
			result += "" + (*this->return_value)->to_string();
		}
		return result;
	}

	void InstructionAssignment::bind_to_scope(AggregateScope &agg_scope) {
		// TODO
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
		// TODO
	}
	std::string InstructionLabel::to_string() const {
		return ":" + this->label_name;
	}

	void InstructionBranch::bind_to_scope(AggregateScope &agg_scope) {
		// TODO
	}
	std::string InstructionBranch::to_string() const {
		std::string result = "br ";
		if (this->condition) {
			result += (*this->condition)->to_string() + " ";
		}
		result += this->label->to_string();
		return result;
	}


	std::string Variable::to_string() const {
		return this->name;
	}

	bool L3Function::verify_argument_num(int num) const {
		return num == this->parameter_vars.size();
	}
	std::string L3Function::to_string() const {
		return "@" + this->name;
	}
	L3Function::Builder::Builder() :
		// default-construct everything else
		current_block { mkuptr<BasicBlock>() },
		last_block_falls_through { false }
	{}
	Pair<L3Function, AggregateScope> L3Function::Builder::get_result() {
		// store the current block
		if (!this->current_block->instructions.empty()) {
			this->store_current_block();
		}

		// bind all unbound variables to new variable items
		Vec<Uptr<Variable>> vars;
		for (std::string name : this->agg_scope.variable_scope.get_free_names()) {
			Uptr<Variable> var_ptr = mkuptr<Variable>(name);
			this->agg_scope.variable_scope.resolve_item(mv(name), var_ptr.get());
			vars.emplace_back(mv(var_ptr));
		}

		// return the result
		return std::make_pair(
			mv(L3Function(
				mv(this->name),
				mv(this->blocks),
				mv(vars),
				mv(this->parameter_vars)
			)),
			mv(this->agg_scope)
		);
	}
	void L3Function::Builder::add_name(std::string name) {
		this->name = mv(name);
	}
	void L3Function::Builder::add_next_instruction(Uptr<Instruction> &inst) {
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
		// TODO
		exit(1);
	}
}
