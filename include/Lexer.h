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

#pragma once

#include <cstddef>
#include <string>

#include "error.h"

namespace ccc {

enum class TokenKind {
	EndOfFile,
	Identifier,
	Number,
	String,
	KwInt,
	KwChar,
	KwFloat,
	KwDouble,
	KwVoid,
	KwReturn,
	KwIf,
	KwElse,
	KwWhile,
	KwExtern,
	LParen,
	RParen,
	LBrace,
	RBrace,
	Comma,
	Ellipsis,
	Semicolon,
	Plus,
	Minus,
	Star,
	Slash,
	Percent,
	Equal,
	EqualEqual,
	Bang,
	BangEqual,
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
	AndAnd,
	OrOr,
};

struct Token {
	TokenKind kind = TokenKind::EndOfFile;
	std::string lexeme;
	SourceLocation location;
	int int_value = 0;
	double double_value = 0.0;
	bool is_floating = false;
};

class Lexer {
 public:
	Lexer(std::string source, DiagnosticEngine& diag);

	Token NextToken();

 private:
	char CurrentChar() const;
	char PeekChar(size_t lookahead = 1) const;
	bool IsAtEnd() const;
	void Advance();
	void SkipWhitespaceAndComments();
	Token MakeToken(TokenKind kind, SourceLocation loc, std::string lexeme = "") const;
	Token LexIdentifierOrKeyword();
	Token LexNumber();
	Token LexStringLiteral();

	std::string source_;
	DiagnosticEngine& diag_;
	size_t index_ = 0;
	size_t line_ = 1;
	size_t column_ = 1;
};

const char* TokenKindName(TokenKind kind);

}  // namespace ccc
