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

#include "Parser.h"

#include <algorithm>
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
         kind == TokenKind::KwVoid || kind == TokenKind::KwStruct ||
         kind == TokenKind::KwUnion || kind == TokenKind::KwEnum;
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
    case TokenKind::KwFor:
    case TokenKind::KwReturn:
    case TokenKind::KwInt:
    case TokenKind::KwChar:
    case TokenKind::KwFloat:
    case TokenKind::KwDouble:
    case TokenKind::KwVoid:
    case TokenKind::KwStruct:
    case TokenKind::KwUnion:
    case TokenKind::KwEnum:
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
  Type type = ParseTypeSpecifier();

  while (Match(TokenKind::Star)) {
    ++type.pointer_depth;
  }

  return type;
}

Type Parser::ParseTypeSpecifier() {
  Type type;
  if (Match(TokenKind::KwInt)) {
    type.base = BaseType::Int;
    return type;
  }
  if (Match(TokenKind::KwChar)) {
    type.base = BaseType::Char;
    return type;
  }
  if (Match(TokenKind::KwFloat)) {
    type.base = BaseType::Float;
    return type;
  }
  if (Match(TokenKind::KwDouble)) {
    type.base = BaseType::Double;
    return type;
  }
  if (Match(TokenKind::KwVoid)) {
    type.base = BaseType::Void;
    return type;
  }

  if (Match(TokenKind::KwStruct) || Match(TokenKind::KwUnion) ||
      Match(TokenKind::KwEnum)) {
    const TokenKind kind = previous_.kind;
    if (kind == TokenKind::KwStruct) {
      type.user_kind = UserTypeKind::Struct;
    } else if (kind == TokenKind::KwUnion) {
      type.user_kind = UserTypeKind::Union;
    } else {
      type.user_kind = UserTypeKind::Enum;
    }

    if (Match(TokenKind::Identifier)) {
      type.user_tag = previous_.lexeme;
    }
    if (Check(TokenKind::LBrace)) {
      std::vector<TagMemberDecl> members;
      if (type.user_kind == UserTypeKind::Enum) {
        if (Expect(TokenKind::LBrace, "expected '{' in enum definition")) {
          while (!Check(TokenKind::RBrace) && !Check(TokenKind::EndOfFile)) {
            if (!Match(TokenKind::Identifier)) {
              diag_.ReportError(current_.location,
                                "expected enumerator identifier");
              Advance();
              continue;
            }
            if (Match(TokenKind::Equal)) {
              ParseExpression();
            }
            Match(TokenKind::Comma);
          }
          Expect(TokenKind::RBrace, "expected '}' to close enum definition");
        }
      } else {
        members = ParseTagDefinitionBody();
      }
      if (type.user_tag.empty()) {
        type.user_tag = "__anon_" + std::to_string(parsed_tag_types_.size() + 1);
      }
      RegisterTagTypeDecl(type.user_kind, type.user_tag, members);
    }
    return type;
  }

  diag_.ReportError(current_.location, "expected type specifier");
  type.base = BaseType::Int;
  return type;
}

std::vector<TagMemberDecl> Parser::ParseTagDefinitionBody() {
  std::vector<TagMemberDecl> members;
  if (!Expect(TokenKind::LBrace, "expected '{' in tag definition")) {
    return members;
  }

  while (!Check(TokenKind::RBrace) && !Check(TokenKind::EndOfFile)) {
    if (!IsTypeSpecifier(current_.kind)) {
      diag_.ReportError(current_.location,
                        "expected member declaration in tag definition");
      Advance();
      continue;
    }

    const Type base = ParseTypeSpecifier();
    if (Check(TokenKind::Semicolon)) {
      Advance();
      continue;
    }

    while (true) {
      auto decl = ParseDeclarator(base, true, false);
      if (decl.name.empty()) {
        break;
      }
      members.push_back(TagMemberDecl{decl.name, decl.type});
      if (!Match(TokenKind::Comma)) {
        break;
      }
    }
    Expect(TokenKind::Semicolon,
           "expected ';' after member declaration in tag definition");
  }

  Expect(TokenKind::RBrace, "expected '}' to close tag definition");
  return members;
}

void Parser::RegisterTagTypeDecl(UserTypeKind kind, const std::string &tag,
                                 const std::vector<TagMemberDecl> &members) {
  if (tag.empty() || kind == UserTypeKind::None) {
    return;
  }
  auto found = std::find_if(parsed_tag_types_.begin(), parsed_tag_types_.end(),
                            [&](const TagTypeDecl &decl) {
                              return decl.kind == kind && decl.tag == tag;
                            });
  if (found != parsed_tag_types_.end()) {
    found->members = members;
    return;
  }
  TagTypeDecl decl;
  decl.kind = kind;
  decl.tag = tag;
  decl.members = members;
  parsed_tag_types_.push_back(std::move(decl));
}

Parser::ParsedDeclarator Parser::ParseDeclarator(const Type &base_type,
                                                 bool require_identifier,
                                                 bool allow_function_suffix) {
  ParsedDeclarator decl;
  decl.type = base_type;

  unsigned leading_ptr = 0;
  while (Match(TokenKind::Star)) {
    ++leading_ptr;
  }

  if (Match(TokenKind::LParen)) {
    if (Match(TokenKind::Star)) {
      unsigned fn_ptr_depth = 1;
      while (Match(TokenKind::Star)) {
        ++fn_ptr_depth;
      }
      if (!Expect(TokenKind::Identifier,
                  "expected identifier in function pointer declarator")) {
        decl.type.base = BaseType::Int;
        return decl;
      }
      decl.name = previous_.lexeme;
      if (!Expect(TokenKind::RParen,
                  "expected ')' after function pointer name")) {
        return decl;
      }
      if (!Expect(TokenKind::LParen,
                  "expected '(' after function pointer declarator")) {
        return decl;
      }
      bool is_variadic = false;
      auto params = ParseParameterList(is_variadic);
      Expect(TokenKind::RParen, "expected ')' after parameter list");

      Type return_type = decl.type;
      return_type.pointer_depth += leading_ptr;

      decl.type = Type{};
      decl.type.base = BaseType::Void;
      decl.type.pointer_depth = fn_ptr_depth;
      decl.type.function_pointer = std::make_shared<FunctionPointerSignature>();
      decl.type.function_pointer->return_type = return_type;
      for (const auto &p : params) {
        decl.type.function_pointer->param_types.push_back(p.type);
      }
      decl.type.function_pointer->is_variadic = is_variadic;
      return decl;
    }

    diag_.ReportError(previous_.location,
                      "unsupported parenthesized declarator form");
    while (!Check(TokenKind::RParen) && !Check(TokenKind::EndOfFile)) {
      Advance();
    }
    Expect(TokenKind::RParen, "expected ')' to close declarator");
  }

  if (decl.name.empty()) {
    if (Match(TokenKind::Identifier)) {
      decl.name = previous_.lexeme;
    } else if (require_identifier) {
      diag_.ReportError(current_.location, "expected identifier");
    }
  }

  decl.type.pointer_depth += leading_ptr;

  while (Match(TokenKind::LBracket)) {
    unsigned dim = 0;
    if (Match(TokenKind::Number)) {
      if (previous_.is_floating || previous_.int_value <= 0) {
        diag_.ReportError(previous_.location,
                          "array dimension must be a positive integer");
        dim = 1;
      } else {
        dim = static_cast<unsigned>(previous_.int_value);
      }
    } else {
      diag_.ReportError(current_.location,
                        "expected array dimension size literal");
      dim = 1;
    }
    Expect(TokenKind::RBracket, "expected ']' after array dimension");
    decl.type.array_dimensions.push_back(dim);
  }

  if (allow_function_suffix && Match(TokenKind::LParen)) {
    decl.is_function = true;
    bool is_variadic = false;
    decl.function_params = ParseParameterList(is_variadic);
    decl.function_is_variadic = is_variadic;
    Expect(TokenKind::RParen, "expected ')' after parameters");
  }

  return decl;
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
  program->tag_types = parsed_tag_types_;
  return program;
}

std::unique_ptr<FunctionDecl> Parser::ParseFunction() {
  const SourceLocation start = current_.location;
  const bool is_extern = Match(TokenKind::KwExtern);
  const Type return_spec = ParseTypeSpecifier();
  auto decl = ParseDeclarator(return_spec, true, true);
  if (!decl.is_function) {
    diag_.ReportError(start, "top-level declarations must be functions");
    return nullptr;
  }
  if (IsArrayType(decl.type)) {
    diag_.ReportError(start, "function return type cannot be an array");
    return nullptr;
  }
  std::string fn_name = decl.name;
  Type return_type = decl.type;
  auto params = decl.function_params;

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

  return std::make_unique<FunctionDecl>(start, std::move(fn_name), return_type,
                                        std::move(params), std::move(body),
                                        is_extern, decl.function_is_variadic);
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
    const Type base = ParseTypeSpecifier();
    if (params.empty() && IsVoidType(base) && Check(TokenKind::RParen)) {
      return params;
    }
    auto decl = ParseDeclarator(base, true, false);
    if (decl.name.empty()) {
      break;
    }
    Type param_type = decl.type;
    if (!param_type.array_dimensions.empty()) {
      param_type.array_dimensions.erase(param_type.array_dimensions.begin());
      ++param_type.pointer_depth;
    }
    params.push_back(ParamDecl{loc, decl.name, param_type});
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
  if (Check(TokenKind::KwFor)) {
    return ParseForStatement();
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

std::unique_ptr<Stmt> Parser::ParseForInitStatement() {
  const SourceLocation loc = current_.location;
  if (IsTypeSpecifier(current_.kind)) {
    const Type base = ParseTypeSpecifier();
    std::vector<std::unique_ptr<Stmt>> decls;
    while (true) {
      auto decl = ParseDeclarator(base, true, false);
      if (decl.name.empty()) {
        break;
      }
      if (IsVoidType(decl.type)) {
        diag_.ReportError(loc, "variables cannot have type void");
      }

      std::unique_ptr<Expr> init;
      if (Match(TokenKind::Equal)) {
        init = ParseExpression();
      }
      decls.push_back(std::make_unique<VarDeclStmt>(
          loc, std::move(decl.name), decl.type, std::move(init)));

      if (!Match(TokenKind::Comma)) {
        break;
      }
    }

    if (decls.empty()) {
      return nullptr;
    }
    if (decls.size() == 1) {
      return std::move(decls.front());
    }

    auto block = std::make_unique<CompoundStmt>(loc);
    for (auto &decl : decls) {
      block->statements.push_back(std::move(decl));
    }
    return block;
  }

  auto expr = ParseExpression();
  return std::make_unique<ExprStmt>(loc, std::move(expr));
}

std::unique_ptr<Stmt> Parser::ParseForStatement() {
  const SourceLocation loc = current_.location;
  Advance();
  if (!Expect(TokenKind::LParen, "expected '(' after for")) {
    return nullptr;
  }

  std::unique_ptr<Stmt> init;
  if (!Check(TokenKind::Semicolon)) {
    init = ParseForInitStatement();
  }
  if (!Expect(TokenKind::Semicolon, "expected ';' after for init")) {
    return nullptr;
  }

  std::unique_ptr<Expr> condition;
  if (!Check(TokenKind::Semicolon)) {
    condition = ParseExpression();
  }
  if (!Expect(TokenKind::Semicolon, "expected ';' after for condition")) {
    return nullptr;
  }

  std::unique_ptr<Expr> increment;
  if (!Check(TokenKind::RParen)) {
    increment = ParseExpression();
  }
  if (!Expect(TokenKind::RParen, "expected ')' after for clauses")) {
    return nullptr;
  }

  auto body = ParseStatement();
  return std::make_unique<ForStmt>(loc, std::move(init), std::move(condition),
                                   std::move(increment), std::move(body));
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
  const Type base = ParseTypeSpecifier();

  if (Check(TokenKind::Semicolon)) {
    Advance();
    return std::make_unique<ExprStmt>(loc, nullptr);
  }

  std::vector<std::unique_ptr<Stmt>> decls;

  while (true) {
    auto decl = ParseDeclarator(base, true, false);
    if (decl.name.empty()) {
      Synchronize();
      return nullptr;
    }
    if (IsVoidType(decl.type)) {
      diag_.ReportError(loc, "variables cannot have type void");
    }

    std::unique_ptr<Expr> init;
    if (Match(TokenKind::Equal)) {
      init = ParseExpression();
    }

    decls.push_back(std::make_unique<VarDeclStmt>(
        loc, std::move(decl.name), decl.type, std::move(init)));

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
    return std::make_unique<AssignmentExpr>(loc, std::move(lhs),
                                            std::move(rhs));
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
  return ParsePostfix();
}

std::unique_ptr<Expr> Parser::ParsePostfix() {
  auto expr = ParsePrimary();
  while (true) {
    if (Match(TokenKind::Dot) || Match(TokenKind::Arrow)) {
      const bool via_pointer = previous_.kind == TokenKind::Arrow;
      const SourceLocation loc = previous_.location;
      if (!Expect(TokenKind::Identifier,
                  "expected member name after '.' or '->'")) {
        return expr;
      }
      expr = std::make_unique<MemberExpr>(loc, std::move(expr),
                                          previous_.lexeme, via_pointer);
      continue;
    }
    break;
  }
  return expr;
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
