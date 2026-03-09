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

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "error.h"

namespace ccc {

enum class BaseType {
  Int,
  Char,
  Float,
  Double,
  Void,
};

struct Type {
  BaseType base = BaseType::Int;
  unsigned pointer_depth = 0;

  bool operator==(const Type &other) const {
    return base == other.base && pointer_depth == other.pointer_depth;
  }
};

inline bool IsPointerType(const Type &type) { return type.pointer_depth > 0; }

inline bool IsVoidType(const Type &type) {
  return type.pointer_depth == 0 && type.base == BaseType::Void;
}

inline bool IsIntegerType(const Type &type) {
  if (IsPointerType(type)) {
    return false;
  }
  return type.base == BaseType::Int || type.base == BaseType::Char;
}

inline bool IsFloatingType(const Type &type) {
  if (IsPointerType(type)) {
    return false;
  }
  return type.base == BaseType::Float || type.base == BaseType::Double;
}

inline bool IsNumericType(const Type &type) {
  return IsIntegerType(type) || IsFloatingType(type);
}

inline bool IsVoidPointerType(const Type &type) {
  return type.pointer_depth > 0 && type.base == BaseType::Void;
}

inline int NumericRank(const Type &type) {
  if (IsPointerType(type)) {
    return -1;
  }
  switch (type.base) {
  case BaseType::Char:
    return 0;
  case BaseType::Int:
    return 1;
  case BaseType::Float:
    return 2;
  case BaseType::Double:
    return 3;
  case BaseType::Void:
    return -1;
  }
  return -1;
}

inline Type CommonNumericType(const Type &left, const Type &right) {
  return NumericRank(left) >= NumericRank(right) ? left : right;
}

inline std::string TypeToString(const Type &type) {
  std::ostringstream out;
  switch (type.base) {
  case BaseType::Int:
    out << "int";
    break;
  case BaseType::Char:
    out << "char";
    break;
  case BaseType::Float:
    out << "float";
    break;
  case BaseType::Double:
    out << "double";
    break;
  case BaseType::Void:
    out << "void";
    break;
  }
  for (unsigned i = 0; i < type.pointer_depth; ++i) {
    out << "*";
  }
  return out.str();
}

enum class UnaryOp {
  Negate,
  LogicalNot,
};

enum class BinaryOp {
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Less,
  LessEq,
  Greater,
  GreaterEq,
  Equal,
  NotEqual,
  LogicalAnd,
  LogicalOr,
};

struct Node {
  explicit Node(SourceLocation loc) : location(loc) {}
  virtual ~Node() = default;
  SourceLocation location;
};

struct Expr : Node {
  explicit Expr(SourceLocation loc) : Node(loc) {}
  ~Expr() override = default;
};

struct Stmt : Node {
  explicit Stmt(SourceLocation loc) : Node(loc) {}
  ~Stmt() override = default;
};

struct IntegerLiteralExpr : Expr {
  IntegerLiteralExpr(SourceLocation loc, int value) : Expr(loc), value(value) {}
  int value;
};

struct FloatingLiteralExpr : Expr {
  FloatingLiteralExpr(SourceLocation loc, double value)
      : Expr(loc), value(value) {}
  double value;
};

struct StringLiteralExpr : Expr {
  StringLiteralExpr(SourceLocation loc, std::string value)
      : Expr(loc), value(std::move(value)) {}
  std::string value;
};

struct IdentifierExpr : Expr {
  IdentifierExpr(SourceLocation loc, std::string name)
      : Expr(loc), name(std::move(name)) {}
  std::string name;
};

struct UnaryExpr : Expr {
  UnaryExpr(SourceLocation loc, UnaryOp op, std::unique_ptr<Expr> operand)
      : Expr(loc), op(op), operand(std::move(operand)) {}
  UnaryOp op;
  std::unique_ptr<Expr> operand;
};

struct CastExpr : Expr {
  CastExpr(SourceLocation loc, Type target_type, std::unique_ptr<Expr> operand)
      : Expr(loc), target_type(target_type), operand(std::move(operand)) {}
  Type target_type;
  std::unique_ptr<Expr> operand;
};

struct BinaryExpr : Expr {
  BinaryExpr(SourceLocation loc, BinaryOp op, std::unique_ptr<Expr> lhs,
             std::unique_ptr<Expr> rhs)
      : Expr(loc), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
  BinaryOp op;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
};

struct AssignmentExpr : Expr {
  AssignmentExpr(SourceLocation loc, std::string name,
                 std::unique_ptr<Expr> value)
      : Expr(loc), name(std::move(name)), value(std::move(value)) {}
  std::string name;
  std::unique_ptr<Expr> value;
};

struct CallExpr : Expr {
  CallExpr(SourceLocation loc, std::string callee,
           std::vector<std::unique_ptr<Expr>> args)
      : Expr(loc), callee(std::move(callee)), args(std::move(args)) {}
  std::string callee;
  std::vector<std::unique_ptr<Expr>> args;
};

struct ExprStmt : Stmt {
  ExprStmt(SourceLocation loc, std::unique_ptr<Expr> expr)
      : Stmt(loc), expr(std::move(expr)) {}
  std::unique_ptr<Expr> expr;
};

struct ReturnStmt : Stmt {
  ReturnStmt(SourceLocation loc, std::unique_ptr<Expr> value)
      : Stmt(loc), value(std::move(value)) {}
  std::unique_ptr<Expr> value;
};

struct VarDeclStmt : Stmt {
  VarDeclStmt(SourceLocation loc, std::string name, Type type,
              std::unique_ptr<Expr> initializer)
      : Stmt(loc), name(std::move(name)), type(type),
        initializer(std::move(initializer)) {}
  std::string name;
  Type type;
  std::unique_ptr<Expr> initializer;
};

struct CompoundStmt : Stmt {
  explicit CompoundStmt(SourceLocation loc) : Stmt(loc) {}
  std::vector<std::unique_ptr<Stmt>> statements;
};

struct IfStmt : Stmt {
  IfStmt(SourceLocation loc, std::unique_ptr<Expr> condition,
         std::unique_ptr<Stmt> then_branch, std::unique_ptr<Stmt> else_branch)
      : Stmt(loc), condition(std::move(condition)),
        then_branch(std::move(then_branch)),
        else_branch(std::move(else_branch)) {}
  std::unique_ptr<Expr> condition;
  std::unique_ptr<Stmt> then_branch;
  std::unique_ptr<Stmt> else_branch;
};

struct WhileStmt : Stmt {
  WhileStmt(SourceLocation loc, std::unique_ptr<Expr> condition,
            std::unique_ptr<Stmt> body)
      : Stmt(loc), condition(std::move(condition)), body(std::move(body)) {}
  std::unique_ptr<Expr> condition;
  std::unique_ptr<Stmt> body;
};

struct ParamDecl {
  SourceLocation location;
  std::string name;
  Type type;
};

struct FunctionDecl : Node {
  FunctionDecl(SourceLocation loc, std::string name, Type return_type,
               std::vector<ParamDecl> params,
               std::unique_ptr<CompoundStmt> body, bool is_extern = false,
               bool is_variadic = false)
      : Node(loc), name(std::move(name)), return_type(return_type),
        params(std::move(params)), body(std::move(body)), is_extern(is_extern),
        is_variadic(is_variadic) {}

  bool HasBody() const { return body != nullptr; }

  std::string name;
  Type return_type;
  std::vector<ParamDecl> params;
  std::unique_ptr<CompoundStmt> body;
  bool is_extern = false;
  bool is_variadic = false;
};

struct Program : Node {
  explicit Program(SourceLocation loc) : Node(loc) {}
  std::vector<std::unique_ptr<FunctionDecl>> functions;
};

} // namespace ccc
