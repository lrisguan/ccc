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

#include "SemanticAnalyzer.h"

#include <vector>

namespace ccc {

namespace {

Type IntType() { return Type{BaseType::Int, 0}; }

std::string TagKey(UserTypeKind kind, const std::string &tag) {
  switch (kind) {
  case UserTypeKind::Struct:
    return "struct:" + tag;
  case UserTypeKind::Union:
    return "union:" + tag;
  case UserTypeKind::Enum:
    return "enum:" + tag;
  case UserTypeKind::None:
    return "none:" + tag;
  }
  return "none:" + tag;
}

Type DecayArrayType(Type type) {
  if (!type.array_dimensions.empty()) {
    type.array_dimensions.erase(type.array_dimensions.begin());
    ++type.pointer_depth;
  }
  return type;
}

Type FunctionSymbolAsPointerType(const FunctionSymbol &symbol) {
  Type type;
  type.base = BaseType::Void;
  type.pointer_depth = 1;
  type.function_pointer = std::make_shared<FunctionPointerSignature>();
  type.function_pointer->return_type = symbol.return_type;
  type.function_pointer->param_types = symbol.param_types;
  type.function_pointer->is_variadic = symbol.is_variadic;
  return type;
}

bool IsConditionType(const Type &type) {
  return IsNumericType(type) || IsPointerType(type);
}

} // namespace

SemanticAnalyzer::SemanticAnalyzer(DiagnosticEngine &diag) : diag_(diag) {
  current_return_type_ = IntType();
}

bool SemanticAnalyzer::IsAssignable(const Type &target,
                                    const Type &source) const {
  if (IsArrayType(target) || IsArrayType(source)) {
    return target == source;
  }
  if (target == source) {
    return true;
  }
  if (IsNumericType(target) && IsNumericType(source)) {
    return true;
  }
  if (IsPointerType(target) && IsPointerType(source)) {
    if (target.function_pointer || source.function_pointer) {
      return target == source;
    }
    return IsVoidPointerType(target) || IsVoidPointerType(source);
  }
  return false;
}

bool SemanticAnalyzer::Analyze(const Program &program) {
  tag_types_.clear();
  for (const auto &tag_decl : program.tag_types) {
    if (tag_decl.kind == UserTypeKind::None || tag_decl.tag.empty()) {
      continue;
    }
    tag_types_[TagKey(tag_decl.kind, tag_decl.tag)] = tag_decl;
  }

  for (const auto &fn : program.functions) {
    FunctionSymbol symbol;
    symbol.return_type = fn->return_type;
    symbol.is_variadic = fn->is_variadic;
    symbol.defined = fn->HasBody();
    for (const auto &p : fn->params) {
      symbol.param_types.push_back(p.type);
      if (IsVoidType(p.type)) {
        diag_.ReportError(p.location, "parameter cannot have type void");
      }
    }
    if (!symbols_.DeclareFunction(fn->name, symbol)) {
      diag_.ReportError(fn->location,
                        "conflicting or duplicate function declaration: '" +
                            fn->name + "'");
    }
  }

  for (const auto &fn : program.functions) {
    if (fn->HasBody()) {
      AnalyzeFunction(*fn);
    }
  }

  return !diag_.HasErrors();
}

void SemanticAnalyzer::AnalyzeFunction(const FunctionDecl &fn) {
  if (!fn.body) {
    return;
  }

  current_return_type_ = fn.return_type;
  symbols_.EnterScope();
  for (const auto &param : fn.params) {
    if (!symbols_.DeclareVariable(param.name, VariableSymbol{param.type})) {
      diag_.ReportError(param.location,
                        "duplicate parameter name: '" + param.name + "'");
    }
  }

  AnalyzeStmt(*fn.body);
  symbols_.ExitScope();
}

void SemanticAnalyzer::AnalyzeStmt(const Stmt &stmt) {
  if (const auto *s = dynamic_cast<const CompoundStmt *>(&stmt)) {
    symbols_.EnterScope();
    for (const auto &nested : s->statements) {
      AnalyzeStmt(*nested);
    }
    symbols_.ExitScope();
    return;
  }

  if (const auto *s = dynamic_cast<const VarDeclStmt *>(&stmt)) {
    if (!symbols_.DeclareVariable(s->name, VariableSymbol{s->type})) {
      diag_.ReportError(s->location,
                        "redeclaration of variable: '" + s->name + "'");
    }
    if (s->initializer) {
      const Type init_type = AnalyzeExpr(*s->initializer);
      if (!IsAssignable(s->type, init_type)) {
        diag_.ReportError(s->initializer->location,
                          "initializer type '" + TypeToString(init_type) +
                              "' does not match variable type '" +
                              TypeToString(s->type) + "'");
      }
    }
    return;
  }

  if (const auto *s = dynamic_cast<const ReturnStmt *>(&stmt)) {
    if (!s->value) {
      if (!IsVoidType(current_return_type_)) {
        diag_.ReportError(s->location, "non-void function must return a value");
      }
      return;
    }
    const Type returned = AnalyzeExpr(*s->value);
    if (!IsAssignable(current_return_type_, returned)) {
      diag_.ReportError(s->value->location,
                        "return type '" + TypeToString(returned) +
                            "' does not match function return type '" +
                            TypeToString(current_return_type_) + "'");
    }
    return;
  }

  if (const auto *s = dynamic_cast<const IfStmt *>(&stmt)) {
    const Type cond = AnalyzeExpr(*s->condition);
    if (!IsConditionType(cond)) {
      diag_.ReportError(s->condition->location,
                        "if condition must be numeric or pointer");
    }
    AnalyzeStmt(*s->then_branch);
    if (s->else_branch) {
      AnalyzeStmt(*s->else_branch);
    }
    return;
  }

  if (const auto *s = dynamic_cast<const WhileStmt *>(&stmt)) {
    const Type cond = AnalyzeExpr(*s->condition);
    if (!IsConditionType(cond)) {
      diag_.ReportError(s->condition->location,
                        "while condition must be numeric or pointer");
    }
    AnalyzeStmt(*s->body);
    return;
  }

  if (const auto *s = dynamic_cast<const ForStmt *>(&stmt)) {
    symbols_.EnterScope();
    if (s->init) {
      AnalyzeStmt(*s->init);
    }
    if (s->condition) {
      const Type cond = AnalyzeExpr(*s->condition);
      if (!IsConditionType(cond)) {
        diag_.ReportError(s->condition->location,
                          "for condition must be numeric or pointer");
      }
    }
    if (s->increment) {
      AnalyzeExpr(*s->increment);
    }
    AnalyzeStmt(*s->body);
    symbols_.ExitScope();
    return;
  }

  if (const auto *s = dynamic_cast<const ExprStmt *>(&stmt)) {
    if (s->expr) {
      AnalyzeExpr(*s->expr);
    }
    return;
  }
}

Type SemanticAnalyzer::AnalyzeExpr(const Expr &expr) {
  if (dynamic_cast<const IntegerLiteralExpr *>(&expr)) {
    return IntType();
  }

  if (dynamic_cast<const FloatingLiteralExpr *>(&expr)) {
    return Type{BaseType::Double, 0};
  }

  if (dynamic_cast<const StringLiteralExpr *>(&expr)) {
    return Type{BaseType::Char, 1};
  }

  if (const auto *e = dynamic_cast<const IdentifierExpr *>(&expr)) {
    const auto sym = symbols_.LookupVariable(e->name);
    if (sym.has_value()) {
      return DecayArrayType(sym->type);
    }

    const auto fn = symbols_.LookupFunction(e->name);
    if (fn.has_value()) {
      return FunctionSymbolAsPointerType(*fn);
    }

    if (!sym.has_value()) {
      diag_.ReportError(e->location,
                        "use of undeclared identifier: '" + e->name + "'");
      return IntType();
    }
  }

  if (dynamic_cast<const MemberExpr *>(&expr)) {
    return DecayArrayType(AnalyzeLValueExpr(expr));
  }

  if (const auto *e = dynamic_cast<const UnaryExpr *>(&expr)) {
    const Type operand = AnalyzeExpr(*e->operand);
    if (!IsNumericType(operand)) {
      diag_.ReportError(e->location, "unary operator requires numeric operand");
      return IntType();
    }
    if (e->op == UnaryOp::LogicalNot) {
      return IntType();
    }
    return operand;
  }

  if (const auto *e = dynamic_cast<const CastExpr *>(&expr)) {
    AnalyzeExpr(*e->operand);
    return e->target_type;
  }

  if (const auto *e = dynamic_cast<const BinaryExpr *>(&expr)) {
    const Type left = AnalyzeExpr(*e->lhs);
    const Type right = AnalyzeExpr(*e->rhs);

    switch (e->op) {
    case BinaryOp::Add:
    case BinaryOp::Sub:
    case BinaryOp::Mul:
    case BinaryOp::Div:
      if (!IsNumericType(left) || !IsNumericType(right)) {
        diag_.ReportError(e->location,
                          "arithmetic operators require numeric operands");
        return IntType();
      }
      return CommonNumericType(left, right);
    case BinaryOp::Mod:
      if (!IsIntegerType(left) || !IsIntegerType(right)) {
        diag_.ReportError(e->location, "'%' requires integer operands");
        return IntType();
      }
      return CommonNumericType(left, right);
    case BinaryOp::Less:
    case BinaryOp::LessEq:
    case BinaryOp::Greater:
    case BinaryOp::GreaterEq:
      if (!IsNumericType(left) || !IsNumericType(right)) {
        diag_.ReportError(e->location,
                          "comparison operators require numeric operands");
      }
      return IntType();
    case BinaryOp::Equal:
    case BinaryOp::NotEqual:
      if (!(IsNumericType(left) && IsNumericType(right)) &&
          !(IsPointerType(left) && IsPointerType(right)) &&
          !(IsPointerType(left) && IsIntegerType(right)) &&
          !(IsIntegerType(left) && IsPointerType(right))) {
        diag_.ReportError(
            e->location,
            "equality operators require both numeric or both pointer operands");
      }
      return IntType();
    case BinaryOp::LogicalAnd:
    case BinaryOp::LogicalOr:
      if (!IsConditionType(left) || !IsConditionType(right)) {
        diag_.ReportError(
            e->location,
            "logical operators require numeric or pointer operands");
      }
      return IntType();
    }
  }

  if (const auto *e = dynamic_cast<const AssignmentExpr *>(&expr)) {
    const Type target_type = AnalyzeLValueExpr(*e->target);
    const Type value_type = AnalyzeExpr(*e->value);
    if (!IsAssignable(target_type, value_type)) {
      diag_.ReportError(e->value->location,
                        "assignment value type '" + TypeToString(value_type) +
                            "' does not match variable type '" +
                            TypeToString(target_type) + "'");
    }
    return target_type;
  }

  if (const auto *e = dynamic_cast<const CallExpr *>(&expr)) {
    const auto fn = symbols_.LookupFunction(e->callee);
    if (!fn.has_value()) {
      // Accept undeclared calls as implicit extern int f(...).
      // This keeps libc-style variadic calls usable while headers evolve.
      for (const auto &arg : e->args) {
        AnalyzeExpr(*arg);
      }
      return IntType();
    }

    const size_t fixed_count = fn->param_types.size();
    if ((!fn->is_variadic && fixed_count != e->args.size()) ||
        (fn->is_variadic && e->args.size() < fixed_count)) {
      diag_.ReportError(
          e->location,
          "function '" + e->callee + "' expects " +
              (fn->is_variadic ? (std::to_string(fixed_count) + "+")
                               : std::to_string(fixed_count)) +
              " argument(s), got " + std::to_string(e->args.size()));
    }

    for (size_t i = 0; i < e->args.size(); ++i) {
      const Type arg_type = AnalyzeExpr(*e->args[i]);
      if (i < fn->param_types.size() &&
          !IsAssignable(fn->param_types[i], arg_type)) {
        diag_.ReportError(e->args[i]->location,
                          "argument type mismatch at position " +
                              std::to_string(i + 1) + ": expected '" +
                              TypeToString(fn->param_types[i]) + "', got '" +
                              TypeToString(arg_type) + "'");
      }
    }
    return fn->return_type;
  }

  return IntType();
}

const TagTypeDecl *SemanticAnalyzer::LookupTagType(const Type &type) const {
  if (type.user_kind != UserTypeKind::Struct &&
      type.user_kind != UserTypeKind::Union &&
      type.user_kind != UserTypeKind::Enum) {
    return nullptr;
  }
  const auto found = tag_types_.find(TagKey(type.user_kind, type.user_tag));
  if (found == tag_types_.end()) {
    return nullptr;
  }
  return &found->second;
}

Type SemanticAnalyzer::AnalyzeLValueExpr(const Expr &expr) {
  if (const auto *id = dynamic_cast<const IdentifierExpr *>(&expr)) {
    const auto sym = symbols_.LookupVariable(id->name);
    if (!sym.has_value()) {
      diag_.ReportError(id->location,
                        "assignment to undeclared identifier: '" + id->name +
                            "'");
      return IntType();
    }
    return sym->type;
  }

  if (const auto *member = dynamic_cast<const MemberExpr *>(&expr)) {
    Type base_type = AnalyzeExpr(*member->base);
    if (member->via_pointer) {
      if (base_type.pointer_depth == 0) {
        diag_.ReportError(member->location,
                          "'->' requires a pointer to struct or union");
        return IntType();
      }
      --base_type.pointer_depth;
    } else if (base_type.pointer_depth > 0) {
      diag_.ReportError(member->location,
                        "'.' requires a struct/union object, not pointer");
      return IntType();
    }

    if (base_type.user_kind != UserTypeKind::Struct &&
        base_type.user_kind != UserTypeKind::Union) {
      diag_.ReportError(member->location,
                        "member access requires struct or union type");
      return IntType();
    }

    const TagTypeDecl *tag_decl = LookupTagType(base_type);
    if (!tag_decl) {
      diag_.ReportError(member->location,
                        "unknown or incomplete tag type for member access");
      return IntType();
    }

    for (const auto &field : tag_decl->members) {
      if (field.name == member->member_name) {
        return field.type;
      }
    }

    diag_.ReportError(member->location,
                      "unknown member '" + member->member_name + "' in " +
                          TypeToString(base_type));
    return IntType();
  }

  diag_.ReportError(expr.location,
                    "left-hand side of assignment must be an lvalue");
  return IntType();
}

} // namespace ccc
