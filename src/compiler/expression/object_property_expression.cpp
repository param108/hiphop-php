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

#include <compiler/expression/object_property_expression.h>
#include <compiler/expression/scalar_expression.h>
#include <compiler/expression/expression_list.h>
#include <compiler/analysis/code_error.h>
#include <compiler/analysis/class_scope.h>
#include <compiler/analysis/function_scope.h>
#include <compiler/analysis/variable_table.h>
#include <compiler/analysis/dependency_graph.h>
#include <compiler/option.h>
#include <compiler/expression/simple_variable.h>
#include <util/hash.h>
#include <compiler/parser/hphp.tab.hpp>

using namespace HPHP;
using namespace std;
using namespace boost;

///////////////////////////////////////////////////////////////////////////////
// constructors/destructors

ObjectPropertyExpression::ObjectPropertyExpression
(EXPRESSION_CONSTRUCTOR_PARAMETERS,
 ExpressionPtr object, ExpressionPtr property)
  : Expression(EXPRESSION_CONSTRUCTOR_PARAMETER_VALUES),
    m_object(object), m_property(property),
    m_valid(false), m_localEffects(AccessorEffect) {
  m_object->setContext(Expression::ObjectContext);
}

ExpressionPtr ObjectPropertyExpression::clone() {
  ObjectPropertyExpressionPtr exp(new ObjectPropertyExpression(*this));
  Expression::deepCopy(exp);
  exp->m_object = Clone(m_object);
  exp->m_property = Clone(m_property);
  return exp;
}

///////////////////////////////////////////////////////////////////////////////
// parser functions

///////////////////////////////////////////////////////////////////////////////
// static analysis functions

bool ObjectPropertyExpression::isTemporary() const {
  return !m_valid && !(m_context & (LValue | RefValue | UnsetContext));
}

void ObjectPropertyExpression::setContext(Context context) {
  m_context |= context;
  switch (context) {
    case Expression::LValue:
      if (!hasContext(Expression::UnsetContext)) {
        m_object->setContext(Expression::LValue);
      }
      break;
    case Expression::DeepAssignmentLHS:
    case Expression::DeepOprLValue:
    case Expression::ExistContext:
    case Expression::UnsetContext:
    case Expression::DeepReference:
      m_object->setContext(context);
      break;
    case Expression::RefValue:
    case Expression::RefParameter:
      m_object->setContext(DeepReference);
      break;
    default:
      break;
  }

  if (m_context & (LValue|RefValue)) {
    setEffect(CreateEffect);
  }
}
void ObjectPropertyExpression::clearContext(Context context) {
  m_context &= ~context;
  switch (context) {
    case Expression::LValue:
    case Expression::DeepOprLValue:
    case Expression::DeepAssignmentLHS:
    case Expression::UnsetContext:
    case Expression::DeepReference:
      m_object->clearContext(context);
      break;
    case Expression::RefValue:
    case Expression::RefParameter:
      m_object->clearContext(DeepReference);
      break;
    default:
      break;
  }

  if (!(m_context & (LValue|RefValue))) {
    clearEffect(CreateEffect);
  }
}

void ObjectPropertyExpression::analyzeProgram(AnalysisResultPtr ar) {
  Expression::analyzeProgram(ar);

  m_object->analyzeProgram(ar);
  m_property->analyzeProgram(ar);
}

ConstructPtr ObjectPropertyExpression::getNthKid(int n) const {
  switch (n) {
    case 0:
      return m_object;
    case 1:
      return m_property;
    default:
      ASSERT(false);
      break;
  }
  return ConstructPtr();
}

int ObjectPropertyExpression::getKidCount() const {
  return 2;
}

void ObjectPropertyExpression::setNthKid(int n, ConstructPtr cp) {
  switch (n) {
    case 0:
      m_object = boost::dynamic_pointer_cast<Expression>(cp);
      break;
    case 1:
      m_property = boost::dynamic_pointer_cast<Expression>(cp);
      break;
    default:
      ASSERT(false);
      break;
  }
}

ExpressionPtr ObjectPropertyExpression::preOptimize(AnalysisResultPtr ar) {
  ar->preOptimize(m_object);
  ar->preOptimize(m_property);
  return ExpressionPtr();
}

ExpressionPtr ObjectPropertyExpression::postOptimize(AnalysisResultPtr ar) {
  ar->postOptimize(m_object);
  ar->postOptimize(m_property);
  return ExpressionPtr();
}

void ObjectPropertyExpression::setEffect(Effect effect) {
  if ((m_localEffects & effect) != effect) {
    recomputeEffects();
    m_localEffects |= effect;
  }
}

void ObjectPropertyExpression::clearEffect(Effect effect) {
  if (m_localEffects & effect) {
    recomputeEffects();
    m_localEffects &= ~effect;
  }
}

TypePtr ObjectPropertyExpression::inferTypes(AnalysisResultPtr ar,
                                             TypePtr type, bool coerce) {
  m_valid = false;

  ConstructPtr self = shared_from_this();
  TypePtr objectType = m_object->inferAndCheck(ar, NEW_TYPE(Object), false);

  if (!m_property->is(Expression::KindOfScalarExpression)) {
    // if dynamic property or method, we have nothing to find out
    if (ar->isFirstPass()) {
      ar->getCodeError()->record(self, CodeError::UseDynamicProperty, self);
    }
    m_property->inferAndCheck(ar, Type::String, false);

    // we also lost track of which class variable an expression is about, hence
    // any type inference could be wrong. Instead, we just force variants on
    // all class variables.
    if (m_context & (LValue | RefValue)) {
      ar->forceClassVariants();
    }

    return Type::Variant; // we have to use a variant to hold dynamic value
  }

  ScalarExpressionPtr exp = dynamic_pointer_cast<ScalarExpression>(m_property);
  string name = exp->getString();
  ASSERT(!name.empty());

  m_property->inferAndCheck(ar, Type::String, false);

  ClassScopePtr cls;
  if (objectType && !objectType->getName().empty()) {
    // what object-> has told us
    cls = ar->findExactClass(objectType->getName());
  } else {
    // what ->property has told us
    cls = ar->findClass(name, AnalysisResult::PropertyName);
    if (cls) {
      objectType =
        m_object->inferAndCheck(ar, Type::CreateObjectType(cls->getName()),
                                false);
    }
    if ((m_context & LValue) &&
        objectType && !objectType->is(Type::KindOfObject) &&
                      !objectType->is(Type::KindOfVariant) &&
                      !objectType->is(Type::KindOfSome) &&
                      !objectType->is(Type::KindOfAny)) {
      m_object->inferAndCheck(ar, NEW_TYPE(Object), true);
    }
  }

  if (!cls) {
    if (m_context & (LValue | RefValue)) {
      ar->forceClassVariants(name);
    }
    return Type::Variant;
  }

  const char *accessorName = hasContext(DeepAssignmentLHS) ? "__set" :
    hasContext(ExistContext) ? "__isset" :
    hasContext(UnsetContext) ? "__unset" : "__get";
  if (!cls->implementsAccessor(ar, accessorName)) clearEffect(AccessorEffect);

  // resolved to this class
  int present = 0;
  if (m_context & RefValue) {
    type = Type::Variant;
    coerce = true;
  }

  // use $this inside a static function
  if (m_object->isThis()) {
    FunctionScopePtr func = ar->getFunctionScope();
    if (func->isStatic()) {
      if (ar->isFirstPass()) {
        ar->getCodeError()->record(self, CodeError::MissingObjectContext,
                                   self);
      }
      m_actualType = Type::Variant;
      return m_actualType;
    }
  }

  TypePtr ret;
  if (!cls->derivesFromRedeclaring()) { // Have to use dynamic.
    ret = cls->checkProperty(name, type, coerce, ar, self, present);
    // Private only valid if in the defining class
    if (present &&
        !(present & VariableTable::VariableStatic) &&
        (getOriginalScope(ar) == cls ||
         !(present & VariableTable::VariablePrivate))) {
      m_valid = m_object->getType()->isSpecificObject();
      m_class = cls;
    }
  }

  // get() will return Variant
  if (!m_valid) {
    m_actualType = Type::Variant;
    return m_actualType;
  }

  clearEffect(AccessorEffect);

  if (ar->getPhase() == AnalysisResult::LastInference) {
    if (!(m_context & ObjectContext)) {
      m_object->clearContext(Expression::LValue);
    }
    setContext(Expression::NoLValueWrapper);
  }
  return ret;
}

///////////////////////////////////////////////////////////////////////////////
// code generation functions

void ObjectPropertyExpression::outputPHP(CodeGenerator &cg,
                                         AnalysisResultPtr ar) {
  m_object->outputPHP(cg, ar);
  cg_printf("->");
  if (m_property->getKindOf() == Expression::KindOfScalarExpression) {
    m_property->outputPHP(cg, ar);
  } else {
    cg_printf("{");
    m_property->outputPHP(cg, ar);
    cg_printf("}");
  }
}

bool ObjectPropertyExpression::directVariantProxy(AnalysisResultPtr ar) {
  TypePtr actualType = m_object->getActualType();
  if (actualType && actualType->is(Type::KindOfVariant)) {
    if (m_object->is(KindOfSimpleVariable)) {
      SimpleVariablePtr var =
        dynamic_pointer_cast<SimpleVariable>(m_object);
      const std::string &name = var->getName();
      FunctionScopePtr func =
        dynamic_pointer_cast<FunctionScope>(ar->getScope());
      VariableTablePtr variables = func->getVariables();
      if (!variables->isParameter(name) || variables->isLvalParam(name)) {
        return true;
      }
      if (variables->getAttribute(VariableTable::ContainsDynamicVariable) ||
          variables->getAttribute(VariableTable::ContainsExtract)) {
        return true;
      }
    } else {
      return true;
    }
  }
  return false;
}

void ObjectPropertyExpression::preOutputStash(CodeGenerator &cg,
                                              AnalysisResultPtr ar,
                                              int state) {
  if (!m_valid && (m_context & (LValue | RefValue | UnsetContext))) {
    m_lvalTmp = genCPPTemp(cg, ar);
    cg_printf("Variant %s;\n", m_lvalTmp.c_str());
  }
  Expression::preOutputStash(cg, ar, state);
}

bool ObjectPropertyExpression::preOutputCPP(CodeGenerator &cg,
                                            AnalysisResultPtr ar, int state) {
  return preOutputOffsetLHS(cg, ar, state);
}

void ObjectPropertyExpression::outputCPPImpl(CodeGenerator &cg,
                                             AnalysisResultPtr ar) {
  outputCPPObjProperty(cg, ar, directVariantProxy(ar), false);
}

void ObjectPropertyExpression::outputCPPObjProperty(CodeGenerator &cg,
                                                    AnalysisResultPtr ar,
                                                    bool directVariant,
                                                    int doExist) {
  string func = Option::ObjectPrefix;
  const char *error = ", true";
  ClassScopePtr cls = ar->getClassScope();
  std::string context = "";

  if (cg.getOutput() != CodeGenerator::SystemCPP) {
    if (cls) {
      context = ", s_class_name";
    } else if (FunctionScopePtr funcScope = ar->getFunctionScope()) {
      if (!funcScope->inPseudoMain()) {
        context = ", empty_string";
      }
    }
  }
  if (doExist) {
    func = doExist > 0 ? "o_isset" : "o_empty";
    error = "";
  } else {
    if (m_context & ExistContext) {
      error = ", false";
    }
    if (m_context & (LValue | RefValue | UnsetContext)) {
      if (m_context & UnsetContext) {
        assert(!(m_context & LValue)); // call outputCPPUnset instead
        func += "unsetLval";
      } else {
        func += "lval";
      }
      error = "";
      context = ", " + (m_lvalTmp.empty() ? "Variant()" : m_lvalTmp) + context;
    } else {
      func += "get";
      if (!cls || !cls->getVariables()->hasPrivate()) {
        func += "Public";
        context = "";
      }
    }
  }

  if (m_valid && doExist) cg_printf(doExist > 0 ? "isset(" : "empty(");
  outputCPPObject(cg, ar, directVariant);
  if (m_valid) {
    assert(m_object->getType()->isSpecificObject());
    ScalarExpressionPtr name =
      dynamic_pointer_cast<ScalarExpression>(m_property);
    cg_printf("%s%s", Option::PropertyPrefix, name->getString().c_str());
    if (doExist) cg_printf(")");
  } else {
    cg_printf("%s(", func.c_str());
    outputCPPProperty(cg, ar);
    cg_printf("%s%s)", error, context.c_str());
  }
}

void ObjectPropertyExpression::outputCPPObject(CodeGenerator &cg,
                                               AnalysisResultPtr ar,
                                               int directVariant) {
  if (directVariant < 0) {
    directVariant = directVariantProxy(ar);
  }

  bool bThis = m_object->isThis();
  bool useGetThis = false;
  if (bThis) {
    FunctionScopePtr funcScope = ar->getFunctionScope();
    if (funcScope && funcScope->isStatic()) {
      cg_printf("GET_THIS_ARROW()");
    } else {
      // in order for __set() and __get() to be called
      useGetThis = true;
    }
  }
  if (m_valid) {
    if (!bThis) {
      ASSERT(!directVariant);
      m_object->outputCPP(cg, ar);
      cg_printf("->");
    }
  } else {
    if (!bThis) {
      if (directVariant) {
        TypePtr expectedType = m_object->getExpectedType();
        ASSERT(expectedType->is(Type::KindOfObject));
        // Clear m_expectedType to avoid type cast (toObject).
        m_object->setExpectedType(TypePtr());
        m_object->outputCPP(cg, ar);
        m_object->setExpectedType(expectedType);
      } else {
        m_object->outputCPP(cg, ar);
      }
      cg_printf(".");
    } else {
      if (useGetThis) cg_printf("GET_THIS_DOT()");
    }
  }
}

void ObjectPropertyExpression::outputCPPProperty(CodeGenerator &cg,
                                                 AnalysisResultPtr ar) {
  if (m_property->getKindOf() == Expression::KindOfScalarExpression) {
    ScalarExpressionPtr name =
      dynamic_pointer_cast<ScalarExpression>(m_property);
    cg_printString(name->getString(), ar);
  } else {
    m_property->outputCPP(cg, ar);
  }
}

void ObjectPropertyExpression::outputCPPExistTest(CodeGenerator &cg,
                                                  AnalysisResultPtr ar,
                                                  int op) {
  outputCPPObjProperty(cg, ar, false, op == T_ISSET ? 1 : -1);
}

void ObjectPropertyExpression::outputCPPUnset(CodeGenerator &cg,
                                              AnalysisResultPtr ar) {
  bool bThis = m_object->isThis();
  if (bThis) {
    cg.printf("GET_THIS_ARROW()");
  } else {
    m_object->outputCPP(cg, ar);
    cg_printf("->");
  }
  cg_printf("o_unset(");
  outputCPPProperty(cg, ar);
  cg_printf(")");
}
