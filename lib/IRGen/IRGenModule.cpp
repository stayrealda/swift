//===--- IRGenModule.cpp - Swift Global LLVM IR Generation ----------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
//  This file implements IR generation for global declarations in Swift.
//
//===----------------------------------------------------------------------===//

#include "swift/AST/Decl.h"
#include "swift/AST/Expr.h"
#include "swift/AST/Stmt.h"
#include "swift/AST/Type.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/GlobalValue.h"
#include "llvm/Module.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/Support/ErrorHandling.h"

#include "GenType.h"
#include "IRGenModule.h"

using namespace swift;
using namespace irgen;

IRGenModule::IRGenModule(ASTContext &Context, Options &Opts,
                         llvm::Module &Module,
                         const llvm::TargetData &TargetData)
  : Context(Context), Opts(Opts),
    Module(Module), LLVMContext(Module.getContext()),
    TargetData(TargetData), Types(*new TypeConverter()) {
  Int1Ty = llvm::Type::getInt1Ty(getLLVMContext());
  Int8Ty = llvm::Type::getInt8Ty(getLLVMContext());
  Int16Ty = llvm::Type::getInt16Ty(getLLVMContext());
  Int32Ty = llvm::Type::getInt32Ty(getLLVMContext());
  Int64Ty = llvm::Type::getInt64Ty(getLLVMContext());
}

IRGenModule::~IRGenModule() {
  delete &Types;
}

namespace {
  struct LinkInfo {
    std::string Name;
    llvm::GlobalValue::LinkageTypes Linkage;
    llvm::GlobalValue::VisibilityTypes Visibility;
  };
}

static LinkInfo computeLinkInfo(NamedDecl *D) {
  LinkInfo Link;

  // TODO, obviously.

  Link.Name = D->Name.str();
  Link.Linkage = llvm::GlobalValue::ExternalLinkage;
  Link.Visibility = llvm::GlobalValue::DefaultVisibility;

  return Link;
}

/// Emit a top-level context.
void IRGenModule::emitTopLevel(BraceStmt *Body) {
  for (unsigned I = 0, E = Body->NumElements; I != E; ++I) {
    BraceStmt::ExprStmtOrDecl Elt = Body->Elements[I];
    if (Decl *D = Elt.dyn_cast<Decl*>())
      emitGlobalDecl(D);
    // TODO: handle top-level statements and expressions.
  }
}

/// Emit all the top-level code in the translation unit.
void IRGenModule::emitTranslationUnit(TranslationUnitDecl *TU) {
  // The semantics of the top-level BraceStmt are a bit different from
  // the normal semantics.
  emitTopLevel(TU->Body);
}

/// Emit a global declaration.
void IRGenModule::emitGlobalDecl(Decl *D) {
  switch (D->Kind) {
  case DeclKind::TranslationUnit:
  case DeclKind::OneOfElement:
  case DeclKind::Arg:
  case DeclKind::ElementRef:
    llvm_unreachable("cannot encounter this decl here");
    break;
  
  // These declarations don't require IR-gen support.
  case DeclKind::Import:
  case DeclKind::TypeAlias:
    break;

  case DeclKind::Var:
    emitGlobalVariable(cast<VarDecl>(D));
    break;

  case DeclKind::Func:
    emitGlobalFunction(cast<FuncDecl>(D));
    break;
  }
}

/// Emit a global variable declaration.
llvm::GlobalVariable *IRGenModule::getAddrOfGlobalVariable(VarDecl *VD) {
  // Check whether we've cached this.
  llvm::Constant *&Entry = Globals[VD];
  if (Entry) return cast<llvm::GlobalVariable>(Entry);

  const TypeInfo &TInfo = getFragileTypeInfo(VD->Ty);
  LinkInfo Link = computeLinkInfo(VD);
  llvm::GlobalVariable *Addr
    = new llvm::GlobalVariable(Module, TInfo.Type, /*constant*/ false,
                               Link.Linkage, /*initializer*/ nullptr,
                               Link.Name);
  Addr->setVisibility(Link.Visibility);
  Addr->setAlignment(TInfo.TypeAlignment.getValue());

  Entry = Addr;
  return Addr;
}

/// Emit a global declaration.
void IRGenModule::emitGlobalVariable(VarDecl *VD) {
  llvm::GlobalVariable *Addr = getAddrOfGlobalVariable(VD);

  // For now, always give globals null initializers.
  llvm::Constant *Init =
    llvm::Constant::getNullValue(cast<llvm::PointerType>(Addr->getType())
                                   ->getElementType());
  Addr->setInitializer(Init);

  // FIXME: initializer
  (void) Addr;
}


llvm::Function *IRGenModule::getAddrOfGlobalFunction(FuncDecl *FD) {
  // Check whether we've cached this.
  llvm::Constant *&Entry = Globals[FD];
  if (Entry) return cast<llvm::Function>(Entry);

  llvm::FunctionType *Type = cast<llvm::FunctionType>(getFragileType(FD->Ty));

  LinkInfo Link = computeLinkInfo(FD);
  llvm::Function *Addr
    = cast<llvm::Function>(Module.getOrInsertFunction(Link.Name, Type));
  Addr->setLinkage(Link.Linkage);
  Addr->setVisibility(Link.Visibility);

  Entry = Addr;
  return Addr;
}

void IRGenModule::emitGlobalFunction(FuncDecl *FD) {
  llvm::Function *Addr = getAddrOfGlobalFunction(FD);
  // FIXME: body
  (void) Addr;
}
