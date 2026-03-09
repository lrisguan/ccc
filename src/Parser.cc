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

#include "Parser.h"

#include <utility>

namespace ccc {

Parser::Parser(Lexer &lexer, DiagnosticEngine &diag)
    : lexer_(lexer), diag_(diag), current_(lexer_.NextToken()),
      previous_(current_) {}

bool Parser::Match(TokenKind kind) {
  if (!Check(kind)) {
    return false;
  }
  Advance();
  return true;
}

bool Parser::Check(TokenKind kind) const { return current_.kind == kind; }

bool Parser::Expect(TokenKind kind, const char *message) {
  if (Check(kind)) {
    Advance();
    return true;
  }
  diag_.ReportError(current_.location, std::string(message) + ", got '" +
                                           TokenKindName(current_.kind) + "'");
  return false;
}

bool Parser::IsTypeSpecifier(TokenKind kind) const {
  return kind == TokenKind::KwInt || kind == TokenKind::KwChar ||
         kind == TokenKind::KwFloat || kind == TokenKind::KwDouble ||
         kind == TokenKind::KwVoid;
}

void Parser::Advance() {
  previous_ = current_;
  current_ = lexer_.NextToken();
}

void Parser::Synchronize() {
  while (!Check(TokenKind::EndOfFile)) {
    if (previous_.kind == TokenKind::Semicolon ||
        previous_.kind == TokenKind::RBrace) {
      return;
    }
    switch (current_.kind) {
    case TokenKind::KwIf:
    case TokenKind::KwWhile:
    case TokenKind::KwReturn:
    case TokenKind::KwInt:
    case TokenKind::KwChar:
    case TokenKind::KwFloat:
    case TokenKind::KwDouble:
    case TokenKind::KwVoid:
    case TokenKind::KwExtern:
    case TokenKind::LBrace:
      return;
    default:
      break;
    }
    Advance();
  }
}

Type Parser::ParseType() {
  Type type;
  if (Match(TokenKind::KwInt)) {
    type.base = BaseType::Int;
  } else if (Match(TokenKind::KwChar)) {
    type.base = BaseType::Char;
  } else if (Match(TokenKind::KwFloat)) {
    type.base = BaseType::Float;
  } else if (Match(TokenKind::KwDouble)) {
    type.base = BaseType::Double;
  } else if (Match(TokenKind::KwVoid)) {
    type.base = BaseType::Void;
  } else {
    diag_.ReportError(current_.location, "expected type specifier");
    type.base = BaseType::Int;
  }

  while (Match(TokenKind::Star)) {
    ++type.pointer_depth;
  }

  return type;
}

std::unique_ptr<Program> Parser::ParseProgram() {
  auto program = std::make_unique<Program>(SourceLocation{1, 1});
  while (!Check(TokenKind::EndOfFile)) {
    auto fn = ParseFunction();
    if (fn) {
      program->functions.push_back(std::move(fn));
      continue;
    }
    Synchronize();
    if (Check(TokenKind::EndOfFile)) {
      break;
    }
    Advance();
  }
  return program;
}

std::unique_ptr<FunctionDecl> Parser::ParseFunction() {
  const SourceLocation start = current_.location;
  const bool is_extern = Match(TokenKind::KwExtern);
  const Type return_type = ParseType();

  if (!Expect(TokenKind::Identifier, "expected function name")) {
    return nullptr;
  }
  std::string name = previous_.lexeme;

  if (!Expect(TokenKind::LParen, "expected '(' after function name")) {
    return nullptr;
  }
  bool is_variadic = false;
  auto params = ParseParameterList(is_variadic);
  if (!Expect(TokenKind::RParen, "expected ')' after parameters")) {
    return nullptr;
  }

  std::unique_ptr<CompoundStmt> body;
  if (Match(TokenKind::Semicolon)) {
    body = nullptr;
  } else if (Check(TokenKind::LBrace)) {
    if (is_extern) {
      diag_.ReportError(start,
                        "extern declaration cannot contain a function body");
      return nullptr;
    }
    body = ParseCompoundStatement();
    if (!body) {
      return nullptr;
    }
  } else {
    diag_.ReportError(
        current_.location,
        "expected ';' for declaration or '{' for function definition");
    return nullptr;
  }

  return std::make_unique<FunctionDecl>(start, std::move(name), return_type,
                                        std::move(params), std::move(body),
                                        is_extern, is_variadic);
}

std::vector<ParamDecl> Parser::ParseParameterList(bool &is_variadic) {
  is_variadic = false;
  std::vector<ParamDecl> params;
  if (Check(TokenKind::RParen)) {
    return params;
  }
  if (Match(TokenKind::Ellipsis)) {
    is_variadic = true;
    return params;
  }

  while (true) {
    const SourceLocation loc = current_.location;
    const Type type = ParseType();
    if (params.empty() && IsVoidType(type) && Check(TokenKind::RParen)) {
      return params;
    }
    if (!Expect(TokenKind::Identifier, "expected parameter name")) {
      break;
    }
    params.push_back(ParamDecl{loc, previous_.lexeme, type});
    if (!Match(TokenKind::Comma)) {
      break;
    }
    if (Match(TokenKind::Ellipsis)) {
      is_variadic = true;
      break;
    }
  }
  return params;
}

std::unique_ptr<Stmt> Parser::ParseStatement() {
  if (!pending_statements_.empty()) {
    auto stmt = std::move(pending_statements_.front());
    pending_statements_.pop_front();
    return stmt;
  }

  if (Check(TokenKind::LBrace)) {
    return ParseCompoundStatement();
  }
  if (Check(TokenKind::KwIf)) {
    return ParseIfStatement();
  }
  if (Check(TokenKind::KwWhile)) {
    return ParseWhileStatement();
  }
  if (Check(TokenKind::KwReturn)) {
    return ParseReturnStatement();
  }
  if (IsTypeSpecifier(current_.kind)) {
    return ParseVarDeclStatement();
  }
  return ParseExpressionStatement();
}

std::unique_ptr<CompoundStmt> Parser::ParseCompoundStatement() {
  const SourceLocation loc = current_.location;
  if (!Expect(TokenKind::LBrace, "expected '{'")) {
    return nullptr;
  }
  auto block = std::make_unique<CompoundStmt>(loc);
  while (!Check(TokenKind::RBrace) && !Check(TokenKind::EndOfFile)) {
    auto stmt = ParseStatement();
    if (stmt) {
      block->statements.push_back(std::move(stmt));
      continue;
    }
    Synchronize();
    if (Check(TokenKind::RBrace)) {
      break;
    }
    if (!Check(TokenKind::EndOfFile)) {
      Advance();
    }
  }
  Expect(TokenKind::RBrace, "expected '}' to close block");
  return block;
}

std::unique_ptr<Stmt> Parser::ParseIfStatement() {
  const SourceLocation loc = current_.location;
  Advance();
  if (!Expect(TokenKind::LParen, "expected '(' after if")) {
    return nullptr;
  }
  auto condition = ParseExpression();
  if (!Expect(TokenKind::RParen, "expected ')' after condition")) {
    return nullptr;
  }
  auto then_branch = ParseStatement();
  std::unique_ptr<Stmt> else_branch;
  if (Match(TokenKind::KwElse)) {
    else_branch = ParseStatement();
  }
  return std::make_unique<IfStmt>(loc, std::move(condition),
                                  std::move(then_branch),
                                  std::move(else_branch));
}

std::unique_ptr<Stmt> Parser::ParseWhileStatement() {
  const SourceLocation loc = current_.location;
  Advance();
  if (!Expect(TokenKind::LParen, "expected '(' after while")) {
    return nullptr;
  }
  auto condition = ParseExpression();
  if (!Expect(TokenKind::RParen, "expected ')' after condition")) {
    return nullptr;
  }
  auto body = ParseStatement();
  return std::make_unique<WhileStmt>(loc, std::move(condition),
                                     std::move(body));
}

std::unique_ptr<Stmt> Parser::ParseReturnStatement() {
  const SourceLocation loc = current_.location;
  Advance();
  std::unique_ptr<Expr> value;
  if (!Check(TokenKind::Semicolon)) {
    value = ParseExpression();
  }
  Expect(TokenKind::Semicolon, "expected ';' after return statement");
  return std::make_unique<ReturnStmt>(loc, std::move(value));
}

std::unique_ptr<Stmt> Parser::ParseVarDeclStatement() {
  const SourceLocation loc = current_.location;
  const Type type = ParseType();
  if (IsVoidType(type)) {
    diag_.ReportError(loc, "variables cannot have type void");
  }
  std::vector<std::unique_ptr<Stmt>> decls;

  while (true) {
    if (!Expect(TokenKind::Identifier, "expected variable name")) {
      Synchronize();
      return nullptr;
    }

    std::string name = previous_.lexeme;
    std::unique_ptr<Expr> init;
    if (Match(TokenKind::Equal)) {
      init = ParseExpression();
    }

    decls.push_back(std::make_unique<VarDeclStmt>(loc, std::move(name), type,
                                                  std::move(init)));

    if (!Match(TokenKind::Comma)) {
      break;
    }
  }

  if (!Expect(TokenKind::Semicolon, "expected ';' after declaration")) {
    Synchronize();
    return nullptr;
  }

  for (size_t i = 1; i < decls.size(); ++i) {
    pending_statements_.push_back(std::move(decls[i]));
  }

  return std::move(decls.front());
}

std::unique_ptr<Stmt> Parser::ParseExpressionStatement() {
  const SourceLocation loc = current_.location;
  if (Match(TokenKind::Semicolon)) {
    return std::make_unique<ExprStmt>(loc, nullptr);
  }
  auto expr = ParseExpression();
  Expect(TokenKind::Semicolon, "expected ';' after expression");
  return std::make_unique<ExprStmt>(loc, std::move(expr));
}

std::unique_ptr<Expr> Parser::ParseExpression() { return ParseAssignment(); }

std::unique_ptr<Expr> Parser::ParseAssignment() {
  auto lhs = ParseLogicalOr();
  if (!lhs) {
    return nullptr;
  }

  if (Match(TokenKind::Equal)) {
    const SourceLocation loc = previous_.location;
    auto rhs = ParseAssignment();
    auto *ident = dynamic_cast<IdentifierExpr *>(lhs.get());
    if (!ident) {
      diag_.ReportError(loc,
                        "left-hand side of assignment must be an identifier");
      return rhs;
    }
    return std::make_unique<AssignmentExpr>(loc, ident->name, std::move(rhs));
  }
  return lhs;
}

std::unique_ptr<Expr> Parser::ParseLogicalOr() {
  auto expr = ParseLogicalAnd();
  while (Match(TokenKind::OrOr)) {
    const SourceLocation loc = previous_.location;
    auto rhs = ParseLogicalAnd();
    expr = std::make_unique<BinaryExpr>(loc, BinaryOp::LogicalOr,
                                        std::move(expr), std::move(rhs));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::ParseLogicalAnd() {
  auto expr = ParseEquality();
  while (Match(TokenKind::AndAnd)) {
    const SourceLocation loc = previous_.location;
    auto rhs = ParseEquality();
    expr = std::make_unique<BinaryExpr>(loc, BinaryOp::LogicalAnd,
                                        std::move(expr), std::move(rhs));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::ParseEquality() {
  auto expr = ParseComparison();
  while (Check(TokenKind::EqualEqual) || Check(TokenKind::BangEqual)) {
    BinaryOp op = BinaryOp::Equal;
    if (Match(TokenKind::EqualEqual)) {
      op = BinaryOp::Equal;
    } else {
      Match(TokenKind::BangEqual);
      op = BinaryOp::NotEqual;
    }
    const SourceLocation loc = previous_.location;
    auto rhs = ParseComparison();
    expr =
        std::make_unique<BinaryExpr>(loc, op, std::move(expr), std::move(rhs));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::ParseComparison() {
  auto expr = ParseTerm();
  while (Check(TokenKind::Less) || Check(TokenKind::LessEqual) ||
         Check(TokenKind::Greater) || Check(TokenKind::GreaterEqual)) {
    BinaryOp op = BinaryOp::Less;
    if (Match(TokenKind::Less)) {
      op = BinaryOp::Less;
    } else if (Match(TokenKind::LessEqual)) {
      op = BinaryOp::LessEq;
    } else if (Match(TokenKind::Greater)) {
      op = BinaryOp::Greater;
    } else {
      Match(TokenKind::GreaterEqual);
      op = BinaryOp::GreaterEq;
    }
    const SourceLocation loc = previous_.location;
    auto rhs = ParseTerm();
    expr =
        std::make_unique<BinaryExpr>(loc, op, std::move(expr), std::move(rhs));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::ParseTerm() {
  auto expr = ParseFactor();
  while (Check(TokenKind::Plus) || Check(TokenKind::Minus)) {
    BinaryOp op = BinaryOp::Add;
    if (Match(TokenKind::Plus)) {
      op = BinaryOp::Add;
    } else {
      Match(TokenKind::Minus);
      op = BinaryOp::Sub;
    }
    const SourceLocation loc = previous_.location;
    auto rhs = ParseFactor();
    expr =
        std::make_unique<BinaryExpr>(loc, op, std::move(expr), std::move(rhs));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::ParseFactor() {
  auto expr = ParseUnary();
  while (Check(TokenKind::Star) || Check(TokenKind::Slash) ||
         Check(TokenKind::Percent)) {
    BinaryOp op = BinaryOp::Mul;
    if (Match(TokenKind::Star)) {
      op = BinaryOp::Mul;
    } else if (Match(TokenKind::Slash)) {
      op = BinaryOp::Div;
    } else {
      Match(TokenKind::Percent);
      op = BinaryOp::Mod;
    }
    const SourceLocation loc = previous_.location;
    auto rhs = ParseUnary();
    expr =
        std::make_unique<BinaryExpr>(loc, op, std::move(expr), std::move(rhs));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::ParseUnary() {
  if (Match(TokenKind::Minus)) {
    auto operand = ParseUnary();
    return std::make_unique<UnaryExpr>(previous_.location, UnaryOp::Negate,
                                       std::move(operand));
  }
  if (Match(TokenKind::Bang)) {
    auto operand = ParseUnary();
    return std::make_unique<UnaryExpr>(previous_.location, UnaryOp::LogicalNot,
                                       std::move(operand));
  }
  return ParsePrimary();
}

std::vector<std::unique_ptr<Expr>> Parser::ParseArguments() {
  std::vector<std::unique_ptr<Expr>> args;
  if (Check(TokenKind::RParen)) {
    return args;
  }
  while (true) {
    args.push_back(ParseExpression());
    if (!Match(TokenKind::Comma)) {
      break;
    }
  }
  return args;
}

std::unique_ptr<Expr> Parser::ParsePrimary() {
  if (Match(TokenKind::Number)) {
    if (previous_.is_floating) {
      return std::make_unique<FloatingLiteralExpr>(previous_.location,
                                                   previous_.double_value);
    }
    return std::make_unique<IntegerLiteralExpr>(previous_.location,
                                                previous_.int_value);
  }
  if (Match(TokenKind::Identifier)) {
    const SourceLocation loc = previous_.location;
    std::string name = previous_.lexeme;
    if (Match(TokenKind::LParen)) {
      auto args = ParseArguments();
      Expect(TokenKind::RParen, "expected ')' after argument list");
      return std::make_unique<CallExpr>(loc, std::move(name), std::move(args));
    }
    return std::make_unique<IdentifierExpr>(loc, std::move(name));
  }
  if (Match(TokenKind::String)) {
    return std::make_unique<StringLiteralExpr>(previous_.location,
                                               previous_.lexeme);
  }
  if (Match(TokenKind::LParen)) {
    if (IsTypeSpecifier(current_.kind)) {
      const SourceLocation cast_loc = previous_.location;
      const Type cast_type = ParseType();
      Expect(TokenKind::RParen, "expected ')' after cast type");
      auto operand = ParseUnary();
      return std::make_unique<CastExpr>(cast_loc, cast_type,
                                        std::move(operand));
    }
    auto expr = ParseExpression();
    Expect(TokenKind::RParen, "expected ')' after expression");
    return expr;
  }

  diag_.ReportError(current_.location,
                    std::string("expected expression, got '") +
                        TokenKindName(current_.kind) + "'");
  const SourceLocation loc = current_.location;
  if (!Check(TokenKind::EndOfFile)) {
    Advance();
  }
  return std::make_unique<IntegerLiteralExpr>(loc, 0);
}

} // namespace ccc
