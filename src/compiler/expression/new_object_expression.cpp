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

#include <compiler/expression/new_object_expression.h>
#include <compiler/expression/scalar_expression.h>
#include <compiler/expression/expression_list.h>
#include <compiler/analysis/code_error.h>
#include <compiler/analysis/class_scope.h>
#include <compiler/analysis/function_scope.h>
#include <compiler/analysis/dependency_graph.h>
#include <compiler/option.h>
#include <compiler/analysis/variable_table.h>

using namespace HPHP;
using namespace std;
using namespace boost;

///////////////////////////////////////////////////////////////////////////////
// constructors/destructors

NewObjectExpression::NewObjectExpression
(EXPRESSION_CONSTRUCTOR_PARAMETERS,
 ExpressionPtr variable, ExpressionListPtr params)
  : FunctionCall(EXPRESSION_CONSTRUCTOR_PARAMETER_VALUES,
                 variable, "", params, ExpressionPtr()),
    m_redeclared(false), m_dynamic(false) {
}

ExpressionPtr NewObjectExpression::clone() {
  NewObjectExpressionPtr exp(new NewObjectExpression(*this));
  FunctionCall::deepCopy(exp);
  return exp;
}

///////////////////////////////////////////////////////////////////////////////
// parser functions

///////////////////////////////////////////////////////////////////////////////
// static analysis functions

void NewObjectExpression::analyzeProgram(AnalysisResultPtr ar) {
  FunctionScopePtr func;
  if (!m_name.empty()) {
    addUserClass(ar, m_name);
    if (ClassScopePtr cls = ar->resolveClass(m_name)) {
      if (!cls->isRedeclaring()) {
        func = cls->findConstructor(ar, true);
      }
    }
  }

  m_nameExp->analyzeProgram(ar);
  if (m_params) {
    m_params->analyzeProgram(ar);
    markRefParams(func, "", false);
  }
}

ExpressionPtr NewObjectExpression::preOptimize(AnalysisResultPtr ar) {
  return FunctionCall::preOptimize(ar);
}

ExpressionPtr NewObjectExpression::postOptimize(AnalysisResultPtr ar) {
  return FunctionCall::postOptimize(ar);
}

TypePtr NewObjectExpression::inferTypes(AnalysisResultPtr ar, TypePtr type,
                                        bool coerce) {
  reset();
  ConstructPtr self = shared_from_this();
  if (!m_name.empty()) {
    ClassScopePtr cls = ar->resolveClass(m_name);
    if (cls) {
      m_name = cls->getName();
    }
    if (!cls || cls->isRedeclaring()) {
      if (cls) {
        m_redeclared = true;
        ar->getScope()->getVariables()->
          setAttribute(VariableTable::NeedGlobalPointer);
      }
      if (!cls && ar->isFirstPass()) {
        ar->getCodeError()->record(self, CodeError::UnknownClass, self);
      }
      if (m_params) m_params->inferAndCheck(ar, NEW_TYPE(Any), false);
      return NEW_TYPE(Object);
    }
    if (cls->isVolatile()) {
      ar->getScope()->getVariables()->
        setAttribute(VariableTable::NeedGlobalPointer);
    }
    m_dynamic = cls->derivesFromRedeclaring();
    m_validClass = true;
    FunctionScopePtr func = cls->findConstructor(ar, true);
    if (!func) {
      if (m_params) {
        if (!m_dynamic && m_params->getCount()) {
          if (ar->isFirstPass()) {
            ar->getCodeError()->record(self, CodeError::BadConstructorCall,
                                       self);
          }
          m_params->setOutputCount(0);
        }
        m_params->inferAndCheck(ar, NEW_TYPE(Some), false);
      }
    } else {
      m_extraArg = func->inferParamTypes(ar, self, m_params,
                                         m_validClass);
      m_variableArgument = func->isVariableArgument();
    }
    if (!m_validClass || m_dynamic) {
      m_implementedType = NEW_TYPE(Object);
    } else {
      m_implementedType.reset();
    }
    return Type::CreateObjectType(m_name);
  } else {
    ar->containsDynamicClass();
    if (ar->isFirstPass()) {
      ar->getCodeError()->record(self, CodeError::UseDynamicClass,
                                 self);
    }
    if (m_params) {
      m_params->markParams(false);
    }
  }

  m_implementedType.reset();
  m_nameExp->inferAndCheck(ar, Type::String, false);
  if (m_params) m_params->inferAndCheck(ar, NEW_TYPE(Any), false);
  return NEW_TYPE(Object);
}

///////////////////////////////////////////////////////////////////////////////
// code generation functions

void NewObjectExpression::outputPHP(CodeGenerator &cg, AnalysisResultPtr ar) {
  outputLineMap(cg, ar);

  cg_printf("new ");
  m_nameExp->outputPHP(cg, ar);
  cg_printf("(");
  if (m_params) m_params->outputPHP(cg, ar);
  cg_printf(")");
}

void NewObjectExpression::preOutputStash(CodeGenerator &cg,
                                         AnalysisResultPtr ar, int state) {
  if (!m_receiverTemp.empty()) {
    TypePtr e = getExpectedType();
    if (!e || !Type::IsCastNeeded(ar, getActualType(), e)) {
      bool unused = isUnused();
      setUnused(true);
      outputCPPImpl(cg, ar);
      setUnused(unused);
      setCPPTemp(m_receiverTemp);
      cg_printf(";\n");
      return;
    }
  }
  Expression::preOutputStash(cg, ar, state);
}

void NewObjectExpression::outputCPPImpl(CodeGenerator &cg,
                                        AnalysisResultPtr ar) {
  string &cname = (m_origName == "self" || m_origName == "parent") ?
    m_name : m_origName;
  bool outsideClass = !ar->checkClassPresent(m_origName);
  if (!m_name.empty() && !m_redeclared && m_validClass && !m_dynamic) {
    ClassScopePtr cls = ar->resolveClass(m_name);
    ASSERT(cls);
    if (m_receiverTemp.empty()) {
      if (outsideClass) {
        cls->outputVolatileCheckBegin(cg, ar, cname);
      }
      cg_printf("%s%s((NEWOBJ(%s%s)())->create(",
                Option::SmartPtrPrefix, cls->getId(cg).c_str(),
                Option::ClassPrefix, cls->getId(cg).c_str());
    } else {
      cg_printf("(%s->create(", m_receiverTemp.c_str());
    }

    FunctionScope::outputCPPArguments(m_params, cg, ar, m_extraArg,
                                      m_variableArgument, m_argArrayId,
                                      m_argArrayHash, m_argArrayIndex);
    if (m_receiverTemp.empty()) {
      cg_printf("))");
      if (outsideClass) {
        cls->outputVolatileCheckEnd(cg);
      }
    } else {
      cg_printf(")");
      if (!isUnused()) {
        cg_printf(", %s", m_receiverTemp.c_str());
      }
      cg_printf(")");
    }
  } else {
    if (m_redeclared) {
      if (outsideClass) {
        ClassScope::OutputVolatileCheckBegin(cg, ar, cname);
      }
      cg_printf("g->%s%s->create(", Option::ClassStaticsObjectPrefix,
                m_name.c_str());
    } else {
      cg_printf("create_object(");
      if (!cname.empty()) {
        cg_printf("\"%s\"", cname.c_str());
      } else if (m_nameExp->is(Expression::KindOfSimpleVariable)) {
        m_nameExp->outputCPP(cg, ar);
      } else {
        cg_printf("(");
        m_nameExp->outputCPP(cg, ar);
        cg_printf(")");
      }
      cg_printf(", ");
    }
    if (m_params && m_params->getOutputCount()) {
      FunctionScope::outputCPPArguments(m_params, cg, ar, -1, false);
    } else {
      cg_printf("Array()");
    }
    cg_printf(")");
    if (m_redeclared && outsideClass) {
      ClassScope::OutputVolatileCheckEnd(cg);
    }
  }
}

bool NewObjectExpression::preOutputCPP(CodeGenerator &cg, AnalysisResultPtr ar,
                                       int state) {
  bool tempRcvr = true;

  if (m_name.empty() || m_redeclared || !m_validClass || m_dynamic) {
    tempRcvr = false;
  }

  bool paramEffect = false;
  if (m_params && m_params->getCount() > 0) {
    for (int i = m_params->getCount(); i--; ) {
      if (!(*m_params)[i]->isScalar()) {
        paramEffect = true;
        break;
      }
    }
  }

  if (!paramEffect) {
    tempRcvr = false;
  }

  string &cname = (m_origName == "self" || m_origName == "parent") ?
    m_name : m_origName;

  if (tempRcvr && ar->inExpression()) {
    ar->wrapExpressionBegin(cg);
    m_receiverTemp = genCPPTemp(cg, ar);
    bool outsideClass = !ar->checkClassPresent(m_origName);
    ClassScopePtr cls = ar->resolveClass(m_name);
    ASSERT(cls);
    cg_printf("%s%s %s = ", Option::SmartPtrPrefix,
              cls->getId(cg).c_str(), m_receiverTemp.c_str());
    if (outsideClass) {
      cls->outputVolatileCheckBegin(cg, ar, cname);
    }
    cg_printf("NEWOBJ(%s%s)()", Option::ClassPrefix, cls->getId(cg).c_str());
    if (outsideClass) {
      cls->outputVolatileCheckEnd(cg);
    }
    cg_printf(";\n");
  }

  bool tempParams = FunctionCall::preOutputCPP(cg, ar, state);
  return tempRcvr || tempParams;
}
