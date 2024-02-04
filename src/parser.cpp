#include "std_alias.h"
#include "parser.h"
#include <typeinfo>
#include <sched.h>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <set>
#include <iterator>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <stdint.h>
#include <assert.h>
#include <fstream>

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/analyze.hpp>
#include <tao/pegtl/contrib/raw_string.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include <tao/pegtl/contrib/parse_tree_to_dot.hpp>

namespace pegtl = TAO_PEGTL_NAMESPACE;

namespace L3::parser {
	using namespace std_alias;

	namespace rules {
		// for convenience of reading the rules
		using namespace pegtl;

		// for convenience of adding whitespace
		template<typename Result, typename Separator, typename...Rules>
		struct interleaved_impl;
		template<typename... Results, typename Separator, typename Rule0, typename... RulesRest>
		struct interleaved_impl<seq<Results...>, Separator, Rule0, RulesRest...> :
			interleaved_impl<seq<Results..., Rule0, Separator>, Separator, RulesRest...>
		{};
		template<typename... Results, typename Separator, typename Rule0>
		struct interleaved_impl<seq<Results...>, Separator, Rule0> {
			using type = seq<Results..., Rule0>;
		};
		template<typename Separator, typename... Rules>
		using interleaved = typename interleaved_impl<seq<>, Separator, Rules...>::type;

		struct CommentRule :
			disable<
				TAO_PEGTL_STRING("//"),
				until<eolf>
			>
		{};

		struct SpaceRule :
			sor<one<' '>, one<'\t'>>
		{};

		struct SpacesRule :
			star<SpaceRule>
		{};

		struct LineSeparatorsRule :
			star<seq<SpacesRule, eol>>
		{};

		struct LineSeparatorsWithCommentsRule :
			star<
				seq<
					SpacesRule,
					sor<eol, CommentRule>
				>
			>
		{};

		struct NameRule :
			ascii::identifier // the rules for L3 names are the same as for C identifiers
		{};

		struct VariableRule :
			seq<one<'%'>, NameRule>
		{};

		struct LabelRule :
			seq<one<':'>, NameRule>
		{};

		// aka. "l" in the grammar
		struct L3FunctionNameRule :
			seq<one<'@'>, NameRule>
		{};

		struct NumberRule :
			sor<
				seq<
					opt<sor<one<'-'>, one<'+'>>>,
					range<'1', '9'>,
					star<digit>
				>,
				one<'0'>
			>
		{};

		struct ComparisonOperatorRule :
			sor<
				TAO_PEGTL_STRING("<"),
				TAO_PEGTL_STRING("<="),
				TAO_PEGTL_STRING("="),
				TAO_PEGTL_STRING(">="),
				TAO_PEGTL_STRING(">")
			>
		{};

		struct ArithmeticOperatorRule :
			sor<
				TAO_PEGTL_STRING("+"),
				TAO_PEGTL_STRING("-"),
				TAO_PEGTL_STRING("*"),
				TAO_PEGTL_STRING("&"),
				TAO_PEGTL_STRING("<<"),
				TAO_PEGTL_STRING(">>")
			>
		{};

		struct InexplicableURule :
			sor<
				VariableRule,
				L3FunctionNameRule
			>
		{};

		struct InexplicableTRule :
			sor<
				VariableRule,
				NumberRule
			>
		{};

		struct InexplicableSRule :
			sor<
				InexplicableTRule,
				LabelRule,
				L3FunctionNameRule
			>
		{};

		struct CallArgsRule :
			opt<list<
				InexplicableTRule,
				one<','>,
				SpaceRule
			>>
		{};

		struct DefArgsRule :
			opt<list<
				VariableRule,
				one<','>,
				SpaceRule
			>>
		{};

		struct StdFunctionNameRule :
			sor<
				TAO_PEGTL_STRING("print"),
				TAO_PEGTL_STRING("allocate"),
				TAO_PEGTL_STRING("input"),
				TAO_PEGTL_STRING("tuple-error"),
				TAO_PEGTL_STRING("tensor-error")
			>
		{};

		struct CalleeRule :
			sor<
				InexplicableURule,
				StdFunctionNameRule
			>
		{};

		struct ArrowRule : TAO_PEGTL_STRING("\x3c-") {};

		struct InstructionPureAssignmentRule :
			interleaved<
				SpacesRule,
				VariableRule,
				ArrowRule,
				InexplicableSRule
			>
		{};

		struct InstructionOpAssignmentRule :
			interleaved<
				SpacesRule,
				VariableRule,
				ArrowRule,
				InexplicableTRule,
				ArithmeticOperatorRule,
				InexplicableTRule
			>
		{};

		struct InstructionCompareAssignmentRule :
			interleaved<
				SpacesRule,
				VariableRule,
				ArrowRule,
				InexplicableTRule,
				ComparisonOperatorRule,
				InexplicableTRule
			>
		{};

		struct InstructionLoadAssignmentRule :
			interleaved<
				SpacesRule,
				VariableRule,
				ArrowRule,
				TAO_PEGTL_STRING("load"),
				VariableRule
			>
		{};

		struct InstructionStoreAssignmentRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("store"),
				VariableRule,
				ArrowRule,
				VariableRule
			>
		{};

		struct InstructionReturnRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("return"),
				opt<InexplicableTRule>
			>
		{};

		struct InstructionLabelRule :
			interleaved<
				SpacesRule,
				LabelRule
			>
		{};

		struct InstructionBranchUncondRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("br"),
				LabelRule
			>
		{};

		struct InstructionBranchCondRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("br"),
				InexplicableTRule,
				LabelRule
			>
		{};

		struct FunctionCallRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("call"),
				CalleeRule,
				one<'('>,
				CallArgsRule,
				one<')'>
			>
		{};

		struct InstructionCallVoidRule :
			interleaved<
				SpacesRule,
				FunctionCallRule
			>
		{};

		struct InstructionCallValRule :
			interleaved<
				SpacesRule,
				VariableRule,
				ArrowRule,
				FunctionCallRule
			>
		{};

		struct InstructionRule :
			sor<
				InstructionOpAssignmentRule,
				InstructionCompareAssignmentRule,
				InstructionLoadAssignmentRule,
				InstructionPureAssignmentRule,
				InstructionStoreAssignmentRule,
				InstructionReturnRule,
				InstructionLabelRule,
				InstructionBranchUncondRule,
				InstructionBranchCondRule,
				InstructionCallVoidRule,
				InstructionCallValRule
			>
		{};

		struct InstructionsRule :
			plus<
				seq<
					LineSeparatorsWithCommentsRule,
					bol,
					SpacesRule,
					InstructionRule,
					LineSeparatorsWithCommentsRule
				>
			>
		{};

		struct FunctionRule :
			interleaved<
				SpacesRule,
				TAO_PEGTL_STRING("define"),
				L3FunctionNameRule,
				one<'('>,
				DefArgsRule,
				one<')'>,
				one<'{'>,
				InstructionsRule,
				one<'}'>
			>
		{};

		struct ProgramRule :
			list<
				FunctionRule,
				LineSeparatorsWithCommentsRule
			>
		{};

		template<typename Rule>
		struct Selector : pegtl::parse_tree::selector<
			Rule,
			pegtl::parse_tree::store_content::on<
				NameRule,
				VariableRule,
				LabelRule,
				L3FunctionNameRule,
				NumberRule,
				ComparisonOperatorRule,
				ArithmeticOperatorRule,
				CallArgsRule,
				DefArgsRule,
				StdFunctionNameRule,
				CalleeRule,
				InstructionPureAssignmentRule,
				InstructionOpAssignmentRule,
				InstructionCompareAssignmentRule,
				InstructionLoadAssignmentRule,
				InstructionStoreAssignmentRule,
				InstructionReturnRule,
				InstructionLabelRule,
				InstructionBranchUncondRule,
				InstructionBranchCondRule,
				InstructionCallVoidRule,
				InstructionCallValRule,
				InstructionsRule,
				FunctionRule,
				ProgramRule
			>
		> {};

	}

	struct ParseNode {
		// members
		Vec<Uptr<ParseNode>> children;
		pegtl::internal::inputerator begin;
		pegtl::internal::inputerator end;
		const std::type_info *rule; // which rule this node matched on
		std::string_view type;// only used for displaying parse tree

		// special methods
		ParseNode() = default;
		ParseNode(const ParseNode &) = delete;
		ParseNode(ParseNode &&) = delete;
		ParseNode &operator=(const ParseNode &) = delete;
		ParseNode &operator=(ParseNode &&) = delete;
		~ParseNode() = default;

		// methods used for parsing

		template<typename Rule, typename ParseInput, typename... States>
		void start(const ParseInput &in, States &&...) {
			this->begin = in.inputerator();
		}

		template<typename Rule, typename ParseInput, typename... States>
		void success(const ParseInput &in, States &&...) {
			this->end = in.inputerator();
			this->rule = &typeid(Rule);
			this->type = pegtl::demangle<Rule>();
			this->type.remove_prefix(this->type.find_last_of(':') + 1);
		}

		template<typename Rule, typename ParseInput, typename... States>
		void failure(const ParseInput &in, States &&...) {}

		template<typename... States>
		void emplace_back(Uptr<ParseNode> &&child, States &&...) {
			children.emplace_back(mv(child));
		}

		std::string_view string_view() const {
			return {
				this->begin.data,
				static_cast<std::size_t>(this->end.data - this->begin.data)
			};
		}

		const ParseNode &operator[](int index) const {
			return *this->children.at(index);
		}

		// methods used to display the parse tree

		bool has_content() const noexcept {
			return this->end.data != nullptr;
		}

		bool is_root() const noexcept {
			return static_cast<bool>(this->rule);
		}
	};


	namespace node_processor {
		using namespace L3::program;

		Pair<Uptr<L3Function>, AggregateScope> make_l3_function_with_scope(const ParseNode &n) {
			assert(*n.rule == typeid(rules::FunctionRule));
			L3Function::Builder builder;
			builder.add_name("FUNCTION");
			return builder.get_result();
		}

		Uptr<Program> make_program(const ParseNode &n) {
			assert(*n.rule == typeid(rules::ProgramRule));
			Program::Builder builder;
			for (const Uptr<ParseNode> &child : n.children) {
				auto [function, agg_scope] = make_l3_function_with_scope(*child);
				builder.add_l3_function(mv(function), agg_scope);
			}
			return builder.get_result();
		}

		// TODO fill out
	}

	Uptr<L3::program::Program> parse_file(char *fileName, Opt<std::string> parse_tree_output) {
		using EntryPointRule = pegtl::must<rules::ProgramRule>;

		// Check the grammar for some possible issues.
		// TODO move this to a separate file bc it's performance-intensive
		if (pegtl::analyze<EntryPointRule>() != 0) {
			std::cerr << "There are problems with the grammar" << std::endl;
			exit(1);
		}

		// Parse
		pegtl::file_input<> fileInput(fileName);
		auto root = pegtl::parse_tree::parse<EntryPointRule, ParseNode, rules::Selector>(fileInput);
		if (!root) {
			std::cerr << "ERROR: Parser failed" << std::endl;
			exit(1);
		}
		if (parse_tree_output) {
			std::ofstream output_fstream(*parse_tree_output);
			if (output_fstream.is_open()) {
				pegtl::parse_tree::print_dot(output_fstream, *root);
				output_fstream.close();
			}
		}

		// auto p = node_processor::make_program((*root)[0]);
		// p->get_scope().fake_bind_frees(); // If you want to allow unbound name
		// p->get_scope().ensure_no_frees(); // If you want to error on unbound name
		// return p;
		return {};
	}
}