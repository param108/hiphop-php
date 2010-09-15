/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010 Facebook, Inc. (http://www.facebook.com)          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#include <compiler/analysis/constant_table.h>
#include <compiler/analysis/analysis_result.h>
#include <compiler/analysis/code_error.h>
#include <compiler/analysis/type.h>
#include <compiler/code_generator.h>
#include <compiler/expression/expression.h>
#include <compiler/expression/scalar_expression.h>
#include <compiler/option.h>
#include <util/util.h>
#include <util/hash.h>
#include <compiler/analysis/class_scope.h>
#include <runtime/base/complex_types.h>

using namespace HPHP;
using namespace std;
using namespace boost;

///////////////////////////////////////////////////////////////////////////////

ConstantTable::ConstantTable(BlockScope &blockScope)
    : SymbolTable(blockScope), m_emptyJumpTable(false), m_hasDynamic(false) {
}

///////////////////////////////////////////////////////////////////////////////

TypePtr ConstantTable::add(const std::string &name, TypePtr type,
                           ExpressionPtr exp, AnalysisResultPtr ar,
                           ConstructPtr construct) {

  if (name == "true" || name == "false") {
    return Type::Boolean;
  }

  Symbol *sym = getSymbol(name, true);
  if (!sym->declarationSet()) {
    setType(ar, sym, type, true);
    sym->setDeclaration(construct);
    sym->setValue(exp);
    return type;
  }

  if (ar->isFirstPass()) {
    if (exp != sym->getValue()) {
      ar->getCodeError()->record(CodeError::DeclaredConstantTwice, construct,
                                 sym->getDeclaration());
      sym->setDynamic();
      m_hasDynamic = true;
      type = Type::Variant;
    }
    setType(ar, sym, type, true);
  }

  return type;
}

void ConstantTable::setDynamic(AnalysisResultPtr ar, const std::string &name) {
  Symbol *sym = getSymbol(name, true);
  sym->setDynamic();
  m_hasDynamic = true;
  setType(ar, sym, Type::Variant, true);
}

void ConstantTable::setValue(AnalysisResultPtr ar, const std::string &name,
                             ExpressionPtr value) {
  getSymbol(name, true)->setValue(value);
}

bool ConstantTable::isRecursivelyDeclared(AnalysisResultPtr ar,
                                          const std::string &name) {
  if (Symbol *sym = getSymbol(name)) {
    if (sym->valueSet()) return true;
  }
  ClassScopePtr parent = findParent(ar, name);
  if (parent) {
    return parent->getConstants()->isRecursivelyDeclared(ar, name);
  }
  return false;
}

ConstructPtr ConstantTable::getValueRecur(AnalysisResultPtr ar,
                                          const std::string &name,
                                          ClassScopePtr &defClass) {
  if (Symbol *sym = getSymbol(name)) {
    if (sym->getValue()) return sym->getValue();
  }
  ClassScopePtr parent = findParent(ar, name);
  if (parent) {
    defClass = parent;
    return parent->getConstants()->getValueRecur(ar, name, defClass);
  }
  return ConstructPtr();
}

ConstructPtr ConstantTable::getDeclarationRecur(AnalysisResultPtr ar,
                                                const std::string &name,
                                                ClassScopePtr &defClass) {
  if (Symbol *sym = getSymbol(name)) {
    if (sym->getDeclaration()) return sym->getDeclaration();
  }
  ClassScopePtr parent = findParent(ar, name);
  if (parent) {
    defClass = parent;
    return parent->getConstants()->getDeclarationRecur(ar, name, defClass);
  }
  return ConstructPtr();
}

TypePtr ConstantTable::checkBases(const std::string &name, TypePtr type,
                                  bool coerce, AnalysisResultPtr ar,
                                  ConstructPtr construct,
                                  const std::vector<std::string> &bases,
                                  BlockScope *&defScope) {
  TypePtr actualType;
  defScope = NULL;
  ClassScopePtr parent = findParent(ar, name);
  if (parent) {
    actualType = parent->checkConst(name, type, coerce, ar, construct,
                                    parent->getBases(), defScope);
    if (defScope) return actualType;
  }
  for (int i = bases.size() - 1; i >= (parent ? 1 : 0); i--) {
    const string &base = bases[i];
    ClassScopePtr super = ar->findClass(base);
    if (!super) continue;
    actualType = super->checkConst(name, type, coerce, ar, construct,
                                   super->getBases(), defScope);
    if (defScope) return actualType;
  }
  return actualType;
}

TypePtr ConstantTable::check(const std::string &name, TypePtr type,
                             bool coerce, AnalysisResultPtr ar,
                             ConstructPtr construct,
                             const std::vector<std::string> &bases,
                             BlockScope *&defScope) {
  TypePtr actualType;
  defScope = NULL;
  if (name == "true" || name == "false") {
    actualType = Type::Boolean;
  } else {
    Symbol *sym = getSymbol(name, true);
    if (!sym->valueSet()) {
      if (ar->getPhase() != AnalysisResult::AnalyzeInclude) {
        actualType = checkBases(name, type, coerce, ar, construct,
                                bases, defScope);
        if (defScope) return actualType;
        ar->getCodeError()->record(CodeError::UseUndeclaredConstant,
                                   construct);
        if (m_blockScope.is(BlockScope::ClassScope)) {
          actualType = Type::Variant;
        } else {
          actualType = Type::String;
        }
        setType(ar, sym, actualType, true);
      }
    } else {
      if (sym->getCoerced()) {
        defScope = &m_blockScope;
        actualType = sym->getCoerced();
        if (actualType->is(Type::KindOfSome) ||
            actualType->is(Type::KindOfAny)) {
          setType(ar, sym, type, true);
          return type;
        }
      } else {
        actualType = checkBases(name, type, coerce, ar, construct,
                                bases, defScope);
        if (defScope) return actualType;
        actualType = NEW_TYPE(Some);
        setType(ar, sym, actualType, true);
        sym->setDeclaration(construct);
      }
    }
  }

  if (Type::IsBadTypeConversion(ar, actualType, type, coerce)) {
    ar->getCodeError()->record(construct, type->getKindOf(),
                               actualType->getKindOf());
  }
  return actualType;
}

ClassScopePtr ConstantTable::findParent(AnalysisResultPtr ar,
                                        const std::string &name) {
  for (ClassScopePtr parent = m_blockScope.getParentScope(ar);
       parent && !parent->isRedeclaring();
       parent = parent->getParentScope(ar)) {
    if (parent->hasConst(name)) {
      return parent;
    }
  }
  return ClassScopePtr();
}

///////////////////////////////////////////////////////////////////////////////

void ConstantTable::outputPHP(CodeGenerator &cg, AnalysisResultPtr ar) {
  if (Option::GenerateInferredTypes) {
    for (unsigned int i = 0; i < m_symbolVec.size(); i++) {
      Symbol *sym = m_symbolVec[i];
      if (sym->isSystem()) continue;

      cg_printf("// @const  %s\t$%s\n",
                sym->getFinalType()->toString().c_str(),
                sym->getName().c_str());
    }
  }
}

void ConstantTable::outputCPPDynamicDecl(CodeGenerator &cg,
                                         AnalysisResultPtr ar) {
  const char *prefix = Option::ConstantPrefix;
  string classId;
  const char *fmt = "Variant %s%s%s;\n";
  ClassScopePtr scope = ar->getClassScope();
  if (scope) {
    prefix = Option::ClassConstantPrefix;
    classId = scope->getId(cg);
    fmt = "Variant %s%s_%s;\n";
  }

  for (StringToSymbolMap::iterator iter = m_symbolMap.begin(),
         end = m_symbolMap.end(); iter != end; ++iter) {
    Symbol *sym = &iter->second;
    if (sym->declarationSet() && sym->isDynamic()) {
      cg_printf(fmt, prefix, classId.c_str(),
                cg.formatLabel(sym->getName()).c_str());
    }
  }
}

void ConstantTable::outputCPPDynamicImpl(CodeGenerator &cg,
                                         AnalysisResultPtr ar) {
  for (StringToSymbolMap::iterator iter = m_symbolMap.begin(),
         end = m_symbolMap.end(); iter != end; ++iter) {
    Symbol *sym = &iter->second;
    if (sym->declarationSet() && sym->isDynamic()) {
      cg_printf("%s%s = \"%s\";\n", Option::ConstantPrefix,
                cg.formatLabel(sym->getName()).c_str(),
                cg.escapeLabel(sym->getName()).c_str());
    }
  }
}

void ConstantTable::collectCPPGlobalSymbols(StringPairVec &symbols,
                                            CodeGenerator &cg,
                                            AnalysisResultPtr ar) {
  for (StringToSymbolMap::iterator iter = m_symbolMap.begin(),
         end = m_symbolMap.end(); iter != end; ++iter) {
    Symbol *sym = &iter->second;
    if (sym->declarationSet() && sym->isDynamic()) {
      string varname = Option::ConstantPrefix + cg.formatLabel(sym->getName());
      symbols.push_back(pair<string, string>(varname, varname));
    }
  }
}

void ConstantTable::outputCPP(CodeGenerator &cg, AnalysisResultPtr ar) {
  bool decl = true;
  if (cg.getContext() == CodeGenerator::CppConstantsDecl) {
    decl = false;
  }

  bool printed = false;
  for (StringToSymbolMap::iterator iter = m_symbolMap.begin(),
         end = m_symbolMap.end(); iter != end; ++iter) {
    Symbol *sym = &iter->second;
    if (!sym->declarationSet() || sym->isDynamic()) continue;
    if (sym->isSystem() && cg.getOutput() != CodeGenerator::SystemCPP) continue;
    const string &name = sym->getName();
    ConstructPtr value = sym->getValue();
    printed = true;

    cg_printf(decl ? "extern const " : "const ");
    TypePtr type = sym->getFinalType();
    bool isString = type->is(Type::KindOfString);
    if (isString) {
      cg_printf("StaticString");
    } else {
      type->outputCPPDecl(cg, ar);
    }
    if (decl) {
      cg_printf(" %s%s", Option::ConstantPrefix,
                cg.formatLabel(name).c_str());
    } else {
      cg_printf(" %s%s", Option::ConstantPrefix,
                cg.formatLabel(name).c_str());
      cg_printf(isString ? "(" : " = ");
      if (value) {
        ExpressionPtr exp = dynamic_pointer_cast<Expression>(value);
        ASSERT(!exp->getExpectedType());
        ScalarExpressionPtr scalarExp =
          dynamic_pointer_cast<ScalarExpression>(exp);
        if (isString && scalarExp) {
          cg_printf("LITSTR_INIT(%s)",
                    scalarExp->getCPPLiteralString(cg).c_str());
        } else {
          exp->outputCPP(cg, ar);
        }
      } else {
        cg_printf("\"%s\"", cg.escapeLabel(name).c_str());
      }
      if (isString) {
        cg_printf(")");
      }
    }
    cg_printf(";\n");
  }
  if (printed) {
    cg_printf("\n");
  }
}

void ConstantTable::outputCPPJumpTable(CodeGenerator &cg,
                                       AnalysisResultPtr ar,
                                       bool needsGlobals,
                                       bool ret) {
  bool system = cg.getOutput() == CodeGenerator::SystemCPP;
  vector<const char *> strings;
  if (!m_symbolVec.empty()) {
    strings.reserve(m_symbolVec.size());
    BOOST_FOREACH(Symbol *sym, m_symbolVec) {
      // Extension defined constants have no value but we are sure they exist
      if (!system && !sym->getValue()) continue;
      strings.push_back(sym->getName().c_str());
    }
  }

  m_emptyJumpTable = strings.empty();
  if (!m_emptyJumpTable) {
    if (m_hasDynamic) {
      if (needsGlobals) {
        cg.printDeclareGlobals();
      }
      ClassScopePtr cls = ar->getClassScope();
      if (cls && cls->needLazyStaticInitializer()) {
        cg_printf("lazy_initializer(g);\n");
      }
    }
    for (JumpTable jt(cg, strings, false, false, false); jt.ready();
         jt.next()) {
      const char *name = jt.key();
      string varName = string(Option::ClassConstantPrefix) +
        getScope()->getId(cg) + "_" + cg.formatLabel(name);
      if (isDynamic(name)) {
        varName = string("g->") + varName;
      }
      cg_printf("HASH_RETURN(0x%016llXLL, %s, \"%s\");\n",
                hash_string(name), varName.c_str(),
                cg.escapeLabel(name).c_str());
    }
  }
  if (ret) {
    // TODO this is wrong
    cg_printf("return s;\n");
  }
}

void ConstantTable::outputCPPConstantSymbol(CodeGenerator &cg,
                                            AnalysisResultPtr ar,
                                            Symbol *sym) {
  bool cls = ar->getClassScope();
  if (sym->valueSet() &&
      (!sym->isDynamic() || cls)  &&
      !ar->isConstantRedeclared(sym->getName())) {
    ExpressionPtr value = dynamic_pointer_cast<Expression>(sym->getValue());
    Variant v;
    if (value && value->getScalarValue(v)) {
      int len;
      string output = getEscapedText(v, len);
      cg_printf("\"%s\", (const char *)%d, \"%s\",\n",
                cg.escapeLabel(sym->getName()).c_str(), len, output.c_str());
    } else {
      cg_printf("\"%s\", (const char *)0, NULL,\n",
                cg.escapeLabel(sym->getName()).c_str());
    }
  }
}

void ConstantTable::outputCPPClassMap(CodeGenerator &cg,
                                      AnalysisResultPtr ar,
                                      bool last /* = true */) {
  for (unsigned int i = 0; i < m_symbolVec.size(); i++) {
    outputCPPConstantSymbol(cg, ar, m_symbolVec[i]);
  }
  if (last) cg_printf("NULL,\n");
}
