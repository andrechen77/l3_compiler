#include "program.h"
#include "std_alias.h"
#include "utils.h"
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

	ComparisonOperator str_to_cmp_op(std::string_view str) {
		static const Map<std::string, ComparisonOperator> map {
			{ "<", ComparisonOperator::lt },
			{ "<=", ComparisonOperator::le },
			{ "=", ComparisonOperator::eq },
			{ ">=", ComparisonOperator::ge },
			{ ">", ComparisonOperator::gt }
		};
		return map.find(str)->second;
	}
	std::string to_string(ComparisonOperator op) {
		static const std::string map[] = {
			"<", "<=", "=", ">=", ">"
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

	std::string InstructionReturn::to_string() const {
		std::string result = "return";
		if (this->return_value) {
			result += "" + (*this->return_value)->to_string();
		}
		return result;
	}

	std::string InstructionAssignment::to_string() const {
		std::string result;
		if (this->maybe_dest) {
			result += (*this->maybe_dest)->to_string() + " <- ";
		}
		result += this->source->to_string();
		return result;
	}

	std::string InstructionLabel::to_string() const {
		return ":" + this->label_name;
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
	/* L3Function::Builder::Builder() :
		// default-construct everything else
		last_block_falls_through { false }
	{}
	class BuilderHelper : public InstructionVisitor {
		// TODO you were here
		BuilderHelper() {
			L3Function::Builder b;
			b.vars;
		}
	};
	void L3Function::Builder::add_next_instruction(Uptr<Instruction> &instruction) {
		if (!this->current_block) {
			this->current_block.emplace();
		}
		if (this->last_block_falls_through) {
			this->last_block_falls_through = false;
			this->blocks.back()->succ_blocks.push_back((*this->current_block).get());
		}
		instruction->bind_to_scope(this->agg_scope);
		// TODO do something special based on the type of instruction
		// instructions with call or return/branch instructions wrap up the block
		// label instructions create a new block
		(*this->current_block)->instructions.push_back(mv(instruction));
	} */

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
