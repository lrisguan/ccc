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

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "AST.h"
#include "error.h"
#include "llvm/IR/IRBuilder.h"

namespace llvm {
class LLVMContext;
class Module;
class Value;
class Function;
class Type;
class AllocaInst;
class StructType;
} // namespace llvm

namespace ccc {

class IRGenerator {
public:
  explicit IRGenerator(DiagnosticEngine &diag);
  ~IRGenerator();

  bool GenerateModule(const Program &program, const std::string &module_name);
  bool RunOptimizations(unsigned opt_level);
  bool EmitIRFile(const std::string &path) const;
  bool EmitObjectFile(const std::string &path) const;

private:
  struct LocalBinding {
    llvm::AllocaInst *alloca = nullptr;
    Type type;
  };

  struct LValue {
    llvm::Value *ptr = nullptr;
    Type type;
  };

  llvm::Type *ToLLVMType(const Type &type) const;
  std::string TagKey(UserTypeKind kind, const std::string &tag) const;
  const TagTypeDecl *LookupTagType(const Type &type) const;
  Type InferExprType(const Expr &expr) const;
  LValue EmitLValue(const Expr &expr);
  llvm::Value *CastValueToType(llvm::Value *value, const Type &from,
                               const Type &to);
  bool DeclareFunctions(const Program &program);
  bool EmitFunction(const FunctionDecl &fn_decl);

  void EnterScope();
  void ExitScope();
  bool DeclareLocal(const std::string &name, llvm::AllocaInst *alloca,
							const Type &type);
  const LocalBinding *LookupLocal(const std::string &name) const;

  llvm::AllocaInst *CreateEntryAlloca(llvm::Function *fn,
                                      const std::string &name,
                                      const Type &type);
  bool EmitStmt(const Stmt &stmt);
  llvm::Value *EmitExpr(const Expr &expr);
  llvm::Value *EmitCondition(const Expr &expr);

  DiagnosticEngine &diag_;
  std::unique_ptr<llvm::LLVMContext> context_;
  std::unique_ptr<llvm::Module> module_;
  std::unique_ptr<llvm::IRBuilder<>> builder_;
  llvm::Function *current_function_ = nullptr;
  std::vector<std::unordered_map<std::string, LocalBinding>> local_scopes_;
  std::unordered_map<std::string, TagTypeDecl> tag_types_;
  mutable std::unordered_map<std::string, llvm::StructType *> struct_types_;
};

} // namespace ccc
