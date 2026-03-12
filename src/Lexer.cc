/**
 * ccc
 * Copyright (c) 2026 lrisguan <lrisguan@outlook.com>
 * 
 * This program is released under the terms of the  GNU General Public License version 2(GPLv2).
 * See https://opensource.org/licenses/GPL-2.0 for more information.
 * 
 * Project homepage: https://github.com/lrisguan/ccc
 * Description: A frontend of C
 */

#include "Lexer.h"

#include <cctype>
#include <unordered_map>

namespace ccc {

namespace {

const std::unordered_map<std::string, TokenKind> kKeywords = {
		{"int", TokenKind::KwInt},     {"void", TokenKind::KwVoid},
		{"char", TokenKind::KwChar},
		{"float", TokenKind::KwFloat}, {"double", TokenKind::KwDouble},
		{"struct", TokenKind::KwStruct}, {"union", TokenKind::KwUnion},
		{"enum", TokenKind::KwEnum},
		{"return", TokenKind::KwReturn}, {"if", TokenKind::KwIf},
	{"else", TokenKind::KwElse},   {"while", TokenKind::KwWhile},
	{"for", TokenKind::KwFor},
	{"extern", TokenKind::KwExtern},
};

}  // namespace

Lexer::Lexer(std::string source, DiagnosticEngine& diag)
		: source_(std::move(source)), diag_(diag) {}

char Lexer::CurrentChar() const {
	if (index_ >= source_.size()) {
		return '\0';
	}
	return source_[index_];
}

char Lexer::PeekChar(size_t lookahead) const {
	const size_t i = index_ + lookahead;
	if (i >= source_.size()) {
		return '\0';
	}
	return source_[i];
}

bool Lexer::IsAtEnd() const {
	return index_ >= source_.size();
}

void Lexer::Advance() {
	if (IsAtEnd()) {
		return;
	}
	if (source_[index_] == '\n') {
		++line_;
		column_ = 1;
	} else {
		++column_;
	}
	++index_;
}

void Lexer::SkipWhitespaceAndComments() {
	while (!IsAtEnd()) {
		const char c = CurrentChar();
		if (std::isspace(static_cast<unsigned char>(c))) {
			Advance();
			continue;
		}
		if (c == '/' && PeekChar() == '/') {
			while (!IsAtEnd() && CurrentChar() != '\n') {
				Advance();
			}
			continue;
		}
		if (c == '/' && PeekChar() == '*') {
			const SourceLocation start{line_, column_};
			Advance();
			Advance();
			while (!IsAtEnd()) {
				if (CurrentChar() == '*' && PeekChar() == '/') {
					Advance();
					Advance();
					break;
				}
				Advance();
			}
			if (IsAtEnd() && !(index_ >= 2 && source_[index_ - 2] == '*' && source_[index_ - 1] == '/')) {
				diag_.ReportError(start, "unterminated block comment");
			}
			continue;
		}
		break;
	}
}

Token Lexer::MakeToken(TokenKind kind, SourceLocation loc, std::string lexeme) const {
	return Token{kind, std::move(lexeme), loc, 0};
}

Token Lexer::LexIdentifierOrKeyword() {
	const SourceLocation loc{line_, column_};
	std::string text;
	while (!IsAtEnd()) {
		const char c = CurrentChar();
		if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
			text.push_back(c);
			Advance();
			continue;
		}
		break;
	}

	const auto it = kKeywords.find(text);
	if (it != kKeywords.end()) {
		return MakeToken(it->second, loc, text);
	}
	return MakeToken(TokenKind::Identifier, loc, text);
}

Token Lexer::LexNumber() {
	const SourceLocation loc{line_, column_};
	std::string text;
	bool is_floating = false;
	while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(CurrentChar()))) {
		text.push_back(CurrentChar());
		Advance();
	}

	if (!IsAtEnd() && CurrentChar() == '.' &&
				std::isdigit(static_cast<unsigned char>(PeekChar()))) {
		is_floating = true;
		text.push_back(CurrentChar());
		Advance();
		while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(CurrentChar()))) {
			text.push_back(CurrentChar());
			Advance();
		}
	}

	Token tok = MakeToken(TokenKind::Number, loc, text);
	tok.is_floating = is_floating;
	try {
		if (is_floating) {
			tok.double_value = std::stod(text);
		} else {
			tok.int_value = std::stoi(text);
			tok.double_value = static_cast<double>(tok.int_value);
		}
	} catch (...) {
		diag_.ReportError(loc, is_floating ? "floating literal out of range"
													 : "integer literal out of range");
		tok.int_value = 0;
		tok.double_value = 0.0;
	}
	return tok;
}

Token Lexer::LexStringLiteral() {
	const SourceLocation loc{line_, column_};
	std::string value;
	Advance();
	while (!IsAtEnd()) {
		const char c = CurrentChar();
		if (c == '"') {
			Advance();
			return MakeToken(TokenKind::String, loc, value);
		}
		if (c == '\\') {
			Advance();
			if (IsAtEnd()) {
				break;
			}
			const char esc = CurrentChar();
			switch (esc) {
				case 'n': value.push_back('\n'); break;
				case 't': value.push_back('\t'); break;
				case 'r': value.push_back('\r'); break;
				case '\\': value.push_back('\\'); break;
				case '"': value.push_back('"'); break;
				case '\'': value.push_back('\''); break;
				case '0': value.push_back('\0'); break;
				default: value.push_back(esc); break;
			}
			Advance();
			continue;
		}
		value.push_back(c);
		Advance();
	}

	diag_.ReportError(loc, "unterminated string literal");
	return MakeToken(TokenKind::String, loc, value);
}

Token Lexer::NextToken() {
	SkipWhitespaceAndComments();
	const SourceLocation loc{line_, column_};
	if (IsAtEnd()) {
		return MakeToken(TokenKind::EndOfFile, loc, "");
	}

	const char c = CurrentChar();
	if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
		return LexIdentifierOrKeyword();
	}
	if (std::isdigit(static_cast<unsigned char>(c))) {
		return LexNumber();
	}
	if (c == '.' && std::isdigit(static_cast<unsigned char>(PeekChar()))) {
		return LexNumber();
	}
	if (c == '"') {
		return LexStringLiteral();
	}

	Advance();
	switch (c) {
		case '(': return MakeToken(TokenKind::LParen, loc, "(");
		case ')': return MakeToken(TokenKind::RParen, loc, ")");
		case '{': return MakeToken(TokenKind::LBrace, loc, "{");
		case '}': return MakeToken(TokenKind::RBrace, loc, "}");
		case '[': return MakeToken(TokenKind::LBracket, loc, "[");
		case ']': return MakeToken(TokenKind::RBracket, loc, "]");
		case ',': return MakeToken(TokenKind::Comma, loc, ",");
		case '.':
			if (CurrentChar() == '.' && PeekChar() == '.') {
				Advance();
				Advance();
				return MakeToken(TokenKind::Ellipsis, loc, "...");
			}
			return MakeToken(TokenKind::Dot, loc, ".");
		case ';': return MakeToken(TokenKind::Semicolon, loc, ";");
		case '+': return MakeToken(TokenKind::Plus, loc, "+");
		case '-':
			if (CurrentChar() == '>') {
				Advance();
				return MakeToken(TokenKind::Arrow, loc, "->");
			}
			return MakeToken(TokenKind::Minus, loc, "-");
		case '*': return MakeToken(TokenKind::Star, loc, "*");
		case '%': return MakeToken(TokenKind::Percent, loc, "%");
		case '/': return MakeToken(TokenKind::Slash, loc, "/");
		case '=':
			if (CurrentChar() == '=') {
				Advance();
				return MakeToken(TokenKind::EqualEqual, loc, "==");
			}
			return MakeToken(TokenKind::Equal, loc, "=");
		case '!':
			if (CurrentChar() == '=') {
				Advance();
				return MakeToken(TokenKind::BangEqual, loc, "!=");
			}
			return MakeToken(TokenKind::Bang, loc, "!");
		case '<':
			if (CurrentChar() == '=') {
				Advance();
				return MakeToken(TokenKind::LessEqual, loc, "<=");
			}
			return MakeToken(TokenKind::Less, loc, "<");
		case '>':
			if (CurrentChar() == '=') {
				Advance();
				return MakeToken(TokenKind::GreaterEqual, loc, ">=");
			}
			return MakeToken(TokenKind::Greater, loc, ">");
		case '&':
			if (CurrentChar() == '&') {
				Advance();
				return MakeToken(TokenKind::AndAnd, loc, "&&");
			}
			break;
		case '|':
			if (CurrentChar() == '|') {
				Advance();
				return MakeToken(TokenKind::OrOr, loc, "||");
			}
			break;
		default:
			break;
	}

	diag_.ReportError(loc, std::string("unexpected character '") + c + "'");
	return NextToken();
}

const char* TokenKindName(TokenKind kind) {
	switch (kind) {
		case TokenKind::EndOfFile: return "end-of-file";
		case TokenKind::Identifier: return "identifier";
		case TokenKind::Number: return "number";
		case TokenKind::String: return "string";
		case TokenKind::KwInt: return "int";
		case TokenKind::KwChar: return "char";
		case TokenKind::KwFloat: return "float";
		case TokenKind::KwDouble: return "double";
		case TokenKind::KwVoid: return "void";
		case TokenKind::KwStruct: return "struct";
		case TokenKind::KwUnion: return "union";
		case TokenKind::KwEnum: return "enum";
		case TokenKind::KwReturn: return "return";
		case TokenKind::KwIf: return "if";
		case TokenKind::KwElse: return "else";
		case TokenKind::KwWhile: return "while";
		case TokenKind::KwFor: return "for";
		case TokenKind::KwExtern: return "extern";
		case TokenKind::LParen: return "(";
		case TokenKind::RParen: return ")";
		case TokenKind::LBrace: return "{";
		case TokenKind::RBrace: return "}";
		case TokenKind::LBracket: return "[";
		case TokenKind::RBracket: return "]";
		case TokenKind::Comma: return ",";
		case TokenKind::Ellipsis: return "...";
		case TokenKind::Semicolon: return ";";
		case TokenKind::Dot: return ".";
		case TokenKind::Arrow: return "->";
		case TokenKind::Plus: return "+";
		case TokenKind::Minus: return "-";
		case TokenKind::Star: return "*";
		case TokenKind::Slash: return "/";
		case TokenKind::Percent: return "%";
		case TokenKind::Equal: return "=";
		case TokenKind::EqualEqual: return "==";
		case TokenKind::Bang: return "!";
		case TokenKind::BangEqual: return "!=";
		case TokenKind::Less: return "<";
		case TokenKind::LessEqual: return "<=";
		case TokenKind::Greater: return ">";
		case TokenKind::GreaterEqual: return ">=";
		case TokenKind::AndAnd: return "&&";
		case TokenKind::OrOr: return "||";
	}
	return "<unknown-token>";
}

}  // namespace ccc
