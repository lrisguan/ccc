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

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "AST.h"

namespace ccc {

struct VariableSymbol {
  Type type;
};

struct FunctionSymbol {
  Type return_type;
  std::vector<Type> param_types;
  bool is_variadic = false;
  bool defined = false;
};

class SymbolTable {
public:
  SymbolTable();

  void EnterScope();
  void ExitScope();

  bool DeclareVariable(const std::string &name, VariableSymbol symbol);
  std::optional<VariableSymbol> LookupVariable(const std::string &name) const;

  bool DeclareFunction(const std::string &name, FunctionSymbol symbol);
  std::optional<FunctionSymbol> LookupFunction(const std::string &name) const;

private:
  std::vector<std::unordered_map<std::string, VariableSymbol>> variable_scopes_;
  std::unordered_map<std::string, FunctionSymbol> functions_;
};

} // namespace ccc
