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

#include <unordered_map>

#include "AST.h"
#include "SymbolTable.h"
#include "error.h"

namespace ccc {

class SemanticAnalyzer {
 public:
	explicit SemanticAnalyzer(DiagnosticEngine& diag);

	bool Analyze(const Program& program);

 private:
	void AnalyzeFunction(const FunctionDecl& fn);
	void AnalyzeStmt(const Stmt& stmt);
	Type AnalyzeExpr(const Expr& expr);
	Type AnalyzeLValueExpr(const Expr& expr);
	bool IsAssignable(const Type& target, const Type& source) const;
	const TagTypeDecl* LookupTagType(const Type& type) const;

	DiagnosticEngine& diag_;
	SymbolTable symbols_;
	std::unordered_map<std::string, TagTypeDecl> tag_types_;
	Type current_return_type_;
};

}  // namespace ccc
