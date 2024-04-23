#include <stack>
#include <queue>
#include <optional>
#include <memory>
#include <variant>
#include "DrillLib.h"

namespace tbrs {
	enum Token {
		OP,
		NUM,
		INPUT,
		LPAREN,
		RPAREN
	};

	using Operator = char;

	bool parse_op(Operator* op, StrA* parseStr) {
		if (*parseStr->str == '+' || *parseStr->str == '-' || *parseStr->str == '*' || *parseStr->str == '/') {
			*op = *parseStr->str;
			parseStr->str++;
			parseStr->length--;
			return true;
		}
		return false;
	}

	bool parse_dollardollar(StrA* parseStr) {
		if (parseStr->length >= 2 && parseStr->str[0] == parseStr->str[1] == '$') {
			parseStr->str += 2;
			parseStr->length -= 2;
			return true;
		}
		return false;
	}

	bool parse_lparen(StrA* parseStr) {
		if (*parseStr->str == '(') {
			parseStr->str++;
			parseStr->length--;
			return true;
		}
		return false;
	}

	bool parse_rparen(StrA* parseStr) {
		if (*parseStr->str == ')') {
			parseStr->str++;
			parseStr->length--;
			return true;
		}
		return false;
	}

	struct Tokens {
		std::queue<Token> tokens;
		std::queue<F64> numbers;
		std::queue<Operator> operators;
	};

	std::optional<Tokens> lex(StrA parseStr) {
		Tokens result;
		while (parseStr.length > 0) {
			F64 num;
			Operator op;
			if (ParseTools::parse_f64(&num, &parseStr)) {
				result.tokens.push(NUM);
				result.numbers.push(num);
			}
			else if (parse_op(&op, &parseStr)) {
				result.tokens.push(OP);
				result.operators.push(op);
			}
			else if (parse_dollardollar(&parseStr)) {
				result.tokens.push(INPUT);
			}
			else if (parse_lparen(&parseStr)) {
				result.tokens.push(LPAREN);
			}
			else if (parse_rparen(&parseStr)) {
				result.tokens.push(RPAREN);
			}
			else return std::nullopt;
			ParseTools::skip_whitespace(&parseStr);
		}
		return result;
	}

	struct Input {};
	using NodeVal = std::variant<Operator, F64, Input>;

	struct Node {
		NodeVal value;
		std::unique_ptr<Node> lhs;
		std::unique_ptr<Node> rhs;

		Node(NodeVal value, std::unique_ptr<Node> lhs = nullptr, std::unique_ptr<Node> rhs = nullptr) : value{ value }, lhs{ std::move(lhs) }, rhs{ std::move(rhs) } {}
	};

	using AST = std::unique_ptr<Node>;

	U64 precedence(Operator op) {
		switch (op) {
		case '*':
		case '/':
			return 2;
		case '+':
		case '-':
			return 1;
		default:
			return 0;
		}
	}

	std::optional<AST> parse_helper(Tokens&, AST, U64);

	std::optional<AST> parse(Tokens& tokens, U64 prevPrecedence = 0) {
		std::optional<AST> result;

		if (tokens.tokens.empty()) return std::nullopt;
		Token front = tokens.tokens.front();
		tokens.tokens.pop();

		switch (front) {
		case OP:
			if (tokens.operators.front() != '-') return std::nullopt;
			tokens.operators.pop();
			result = parse(tokens);
			if (!result.has_value()) return std::nullopt;
			result = std::make_unique<Node>('-', std::make_unique<Node>(-1.0), std::move(result.value()));
			break;
		case NUM:
			result = std::make_unique<Node>(tokens.numbers.front());
			tokens.numbers.pop();
			break;
		case INPUT:
			result = std::make_unique<Node>(Input{});
			break;
		case LPAREN:
			result = parse(tokens);
			if (tokens.tokens.empty() || tokens.tokens.front() != RPAREN) return std::nullopt;
			tokens.tokens.pop();
			break;
		default:
			return std::nullopt;
		}

		while (!tokens.tokens.empty()) {
			if (tokens.tokens.front() != OP) break;

			U64 curPrecedence = precedence(tokens.operators.front());
			if (curPrecedence < prevPrecedence) break;

			result = parse_helper(tokens, std::move(result.value()), curPrecedence);
		}

		return result;
	}

	std::optional<AST> parse_helper(Tokens& tokens, AST left, U64 prevPrecedence) {
		if (tokens.tokens.empty()) return std::nullopt;
		Operator op = tokens.operators.front();
		tokens.operators.pop();
		tokens.tokens.pop();

		std::optional<AST> right = parse(tokens, prevPrecedence);
		if (!right.has_value()) return std::nullopt;

		return std::make_unique<Node>(op, std::move(left), std::move(right.value()));
	}

	enum ByteCode {
		LDIN,
		LDCNST,
		ADD,
		SUB,
		MUL,
		DIV
	};

	struct ByteProgram {
		F64* constants;
		ByteCode* code;
		U64 len;
		B8 valid;

		void init() {
			constants = nullptr;
			code = nullptr;
			len = 0;
			valid = false;
		}

		void destroy() {
			delete constants;
			delete code;
		}
	};

	template<typename... Fs>
	struct overload : Fs... {
		using Fs::operator()...;
	};

	template<class...Fs>
	overload(Fs...) -> overload<Fs...>;

	void traverse(AST root, std::vector<ByteCode>& code, std::vector<F64>& constants) {
		if (!root) return;
		traverse(std::move(root->lhs), code, constants);
		traverse(std::move(root->rhs), code, constants);
		std::visit(
			overload{
				[&](Input) {
					code.push_back(LDIN);
				},
				[&](F64 num) {
					constants.push_back(num);
					code.push_back(LDCNST);
				},
				[&](Operator op) {
					switch (op) {
						case '+':
							code.push_back(ADD);
							break;
						case '-':
							code.push_back(SUB);
							break;
						case '*':
							code.push_back(MUL);
							break;
						case '/':
							code.push_back(DIV);
							break;
					}
				}
			},
			root->value);
	}

	ByteProgram compile(AST ast) {
		ByteProgram result;
		result.init();
		std::vector<ByteCode> code;
		std::vector<F64> constants;

		traverse(std::move(ast), code, constants);

		result.code = new ByteCode[code.size()];
		result.constants = new F64[constants.size()];
		result.len = code.size();
		result.valid = true;

		memcpy(result.code, code.data(), code.size() * sizeof(ByteCode));
		memcpy(result.constants, constants.data(), constants.size() * sizeof(F64));

		return result;
	}

	void parse_program(ByteProgram* program, StrA parseStr) {
		ParseTools::skip_whitespace(&parseStr);
		std::optional<Tokens> tokens = lex(parseStr);
		if (!tokens.has_value()) {
			program->valid = false;
			return;
		}

		std::optional<AST> ast = parse(tokens.value());
		if (!ast.has_value()) {
			program->valid = false;
			return;
		}

		program->destroy();
		*program = compile(std::move(ast.value()));
	}

	using AVX2 = __m256d;

	AVX2 interpret(ByteProgram program, AVX2 input) {
		std::stack<AVX2> stack;
		AVX2 op1, op2;
		U64 constIdx = 0;
		for (U32 i = 0; i < program.len; i++) {
			switch (program.code[i]) {
			case LDIN:
				stack.push(input);
				break;
			case LDCNST:
				stack.push(_mm256_set1_pd(program.constants[constIdx++]));
				break;
			case ADD:
				op2 = stack.top();
				stack.pop();
				op1 = stack.top();
				stack.pop();
				stack.push(_mm256_add_pd(op1, op2));
				break;
			case SUB:
				op2 = stack.top();
				stack.pop();
				op1 = stack.top();
				stack.pop();
				stack.push(_mm256_sub_pd(op1, op2));
				break;
			case MUL:
				op2 = stack.top();
				stack.pop();
				op1 = stack.top();
				stack.pop();
				stack.push(_mm256_mul_pd(op1, op2));
				break;
			case DIV:
				op2 = stack.top();
				stack.pop();
				op1 = stack.top();
				stack.pop();
				stack.push(_mm256_div_pd(op1, op2));
				break;
			}
		}
		return stack.top();
	}
}