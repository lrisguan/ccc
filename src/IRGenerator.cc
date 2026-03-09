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

#include "IRGenerator.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"

namespace ccc {

namespace {

std::optional<llvm::OptimizationLevel> ToOptLevel(unsigned level) {
  switch (level) {
  case 0:
    return llvm::OptimizationLevel::O0;
  case 1:
    return llvm::OptimizationLevel::O1;
  case 2:
    return llvm::OptimizationLevel::O2;
  case 3:
    return llvm::OptimizationLevel::O3;
  default:
    return std::nullopt;
  }
}

bool IsFloatingIRType(llvm::Type *type) {
  return type->isFloatTy() || type->isDoubleTy();
}

Type TypeFromLLVMType(llvm::Type *type) {
  if (type->isPointerTy()) {
    return Type{BaseType::Void, 1};
  }

  BaseType base = BaseType::Int;
  if (type->isVoidTy()) {
    base = BaseType::Void;
  } else if (type->isIntegerTy(8)) {
    base = BaseType::Char;
  } else if (type->isIntegerTy(32)) {
    base = BaseType::Int;
  } else if (type->isFloatTy()) {
    base = BaseType::Float;
  } else if (type->isDoubleTy()) {
    base = BaseType::Double;
  } else {
    base = BaseType::Int;
  }

  return Type{base, 0};
}

llvm::Value *ZeroValueForType(llvm::Type *type) {
  if (type->isPointerTy()) {
    return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(type));
  }
  if (type->isFloatTy()) {
    return llvm::ConstantFP::get(type, 0.0);
  }
  if (type->isDoubleTy()) {
    return llvm::ConstantFP::get(type, 0.0);
  }
  if (type->isIntegerTy()) {
    return llvm::ConstantInt::get(type, 0, true);
  }
  return llvm::UndefValue::get(type);
}

} // namespace

IRGenerator::IRGenerator(DiagnosticEngine &diag)
    : diag_(diag), context_(std::make_unique<llvm::LLVMContext>()),
      module_(std::make_unique<llvm::Module>("ccc_module", *context_)),
      builder_(std::make_unique<llvm::IRBuilder<>>(*context_)) {
  if (llvm::InitializeNativeTarget()) {
    diag_.ReportError(SourceLocation{1, 1},
                      "failed to initialize native LLVM target");
  }
  if (llvm::InitializeNativeTargetAsmPrinter()) {
    diag_.ReportError(SourceLocation{1, 1},
                      "failed to initialize native LLVM asm printer");
  }
}

IRGenerator::~IRGenerator() = default;

llvm::Type *IRGenerator::ToLLVMType(const Type &type) const {
  llvm::Type *base = nullptr;
  switch (type.base) {
  case BaseType::Int:
    base = llvm::Type::getInt32Ty(*context_);
    break;
  case BaseType::Char:
    base = llvm::Type::getInt8Ty(*context_);
    break;
  case BaseType::Float:
    base = llvm::Type::getFloatTy(*context_);
    break;
  case BaseType::Double:
    base = llvm::Type::getDoubleTy(*context_);
    break;
  case BaseType::Void:
    base = type.pointer_depth > 0 ? llvm::Type::getInt8Ty(*context_)
                                  : llvm::Type::getVoidTy(*context_);
    break;
  }

  for (unsigned i = 0; i < type.pointer_depth; ++i) {
    base = llvm::PointerType::getUnqual(base);
  }
  return base;
}

llvm::Value *IRGenerator::CastValueToType(llvm::Value *value, const Type &from,
                                          const Type &to) {
  if (!value || from == to) {
    return value;
  }

  llvm::Type *target = ToLLVMType(to);
  if (value->getType() == target) {
    return value;
  }

  if (IsPointerType(from) && IsPointerType(to)) {
    return builder_->CreateBitCast(value, target, "ptrcast");
  }

  if (IsNumericType(from) && IsNumericType(to)) {
    const bool from_fp = IsFloatingType(from);
    const bool to_fp = IsFloatingType(to);
    if (from_fp && to_fp) {
      if (from.base == BaseType::Float && to.base == BaseType::Double) {
        return builder_->CreateFPExt(value, target, "fpext");
      }
      if (from.base == BaseType::Double && to.base == BaseType::Float) {
        return builder_->CreateFPTrunc(value, target, "fptrunc");
      }
      return value;
    }
    if (!from_fp && to_fp) {
      return builder_->CreateSIToFP(value, target, "sitofp");
    }
    if (from_fp && !to_fp) {
      return builder_->CreateFPToSI(value, target, "fptosi");
    }

    const unsigned from_bits = value->getType()->getIntegerBitWidth();
    const unsigned to_bits = target->getIntegerBitWidth();
    if (from_bits < to_bits) {
      return builder_->CreateSExt(value, target, "sext");
    }
    if (from_bits > to_bits) {
      return builder_->CreateTrunc(value, target, "trunc");
    }
    return value;
  }

  return builder_->CreateBitCast(value, target, "bitcast");
}

bool IRGenerator::DeclareFunctions(const Program &program) {
  for (const auto &fn : program.functions) {
    llvm::Function *existing = module_->getFunction(fn->name);
    std::vector<llvm::Type *> arg_types;
    arg_types.reserve(fn->params.size());
    for (const auto &p : fn->params) {
      arg_types.push_back(ToLLVMType(p.type));
    }
    auto *ft = llvm::FunctionType::get(ToLLVMType(fn->return_type), arg_types,
                                       fn->is_variadic);

    if (existing) {
      if (existing->getFunctionType() != ft) {
        diag_.ReportError(
            fn->location,
            "conflicting function declaration in IR generation: '" + fn->name +
                "'");
        return false;
      }
      if (fn->HasBody() && !existing->empty()) {
        diag_.ReportError(fn->location,
                          "duplicate function definition in IR generation: '" +
                              fn->name + "'");
        return false;
      }

      size_t index = 0;
      for (auto &arg : existing->args()) {
        if (index < fn->params.size() && arg.getName().empty()) {
          arg.setName(fn->params[index].name);
        }
        ++index;
      }
      continue;
    }

    auto *f = llvm::Function::Create(ft, llvm::GlobalValue::ExternalLinkage,
                                     fn->name, module_.get());

    size_t index = 0;
    for (auto &arg : f->args()) {
      if (index < fn->params.size()) {
        arg.setName(fn->params[index].name);
      }
      ++index;
    }
  }
  return true;
}

void IRGenerator::EnterScope() { local_scopes_.push_back({}); }

void IRGenerator::ExitScope() {
  if (!local_scopes_.empty()) {
    local_scopes_.pop_back();
  }
}

bool IRGenerator::DeclareLocal(const std::string &name,
                               llvm::AllocaInst *alloca) {
  if (local_scopes_.empty()) {
    EnterScope();
  }
  auto &scope = local_scopes_.back();
  if (scope.find(name) != scope.end()) {
    return false;
  }
  scope.emplace(name, alloca);
  return true;
}

llvm::AllocaInst *IRGenerator::LookupLocal(const std::string &name) const {
  for (auto it = local_scopes_.rbegin(); it != local_scopes_.rend(); ++it) {
    const auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }
  return nullptr;
}

llvm::AllocaInst *IRGenerator::CreateEntryAlloca(llvm::Function *fn,
                                                 const std::string &name,
                                                 const Type &type) {
  llvm::IRBuilder<> entry_builder(&fn->getEntryBlock(),
                                  fn->getEntryBlock().begin());
  return entry_builder.CreateAlloca(ToLLVMType(type), nullptr, name);
}

llvm::Value *IRGenerator::EmitCondition(const Expr &expr) {
  llvm::Value *v = EmitExpr(expr);
  if (!v) {
    return nullptr;
  }
  llvm::Type *type = v->getType();
  if (type->isIntegerTy(1)) {
    return v;
  }
  if (type->isPointerTy()) {
    return builder_->CreateICmpNE(
        v, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(type)),
        "condtmp");
  }
  if (IsFloatingIRType(type)) {
    return builder_->CreateFCmpONE(v, llvm::ConstantFP::get(type, 0.0),
                                   "condtmp");
  }
  return builder_->CreateICmpNE(v, llvm::ConstantInt::get(type, 0), "condtmp");
}

llvm::Value *IRGenerator::EmitExpr(const Expr &expr) {
  if (const auto *e = dynamic_cast<const IntegerLiteralExpr *>(&expr)) {
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), e->value,
                                  true);
  }

  if (const auto *e = dynamic_cast<const FloatingLiteralExpr *>(&expr)) {
    return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), e->value);
  }

  if (const auto *e = dynamic_cast<const StringLiteralExpr *>(&expr)) {
    return builder_->CreateGlobalStringPtr(e->value, "strlit");
  }

  if (const auto *e = dynamic_cast<const IdentifierExpr *>(&expr)) {
    llvm::AllocaInst *slot = LookupLocal(e->name);
    if (!slot) {
      diag_.ReportError(e->location,
                        "unknown variable in codegen: '" + e->name + "'");
      return nullptr;
    }
    return builder_->CreateLoad(slot->getAllocatedType(), slot,
                                e->name + ".val");
  }

  if (const auto *e = dynamic_cast<const AssignmentExpr *>(&expr)) {
    llvm::AllocaInst *slot = LookupLocal(e->name);
    if (!slot) {
      diag_.ReportError(e->location,
                        "assignment to unknown variable in codegen: '" +
                            e->name + "'");
      return nullptr;
    }
    llvm::Value *rhs = EmitExpr(*e->value);
    if (!rhs) {
      return nullptr;
    }
    const Type from = TypeFromLLVMType(rhs->getType());
    const Type to = TypeFromLLVMType(slot->getAllocatedType());
    rhs = CastValueToType(rhs, from, to);
    builder_->CreateStore(rhs, slot);
    return rhs;
  }

  if (const auto *e = dynamic_cast<const UnaryExpr *>(&expr)) {
    llvm::Value *operand = EmitExpr(*e->operand);
    if (!operand) {
      return nullptr;
    }
    switch (e->op) {
    case UnaryOp::Negate:
      if (IsFloatingIRType(operand->getType())) {
        return builder_->CreateFNeg(operand, "fnegtmp");
      }
      return builder_->CreateNeg(operand, "negtmp");
    case UnaryOp::LogicalNot: {
      llvm::Value *cond = EmitCondition(*e->operand);
      if (!cond) {
        return nullptr;
      }
      llvm::Value *not_value = builder_->CreateNot(cond, "nottmp");
      return builder_->CreateZExt(not_value, llvm::Type::getInt32Ty(*context_),
                                  "booltoint");
    }
    }
  }

  if (const auto *e = dynamic_cast<const CastExpr *>(&expr)) {
    llvm::Value *operand = EmitExpr(*e->operand);
    if (!operand) {
      return nullptr;
    }
    const Type from = TypeFromLLVMType(operand->getType());
    return CastValueToType(operand, from, e->target_type);
  }

  if (const auto *e = dynamic_cast<const BinaryExpr *>(&expr)) {
    llvm::Value *lhs = EmitExpr(*e->lhs);
    llvm::Value *rhs = EmitExpr(*e->rhs);
    if (!lhs || !rhs) {
      return nullptr;
    }

    const Type left_type = TypeFromLLVMType(lhs->getType());
    const Type right_type = TypeFromLLVMType(rhs->getType());

    if (IsNumericType(left_type) && IsNumericType(right_type)) {
      const Type common = CommonNumericType(left_type, right_type);
      lhs = CastValueToType(lhs, left_type, common);
      rhs = CastValueToType(rhs, right_type, common);
      const bool fp = IsFloatingIRType(lhs->getType());

      switch (e->op) {
      case BinaryOp::Add:
        return fp ? static_cast<llvm::Value *>(
                        builder_->CreateFAdd(lhs, rhs, "addtmp"))
                  : static_cast<llvm::Value *>(
                        builder_->CreateAdd(lhs, rhs, "addtmp"));
      case BinaryOp::Sub:
        return fp ? static_cast<llvm::Value *>(
                        builder_->CreateFSub(lhs, rhs, "subtmp"))
                  : static_cast<llvm::Value *>(
                        builder_->CreateSub(lhs, rhs, "subtmp"));
      case BinaryOp::Mul:
        return fp ? static_cast<llvm::Value *>(
                        builder_->CreateFMul(lhs, rhs, "multmp"))
                  : static_cast<llvm::Value *>(
                        builder_->CreateMul(lhs, rhs, "multmp"));
      case BinaryOp::Div:
        return fp ? static_cast<llvm::Value *>(
                        builder_->CreateFDiv(lhs, rhs, "divtmp"))
                  : static_cast<llvm::Value *>(
                        builder_->CreateSDiv(lhs, rhs, "divtmp"));
      case BinaryOp::Mod:
        if (fp) {
          return builder_->CreateFRem(lhs, rhs, "modtmp");
        }
        return builder_->CreateSRem(lhs, rhs, "modtmp");
      case BinaryOp::Less:
        return builder_->CreateZExt(
            fp ? static_cast<llvm::Value *>(
                     builder_->CreateFCmpOLT(lhs, rhs, "lttmp"))
               : static_cast<llvm::Value *>(
                     builder_->CreateICmpSLT(lhs, rhs, "lttmp")),
            llvm::Type::getInt32Ty(*context_), "booltoint");
      case BinaryOp::LessEq:
        return builder_->CreateZExt(
            fp ? static_cast<llvm::Value *>(
                     builder_->CreateFCmpOLE(lhs, rhs, "letmp"))
               : static_cast<llvm::Value *>(
                     builder_->CreateICmpSLE(lhs, rhs, "letmp")),
            llvm::Type::getInt32Ty(*context_), "booltoint");
      case BinaryOp::Greater:
        return builder_->CreateZExt(
            fp ? static_cast<llvm::Value *>(
                     builder_->CreateFCmpOGT(lhs, rhs, "gttmp"))
               : static_cast<llvm::Value *>(
                     builder_->CreateICmpSGT(lhs, rhs, "gttmp")),
            llvm::Type::getInt32Ty(*context_), "booltoint");
      case BinaryOp::GreaterEq:
        return builder_->CreateZExt(
            fp ? static_cast<llvm::Value *>(
                     builder_->CreateFCmpOGE(lhs, rhs, "getmp"))
               : static_cast<llvm::Value *>(
                     builder_->CreateICmpSGE(lhs, rhs, "getmp")),
            llvm::Type::getInt32Ty(*context_), "booltoint");
      case BinaryOp::Equal:
        return builder_->CreateZExt(
            fp ? static_cast<llvm::Value *>(
                     builder_->CreateFCmpOEQ(lhs, rhs, "eqtmp"))
               : static_cast<llvm::Value *>(
                     builder_->CreateICmpEQ(lhs, rhs, "eqtmp")),
            llvm::Type::getInt32Ty(*context_), "booltoint");
      case BinaryOp::NotEqual:
        return builder_->CreateZExt(
            fp ? static_cast<llvm::Value *>(
                     builder_->CreateFCmpONE(lhs, rhs, "netmp"))
               : static_cast<llvm::Value *>(
                     builder_->CreateICmpNE(lhs, rhs, "netmp")),
            llvm::Type::getInt32Ty(*context_), "booltoint");
      case BinaryOp::LogicalAnd: {
        llvm::Value *lcond = builder_->CreateICmpNE(
            lhs,
            fp ? static_cast<llvm::Value *>(
                     llvm::ConstantFP::get(lhs->getType(), 0.0))
               : static_cast<llvm::Value *>(
                     llvm::ConstantInt::get(lhs->getType(), 0)),
            "and.lcond");
        llvm::Value *rcond = builder_->CreateICmpNE(
            rhs,
            fp ? static_cast<llvm::Value *>(
                     llvm::ConstantFP::get(rhs->getType(), 0.0))
               : static_cast<llvm::Value *>(
                     llvm::ConstantInt::get(rhs->getType(), 0)),
            "and.rcond");
        return builder_->CreateZExt(builder_->CreateAnd(lcond, rcond, "andtmp"),
                                    llvm::Type::getInt32Ty(*context_),
                                    "booltoint");
      }
      case BinaryOp::LogicalOr: {
        llvm::Value *lcond = builder_->CreateICmpNE(
            lhs,
            fp ? static_cast<llvm::Value *>(
                     llvm::ConstantFP::get(lhs->getType(), 0.0))
               : static_cast<llvm::Value *>(
                     llvm::ConstantInt::get(lhs->getType(), 0)),
            "or.lcond");
        llvm::Value *rcond = builder_->CreateICmpNE(
            rhs,
            fp ? static_cast<llvm::Value *>(
                     llvm::ConstantFP::get(rhs->getType(), 0.0))
               : static_cast<llvm::Value *>(
                     llvm::ConstantInt::get(rhs->getType(), 0)),
            "or.rcond");
        return builder_->CreateZExt(builder_->CreateOr(lcond, rcond, "ortmp"),
                                    llvm::Type::getInt32Ty(*context_),
                                    "booltoint");
      }
      }
    }

    if (lhs->getType()->isPointerTy() && rhs->getType()->isPointerTy()) {
      switch (e->op) {
      case BinaryOp::Equal:
        return builder_->CreateZExt(builder_->CreateICmpEQ(lhs, rhs, "peqtmp"),
                                    llvm::Type::getInt32Ty(*context_),
                                    "booltoint");
      case BinaryOp::NotEqual:
        return builder_->CreateZExt(builder_->CreateICmpNE(lhs, rhs, "pnetmp"),
                                    llvm::Type::getInt32Ty(*context_),
                                    "booltoint");
      case BinaryOp::LogicalAnd: {
        llvm::Value *lcond = builder_->CreateICmpNE(
            lhs,
            llvm::ConstantPointerNull::get(
                llvm::cast<llvm::PointerType>(lhs->getType())),
            "and.lcond");
        llvm::Value *rcond = builder_->CreateICmpNE(
            rhs,
            llvm::ConstantPointerNull::get(
                llvm::cast<llvm::PointerType>(rhs->getType())),
            "and.rcond");
        return builder_->CreateZExt(builder_->CreateAnd(lcond, rcond, "andtmp"),
                                    llvm::Type::getInt32Ty(*context_),
                                    "booltoint");
      }
      case BinaryOp::LogicalOr: {
        llvm::Value *lcond = builder_->CreateICmpNE(
            lhs,
            llvm::ConstantPointerNull::get(
                llvm::cast<llvm::PointerType>(lhs->getType())),
            "or.lcond");
        llvm::Value *rcond = builder_->CreateICmpNE(
            rhs,
            llvm::ConstantPointerNull::get(
                llvm::cast<llvm::PointerType>(rhs->getType())),
            "or.rcond");
        return builder_->CreateZExt(builder_->CreateOr(lcond, rcond, "ortmp"),
                                    llvm::Type::getInt32Ty(*context_),
                                    "booltoint");
      }
      default:
        break;
      }
    }

    if (lhs->getType()->isPointerTy() && rhs->getType()->isIntegerTy()) {
      switch (e->op) {
      case BinaryOp::Equal: {
        llvm::Value *rhs_ptr =
            builder_->CreateIntToPtr(rhs, lhs->getType(), "inttoptr");
        return builder_->CreateZExt(
            builder_->CreateICmpEQ(lhs, rhs_ptr, "peqtmp"),
            llvm::Type::getInt32Ty(*context_), "booltoint");
      }
      case BinaryOp::NotEqual: {
        llvm::Value *rhs_ptr =
            builder_->CreateIntToPtr(rhs, lhs->getType(), "inttoptr");
        return builder_->CreateZExt(
            builder_->CreateICmpNE(lhs, rhs_ptr, "pnetmp"),
            llvm::Type::getInt32Ty(*context_), "booltoint");
      }
      default:
        break;
      }
    }

    if (lhs->getType()->isIntegerTy() && rhs->getType()->isPointerTy()) {
      switch (e->op) {
      case BinaryOp::Equal: {
        llvm::Value *lhs_ptr =
            builder_->CreateIntToPtr(lhs, rhs->getType(), "inttoptr");
        return builder_->CreateZExt(
            builder_->CreateICmpEQ(lhs_ptr, rhs, "peqtmp"),
            llvm::Type::getInt32Ty(*context_), "booltoint");
      }
      case BinaryOp::NotEqual: {
        llvm::Value *lhs_ptr =
            builder_->CreateIntToPtr(lhs, rhs->getType(), "inttoptr");
        return builder_->CreateZExt(
            builder_->CreateICmpNE(lhs_ptr, rhs, "pnetmp"),
            llvm::Type::getInt32Ty(*context_), "booltoint");
      }
      default:
        break;
      }
    }

    diag_.ReportError(expr.location,
                      "unsupported operand types for binary expression");
    return nullptr;
  }

  if (const auto *e = dynamic_cast<const CallExpr *>(&expr)) {
    llvm::Function *callee = module_->getFunction(e->callee);
    if (!callee) {
      auto *ft =
          llvm::FunctionType::get(llvm::Type::getInt32Ty(*context_), {}, true);
      callee = llvm::Function::Create(ft, llvm::GlobalValue::ExternalLinkage,
                                      e->callee, module_.get());
    }
    if (!callee->isVarArg() && callee->arg_size() != e->args.size()) {
      diag_.ReportError(e->location, "argument count mismatch in call to '" +
                                         e->callee + "'");
      return nullptr;
    }

    std::vector<llvm::Value *> args;
    args.reserve(e->args.size());
    size_t idx = 0;
    for (const auto &arg_expr : e->args) {
      llvm::Value *arg = EmitExpr(*arg_expr);
      if (!arg) {
        return nullptr;
      }
      if (idx < callee->arg_size()) {
        llvm::Type *target = callee->getFunctionType()->getParamType(idx);
        const Type from = TypeFromLLVMType(arg->getType());
        const Type to = TypeFromLLVMType(target);
        arg = CastValueToType(arg, from, to);
      }
      args.push_back(arg);
      ++idx;
    }
    if (callee->getReturnType()->isVoidTy()) {
      builder_->CreateCall(callee, args);
      return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0);
    }
    return builder_->CreateCall(callee, args, "calltmp");
  }

  diag_.ReportError(expr.location,
                    "unsupported expression during IR generation");
  return nullptr;
}

bool IRGenerator::EmitStmt(const Stmt &stmt) {
  if (const auto *s = dynamic_cast<const CompoundStmt *>(&stmt)) {
    EnterScope();
    for (const auto &nested : s->statements) {
      if (!EmitStmt(*nested)) {
        ExitScope();
        return false;
      }
    }
    ExitScope();
    return true;
  }

  if (const auto *s = dynamic_cast<const VarDeclStmt *>(&stmt)) {
    llvm::AllocaInst *slot =
        CreateEntryAlloca(current_function_, s->name, s->type);
    if (!DeclareLocal(s->name, slot)) {
      diag_.ReportError(s->location,
                        "duplicate local in codegen scope: '" + s->name + "'");
      return false;
    }
    llvm::Value *init = ZeroValueForType(slot->getAllocatedType());
    if (s->initializer) {
      init = EmitExpr(*s->initializer);
      if (!init) {
        return false;
      }
      const Type from = TypeFromLLVMType(init->getType());
      const Type to = s->type;
      init = CastValueToType(init, from, to);
    }
    builder_->CreateStore(init, slot);
    return true;
  }

  if (const auto *s = dynamic_cast<const ExprStmt *>(&stmt)) {
    if (!s->expr) {
      return true;
    }
    return EmitExpr(*s->expr) != nullptr;
  }

  if (const auto *s = dynamic_cast<const ReturnStmt *>(&stmt)) {
    if (!s->value) {
      builder_->CreateRetVoid();
      return true;
    }
    llvm::Value *v = EmitExpr(*s->value);
    if (!v) {
      return false;
    }
    llvm::Type *ret_type = current_function_->getReturnType();
    if (v->getType() != ret_type) {
      const Type from = TypeFromLLVMType(v->getType());
      const Type to = TypeFromLLVMType(ret_type);
      v = CastValueToType(v, from, to);
    }
    builder_->CreateRet(v);
    return true;
  }

  if (const auto *s = dynamic_cast<const IfStmt *>(&stmt)) {
    llvm::Value *cond = EmitCondition(*s->condition);
    if (!cond) {
      return false;
    }

    llvm::Function *fn = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock *then_bb =
        llvm::BasicBlock::Create(*context_, "if.then", fn);
    llvm::BasicBlock *merge_bb =
        llvm::BasicBlock::Create(*context_, "if.end", fn);
    llvm::BasicBlock *else_bb =
        s->else_branch ? llvm::BasicBlock::Create(*context_, "if.else", fn)
                       : merge_bb;

    builder_->CreateCondBr(cond, then_bb, else_bb);

    builder_->SetInsertPoint(then_bb);
    if (!EmitStmt(*s->then_branch)) {
      return false;
    }
    if (!builder_->GetInsertBlock()->getTerminator()) {
      builder_->CreateBr(merge_bb);
    }

    if (s->else_branch) {
      builder_->SetInsertPoint(else_bb);
      if (!EmitStmt(*s->else_branch)) {
        return false;
      }
      if (!builder_->GetInsertBlock()->getTerminator()) {
        builder_->CreateBr(merge_bb);
      }
    }

    builder_->SetInsertPoint(merge_bb);
    return true;
  }

  if (const auto *s = dynamic_cast<const WhileStmt *>(&stmt)) {
    llvm::Function *fn = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock *cond_bb =
        llvm::BasicBlock::Create(*context_, "while.cond", fn);
    llvm::BasicBlock *body_bb =
        llvm::BasicBlock::Create(*context_, "while.body", fn);
    llvm::BasicBlock *end_bb =
        llvm::BasicBlock::Create(*context_, "while.end", fn);

    builder_->CreateBr(cond_bb);
    builder_->SetInsertPoint(cond_bb);
    llvm::Value *cond = EmitCondition(*s->condition);
    if (!cond) {
      return false;
    }
    builder_->CreateCondBr(cond, body_bb, end_bb);

    builder_->SetInsertPoint(body_bb);
    if (!EmitStmt(*s->body)) {
      return false;
    }
    if (!builder_->GetInsertBlock()->getTerminator()) {
      builder_->CreateBr(cond_bb);
    }

    builder_->SetInsertPoint(end_bb);
    return true;
  }

  diag_.ReportError(stmt.location,
                    "unsupported statement during IR generation");
  return false;
}

bool IRGenerator::EmitFunction(const FunctionDecl &fn_decl) {
  if (!fn_decl.body) {
    return true;
  }

  llvm::Function *fn = module_->getFunction(fn_decl.name);
  if (!fn) {
    diag_.ReportError(fn_decl.location,
                      "internal error: function missing in module: '" +
                          fn_decl.name + "'");
    return false;
  }

  current_function_ = fn;
  local_scopes_.clear();

  llvm::BasicBlock *entry = llvm::BasicBlock::Create(*context_, "entry", fn);
  builder_->SetInsertPoint(entry);

  EnterScope();
  size_t index = 0;
  for (auto &arg : fn->args()) {
    Type arg_type = TypeFromLLVMType(arg.getType());
    if (index < fn_decl.params.size()) {
      arg_type = fn_decl.params[index].type;
    }
    llvm::AllocaInst *slot =
        CreateEntryAlloca(fn, std::string(arg.getName()), arg_type);
    builder_->CreateStore(&arg, slot);
    DeclareLocal(std::string(arg.getName()), slot);
    ++index;
  }

  if (!EmitStmt(*fn_decl.body)) {
    ExitScope();
    return false;
  }

  if (!builder_->GetInsertBlock()->getTerminator()) {
    if (fn_decl.return_type.base == BaseType::Void &&
        fn_decl.return_type.pointer_depth == 0) {
      builder_->CreateRetVoid();
    } else {
      llvm::Type *ret_type = ToLLVMType(fn_decl.return_type);
      builder_->CreateRet(ZeroValueForType(ret_type));
    }
  }
  ExitScope();

  if (llvm::verifyFunction(*fn, &llvm::errs())) {
    diag_.ReportError(fn_decl.location,
                      "LLVM verification failed for function: '" +
                          fn_decl.name + "'");
    return false;
  }
  return true;
}

bool IRGenerator::GenerateModule(const Program &program,
                                 const std::string &module_name) {
  module_ = std::make_unique<llvm::Module>(module_name, *context_);

  if (!DeclareFunctions(program)) {
    return false;
  }

  for (const auto &fn : program.functions) {
    if (fn->HasBody() && !EmitFunction(*fn)) {
      return false;
    }
  }

  if (llvm::verifyModule(*module_, &llvm::errs())) {
    diag_.ReportError(SourceLocation{1, 1}, "LLVM module verification failed");
    return false;
  }
  return true;
}

bool IRGenerator::RunOptimizations(unsigned opt_level) {
  const auto level = ToOptLevel(opt_level);
  if (!level.has_value()) {
    diag_.ReportError(SourceLocation{1, 1}, "invalid optimization level");
    return false;
  }

  llvm::LoopAnalysisManager lam;
  llvm::FunctionAnalysisManager fam;
  llvm::CGSCCAnalysisManager cgam;
  llvm::ModuleAnalysisManager mam;
  llvm::PassBuilder pb;

  pb.registerModuleAnalyses(mam);
  pb.registerCGSCCAnalyses(cgam);
  pb.registerFunctionAnalyses(fam);
  pb.registerLoopAnalyses(lam);
  pb.crossRegisterProxies(lam, fam, cgam, mam);

  llvm::ModulePassManager mpm = pb.buildPerModuleDefaultPipeline(*level);
  mpm.run(*module_, mam);
  return true;
}

bool IRGenerator::EmitIRFile(const std::string &path) const {
  std::error_code ec;
  llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_None);
  if (ec) {
    return false;
  }
  module_->print(out, nullptr);
  return true;
}

bool IRGenerator::EmitObjectFile(const std::string &path) const {
  const std::string target_triple = llvm::sys::getDefaultTargetTriple();
  module_->setTargetTriple(target_triple);

  std::string target_error;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(target_triple, target_error);
  if (!target) {
    diag_.ReportError(SourceLocation{1, 1},
                      "failed to lookup LLVM target: " + target_error);
    return false;
  }

  llvm::TargetOptions options;
  std::unique_ptr<llvm::TargetMachine> target_machine(
      target->createTargetMachine(target_triple, "generic", "", options,
                                  std::nullopt));
  if (!target_machine) {
    diag_.ReportError(SourceLocation{1, 1},
                      "failed to create LLVM target machine");
    return false;
  }

  module_->setDataLayout(target_machine->createDataLayout());

  std::error_code ec;
  llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_None);
  if (ec) {
    diag_.ReportError(SourceLocation{1, 1},
                      "failed to open object file output: " + path);
    return false;
  }

  llvm::legacy::PassManager pass;
  if (target_machine->addPassesToEmitFile(pass, dest, nullptr,
                                          llvm::CodeGenFileType::ObjectFile)) {
    diag_.ReportError(SourceLocation{1, 1},
                      "LLVM target does not support object file emission");
    return false;
  }

  pass.run(*module_);
  dest.flush();
  return true;
}

} // namespace ccc
