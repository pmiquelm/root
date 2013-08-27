// @(#)root/core/meta:$Id$
// Author: Paul Russo   30/07/2012
// Author: Vassil Vassilev   9/02/2013

/*************************************************************************
 * Copyright (C) 1995-2013, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// TClingCallFunc                                                       //
//                                                                      //
// Emulation of the CINT CallFunc class.                                //
//                                                                      //
// The CINT C++ interpreter provides an interface for calling           //
// functions through the generated wrappers in dictionaries with        //
// the CallFunc class. This class provides the same functionality,      //
// using an interface as close as possible to CallFunc but the          //
// function metadata and calling service comes from the Cling           //
// C++ interpreter and the Clang C++ compiler, not CINT.                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "TClingCallFunc.h"

#include "TClingClassInfo.h"
#include "TClingMethodInfo.h"
#include "TInterpreterValue.h"

#include "TError.h"
#include "TCling.h"

#include "cling/Interpreter/CompilationOptions.h"
#include "cling/Interpreter/Interpreter.h"
#include "cling/Interpreter/LookupHelper.h"
#include "cling/Interpreter/StoredValueRef.h"
#include "cling/Interpreter/Transaction.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/Type.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/Lookup.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"

#include "clang/Sema/SemaInternal.h"

#include <string>
#include <vector>

using namespace ROOT;

// This ought to be declared by the implementer .. oh well...
extern void unresolvedSymbol();

cling::StoredValueRef TClingCallFunc::EvaluateExpr(const clang::Expr* E) const
{
   // Evaluate an Expr* and return its cling::StoredValueRef
   cling::StoredValueRef valref;

   using namespace clang;
   ASTContext& C = fInterp->getSema().getASTContext();
   llvm::APSInt res;
   if (E->EvaluateAsInt(res, C, /*AllowSideEffects*/Expr::SE_NoSideEffects)) {
      llvm::GenericValue gv;
      gv.IntVal = res;
      cling::Value val(gv, C.IntTy, fInterp->getLLVMType(C.IntTy));
      return cling::StoredValueRef::bitwiseCopy(C, val);
   }

   // TODO: Build a wrapper around the expression to avoid decompilation and 
   // compilation and other string operations.
   cling::Interpreter::CompilationResult cr 
      = fInterp->evaluate(ExprToString(E), valref);
   if (cr == cling::Interpreter::kSuccess)
      return valref;
   return cling::StoredValueRef::invalidValue();
}

void TClingCallFunc::Exec(void *address, TInterpreterValue* interpVal /* =0 */) const
{
   if (!IsValid()) {
      Error("TClingCallFunc::Exec", "Attempt to execute while invalid.");
      return;
   }
}

Long_t TClingCallFunc::ExecInt(void *address) const
{
   if (!IsValid()) {
      Error("TClingCallFunc::ExecInt", "Attempt to execute while invalid.");
      return 0L;
   }
}

long long TClingCallFunc::ExecInt64(void *address) const
{
   if (!IsValid()) {
      Error("TClingCallFunc::ExecInt64", "Attempt to execute while invalid.");
      return 0LL;
   }
}

double TClingCallFunc::ExecDouble(void *address) const
{
   if (!IsValid()) {
      Error("TClingCallFunc::ExecDouble", "Attempt to execute while invalid.");
      return 0.0;
   }
}

TClingMethodInfo *TClingCallFunc::FactoryMethod() const
{
   return new TClingMethodInfo(*fMethod);
}

void TClingCallFunc::Init()
{
   delete fMethod;
   fMethod = 0;
   ResetArg();
}

void *TClingCallFunc::InterfaceMethod() const
{
   if (!IsValid()) {
      return 0;
   }
   return const_cast<void*>(fMethod->GetMethodDecl());
}

bool TClingCallFunc::IsValid() const
{
   return false;
}

void TClingCallFunc::ResetArg()
{
   fArgVals.clear();
   PreallocatePtrs();
}

void TClingCallFunc::SetArg(long param)
{
   clang::ASTContext& C = fInterp->getSema().getASTContext();
   llvm::GenericValue gv;
   clang::QualType QT = C.LongTy;
   gv.IntVal = llvm::APInt(C.getTypeSize(QT), param);
   PushArg(cling::Value(gv, QT, fInterp->getLLVMType(QT)));
}

void TClingCallFunc::SetArg(double param)
{
   clang::ASTContext& C = fInterp->getSema().getASTContext();
   llvm::GenericValue gv;
   clang::QualType QT = C.DoubleTy;
   gv.DoubleVal = param;
   PushArg(cling::Value(gv, QT, fInterp->getLLVMType(QT)));
}

void TClingCallFunc::SetArg(long long param)
{
   clang::ASTContext& C = fInterp->getSema().getASTContext();
   llvm::GenericValue gv;
   clang::QualType QT = C.LongLongTy;
   gv.IntVal = llvm::APInt(C.getTypeSize(QT), param);
   PushArg(cling::Value(gv, QT, fInterp->getLLVMType(QT)));
}

void TClingCallFunc::SetArg(unsigned long long param)
{
   clang::ASTContext& C = fInterp->getSema().getASTContext();
   llvm::GenericValue gv;
   clang::QualType QT = C.UnsignedLongLongTy;
   gv.IntVal = llvm::APInt(C.getTypeSize(QT), param);
   PushArg(cling::Value(gv, QT, fInterp->getLLVMType(QT)));
}

void TClingCallFunc::SetArgArray(long *paramArr, int nparam)
{
   ResetArg();
   for (int i = 0; i < nparam; ++i) {
      SetArg(paramArr[i]);
   }
}

void TClingCallFunc::EvaluateArgList(const std::string &ArgList)
{
   ResetArg();
   llvm::SmallVector<clang::Expr*, 4> exprs;
   fInterp->getLookupHelper().findArgList(ArgList, exprs);
   for (llvm::SmallVector<clang::Expr*, 4>::const_iterator I = exprs.begin(),
         E = exprs.end(); I != E; ++I) {
      cling::StoredValueRef val = EvaluateExpr(*I);
      if (!val.isValid()) {
         // Bad expression, all done.
         break;
      }
      PushArg(val);
   }
}

void TClingCallFunc::SetArgs(const char *params)
{
   ResetArg();
   EvaluateArgList(params);
}

void TClingCallFunc::SetFunc(const TClingClassInfo* info, const char* method,
                             const char* arglist, long* poffset)
{
   SetFunc(info,method,arglist,false,poffset);
}

void TClingCallFunc::SetFunc(const TClingClassInfo* info,
                             const char* method,
                             const char* arglist,  bool objectIsConst, 
                             long* poffset)
{
   delete fMethod;
   fMethod = new TClingMethodInfo(fInterp);
   if (poffset) {
      *poffset = 0L;
   }
   ResetArg();
   if (!info->IsValid()) {
      Error("TClingCallFunc::SetFunc", "Class info is invalid!");
      return;
   }
   if (!strcmp(arglist, ")")) {
      // CINT accepted a single right paren as meaning no arguments.
      arglist = "";
   }
   *fMethod = info->GetMethodWithArgs(method, arglist, objectIsConst, poffset);
   if (!fMethod->IsValid()) {
      //Error("TClingCallFunc::SetFunc", "Could not find method %s(%s)", method,
      //      arglist);
      return;
   }
   // FIXME: The arglist was already parsed by the lookup, we should
   //        enhance the lookup to return the resulting expression
   //        list so we do not need to parse it again here.
   EvaluateArgList(arglist);
}

void TClingCallFunc::SetFunc(const TClingMethodInfo *info)
{
   delete fMethod;
   fMethod = 0;
   fEEFunc = 0;
   fEEAddr = 0;
   ResetArg();
   fMethod = new TClingMethodInfo(*info);
   if (!fMethod->IsValid()) {
      return;
   }
}

void TClingCallFunc::SetFuncProto(const TClingClassInfo *info,
                                  const char *method, const char *proto,
                                  long *poffset,
                                  EFunctionMatchMode mode /* = kConversionMatch */
                                  )
{
   SetFuncProto(info,method,proto,false,poffset, mode);
}

void TClingCallFunc::SetFuncProto(const TClingClassInfo *info,
                                  const char *method, const char *proto,
                                  bool objectIsConst,
                                  long *poffset,
                                  EFunctionMatchMode mode /* =kConversionMatch */
                                  )
{
   delete fMethod;
   fMethod = new TClingMethodInfo(fInterp);
   if (poffset) {
      *poffset = 0L;
   }
   ResetArg();
   if (!info->IsValid()) {
      Error("TClingCallFunc::SetFuncProto", "Class info is invalid!");
      return;
   }
   *fMethod = info->GetMethod(method, proto, objectIsConst, poffset, mode);
   if (!fMethod->IsValid()) {
      //Error("TClingCallFunc::SetFuncProto", "Could not find method %s(%s)",
      //      method, proto);
      return;
   }
}

void TClingCallFunc::SetFuncProto(const TClingClassInfo *info,
                                  const char *method,
                                  const llvm::SmallVector<clang::QualType, 4> &proto,
                                  long *poffset,
                                  EFunctionMatchMode mode /* = kConversionMatch */
                                  )
{
   SetFuncProto(info,method,proto,false,poffset, mode);
}

void TClingCallFunc::SetFuncProto(const TClingClassInfo *info,
                                  const char *method,
                                  const llvm::SmallVector<clang::QualType, 4> &proto,
                                  bool objectIsConst,
                                  long *poffset,
                                  EFunctionMatchMode mode /* =kConversionMatch */
                                  )
{
   delete fMethod;
   fMethod = new TClingMethodInfo(fInterp);
   if (poffset) {
      *poffset = 0L;
   }
   ResetArg();
   if (!info->IsValid()) {
      Error("TClingCallFunc::SetFuncProto", "Class info is invalid!");
      return;
   }
   *fMethod = info->GetMethod(method, proto, objectIsConst, poffset, mode);
   if (!fMethod->IsValid()) {
      //Error("TClingCallFunc::SetFuncProto", "Could not find method %s(%s)",
      //      method, proto);
      return;
   }
}

