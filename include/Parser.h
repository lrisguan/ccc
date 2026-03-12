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

#include <deque>
#include <memory>

#include "AST.h"
#include "Lexer.h"

namespace ccc {

class Parser {
 public:
	Parser(Lexer& lexer, DiagnosticEngine& diag);

	std::unique_ptr<Program> ParseProgram();

 private:
	struct ParsedDeclarator {
		std::string name;
		Type type;
		bool is_function = false;
		std::vector<ParamDecl> function_params;
		bool function_is_variadic = false;
	};

	bool Match(TokenKind kind);
	bool Check(TokenKind kind) const;
	bool Expect(TokenKind kind, const char* message);
	void Advance();
	void Synchronize();

	bool IsTypeSpecifier(TokenKind kind) const;
	Type ParseTypeSpecifier();
	Type ParseType();
	ParsedDeclarator ParseDeclarator(const Type& base_type,
								 bool require_identifier,
								 bool allow_function_suffix);
	std::vector<TagMemberDecl> ParseTagDefinitionBody();
	void RegisterTagTypeDecl(UserTypeKind kind, const std::string& tag,
							 const std::vector<TagMemberDecl>& members);
	std::unique_ptr<FunctionDecl> ParseFunction();
	std::vector<ParamDecl> ParseParameterList(bool& is_variadic);

	std::unique_ptr<Stmt> ParseStatement();
	std::unique_ptr<CompoundStmt> ParseCompoundStatement();
	std::unique_ptr<Stmt> ParseIfStatement();
	std::unique_ptr<Stmt> ParseWhileStatement();
	std::unique_ptr<Stmt> ParseForStatement();
	std::unique_ptr<Stmt> ParseForInitStatement();
	std::unique_ptr<Stmt> ParseReturnStatement();
	std::unique_ptr<Stmt> ParseVarDeclStatement();
	std::unique_ptr<Stmt> ParseExpressionStatement();

	std::unique_ptr<Expr> ParseExpression();
	std::unique_ptr<Expr> ParseAssignment();
	std::unique_ptr<Expr> ParseLogicalOr();
	std::unique_ptr<Expr> ParseLogicalAnd();
	std::unique_ptr<Expr> ParseEquality();
	std::unique_ptr<Expr> ParseComparison();
	std::unique_ptr<Expr> ParseTerm();
	std::unique_ptr<Expr> ParseFactor();
	std::unique_ptr<Expr> ParseUnary();
	std::unique_ptr<Expr> ParsePostfix();
	std::unique_ptr<Expr> ParsePrimary();
	std::vector<std::unique_ptr<Expr>> ParseArguments();

	Lexer& lexer_;
	DiagnosticEngine& diag_;
	Token current_;
	Token previous_;
	std::deque<std::unique_ptr<Stmt>> pending_statements_;
	std::vector<TagTypeDecl> parsed_tag_types_;
};

}  // namespace ccc
