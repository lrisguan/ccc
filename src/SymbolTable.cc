/**
 * ccc
 * Copyright (c) 2026 lrisguan <lrisguan@outlook.com>
 * 
 * This program is released under the terms of the GNU General Public License version 2(GPLv2).
 * See https://opensource.org/licenses/GPL-2.0 for more information.
 * 
 * Project homepage: https://github.com/lrisguan/ccc
 * Description: A frontend of C
 */

#include "SymbolTable.h"

namespace ccc {

SymbolTable::SymbolTable() { EnterScope(); }

void SymbolTable::EnterScope() { variable_scopes_.push_back({}); }

void SymbolTable::ExitScope() {
  if (!variable_scopes_.empty()) {
    variable_scopes_.pop_back();
  }
}

bool SymbolTable::DeclareVariable(const std::string &name,
                                  VariableSymbol symbol) {
  if (variable_scopes_.empty()) {
    EnterScope();
  }
  auto &scope = variable_scopes_.back();
  if (scope.contains(name)) {
    return false;
  }
  scope.emplace(name, symbol);
  return true;
}

std::optional<VariableSymbol>
SymbolTable::LookupVariable(const std::string &name) const {
  for (auto it = variable_scopes_.rbegin(); it != variable_scopes_.rend();
       ++it) {
    const auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }
  return std::nullopt;
}

bool SymbolTable::DeclareFunction(const std::string &name,
                                  FunctionSymbol symbol) {
  const auto found = functions_.find(name);
  if (found == functions_.end()) {
    functions_.emplace(name, std::move(symbol));
    return true;
  }

  const FunctionSymbol &existing = found->second;
  if (existing.return_type != symbol.return_type ||
      existing.param_types != symbol.param_types ||
      existing.is_variadic != symbol.is_variadic ||
      (existing.defined && symbol.defined)) {
    return false;
  }

  if (symbol.defined) {
    functions_[name].defined = true;
  }
  return true;
}

std::optional<FunctionSymbol>
SymbolTable::LookupFunction(const std::string &name) const {
  const auto found = functions_.find(name);
  if (found == functions_.end()) {
    return std::nullopt;
  }
  return found->second;
}

} // namespace ccc
