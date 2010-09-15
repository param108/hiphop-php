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

#include <compiler/expression/class_constant_expression.h>
#include <compiler/analysis/class_scope.h>
#include <compiler/analysis/constant_table.h>
#include <compiler/analysis/code_error.h>
#include <compiler/analysis/dependency_graph.h>
#include <util/hash.h>
#include <util/util.h>
#include <compiler/option.h>
#include <compiler/analysis/variable_table.h>
#include <compiler/expression/scalar_expression.h>
#include <compiler/expression/constant_expression.h>

using namespace HPHP;
using namespace std;
using namespace boost;

///////////////////////////////////////////////////////////////////////////////

// constructors/destructors

ClassConstantExpression::ClassConstantExpression
(EXPRESSION_CONSTRUCTOR_PARAMETERS,
 ExpressionPtr classExp, const std::string &varName)
  : Expression(EXPRESSION_CONSTRUCTOR_PARAMETER_VALUES),
    StaticClassName(classExp), m_varName(varName), m_valid(false),
    m_redeclared(false), m_visited(false) {
}

ExpressionPtr ClassConstantExpression::clone() {
  ClassConstantExpressionPtr exp(new ClassConstantExpression(*this));
  Expression::deepCopy(exp);
  exp->m_class = Clone(m_class);
  return exp;
}

///////////////////////////////////////////////////////////////////////////////
// parser functions

///////////////////////////////////////////////////////////////////////////////
// static analysis functions

bool ClassConstantExpression::containsDynamicConstant(AnalysisResultPtr ar)
  const {
  if (m_class) return true;
  ClassScopePtr cls = ar->findClass(m_className);
  return !cls || cls->isVolatile() ||
    !cls->getConstants()->isRecursivelyDeclared(ar, m_varName);
}

void ClassConstantExpression::analyzeProgram(AnalysisResultPtr ar) {
  if (m_class) {
    m_class->analyzeProgram(ar);
  } else {
    addUserClass(ar, m_className);
  }
}

ConstructPtr ClassConstantExpression::getNthKid(int n) const {
  switch (n) {
    case 0:
      return m_class;
    default:
      ASSERT(false);
      break;
  }
  return ConstructPtr();
}

int ClassConstantExpression::getKidCount() const {
  return 1;
}

void ClassConstantExpression::setNthKid(int n, ConstructPtr cp) {
  switch (n) {
    case 0:
      m_class = boost::dynamic_pointer_cast<Expression>(cp);
      break;
    default:
      ASSERT(false);
      break;
  }
}

ExpressionPtr ClassConstantExpression::preOptimize(AnalysisResultPtr ar) {
  #ifdef PREOPTIMIZE_DEBUG
  std::cout<<"PreOptimizing:"<<getText()<<" from class:"<<m_className<<" for variable:"<<m_varName<<"\n";
  #endif /*PREOPTIMIZE_DEBUG*/
  if (ar->getPhase() < AnalysisResult::FirstPreOptimize) {
    #ifdef PREOPTIMIZE_DEBUG
     std::cout<<"Failing First Optimize\n";
    #endif /*PREOPTIMIZE_DEBUG*/
    return ExpressionPtr();
  }
  if (m_class) {
    #ifdef PREOPTIMIZE_DEBUG
    std::cout<<"Found m_class\n"<<"\n";
    #endif /*PREOPTIMIZE_DEBUG*/
    ar->preOptimize(m_class);
    updateClassName();
    return ExpressionPtr();
  }
  string currentClsName;
  ClassScopePtr currentCls = ar->getClassScope();
  if (currentCls) currentClsName = currentCls->getName();
  bool inCurrentClass = currentClsName == m_className;
  ClassScopePtr cls =
    inCurrentClass ? currentCls : ar->resolveClass(m_className);
  if (!cls) {
      #ifdef PREOPTIMIZE_DEBUG
      std::cout<<"couldnt find classscope\n";
      #endif /*PREOPTIMIZE_DEBUG*/
	return ExpressionPtr();
  }
  if (!inCurrentClass) {
         #ifdef PREOPTIMIZE_DEBUG
	 std::cout<<"inCurrentClass == false\n";
         #endif /*PREOPTIMIZE_DEBUG*/
    if (m_redeclared) {
         #ifdef PREOPTIMIZE_DEBUG
	 std::cout<<"m_redeclared == true\n";
         #endif /*PREOPTIMIZE_DEBUG*/
 	 return ExpressionPtr();
    }
    /* PARAM:I dont know why this check is required
       being a volatile class does not affect constants, which need to be available anyway
    */
    if (cls->isVolatile()) {
	 #ifdef PREOPTIMIZE_DEBUG
	 std::cout<<"isVolatile == true\n";
         #endif /*PREOPTIMIZE_DEBUG*/
	 /*if (strcasecmp(m_className.c_str(),"zapiclient")) {
	 return ExpressionPtr();
	 }
	 std::cout<<"skipping volatile check\n";*/
    }
    if (cls->isRedeclaring()) {
  	 #ifdef PREOPTIMIZE_DEBUG
	 std::cout<<"isRedeclaring == true\n";
         #endif /*PREOPTIMIZE_DEBUG*/
	 return ExpressionPtr();
    }
  }
  ConstantTablePtr constants = cls->getConstants();
  if (constants->isRecursivelyDeclared(ar, m_varName)) {
     #ifdef PREOPTIMIZE_DEBUG
     std::cout<<"isRecursivelyDeclared == true\n";
     #endif /*PREOPTIMIZE_DEBUG*/
    ConstructPtr decl = constants->getValue(m_varName);
    if (decl) {
     #ifdef PREOPTIMIZE_DEBUG
     std::cout<<"Found Declaration\n";
     #endif /*PREOPTIMIZE_DEBUG*/
      ExpressionPtr value = dynamic_pointer_cast<Expression>(decl);
      if (!m_visited) {
     #ifdef PREOPTIMIZE_DEBUG
     std::cout<<"m_visited = false\n";
     #endif /*PREOPTIMIZE_DEBUG*/
        m_visited = true;
        ar->pushScope(cls);
        ExpressionPtr optExp = value->preOptimize(ar);
        ar->popScope();
        m_visited = false;
        if (optExp) {
        #ifdef PREOPTIMIZE_DEBUG
	std::cout<<"Using optimized expression:"<<"\n";
        #endif /*PREOPTIMIZE_DEBUG*/
	 value = optExp;
	}
      }
      if (value->isScalar()) {
        // inline the value
        #ifdef PREOPTIMIZE_DEBUG
	std::cout<<"Found Scalar"<<"\n";
        #endif /*PREOPTIMIZE_DEBUG*/
        if (value->is(Expression::KindOfScalarExpression)) {
        #ifdef PREOPTIMIZE_DEBUG
	std::cout<<"Found KindOfScalarExpression\n";
        #endif /*PREOPTIMIZE_DEBUG*/
          ScalarExpressionPtr exp =
            dynamic_pointer_cast<ScalarExpression>(Clone(value));
          bool annotate = Option::FlAnnotate;
          Option::FlAnnotate = false; // avoid nested comments on getText()
          exp->setComment(getText());
          Option::FlAnnotate = annotate;
          exp->setLocation(getLocation());
          return exp;
        } else if (value->is(Expression::KindOfConstantExpression)) {
	#ifdef PREOPTIMIZE_DEBUG
	std::cout<<"Found KindOfConstantExpression\n";
        #endif /*PREOPTIMIZE_DEBUG*/
          // inline the value
          ConstantExpressionPtr exp =
            dynamic_pointer_cast<ConstantExpression>(Clone(value));
          bool annotate = Option::FlAnnotate;
          Option::FlAnnotate = false; // avoid nested comments
          exp->setComment(getText());
          Option::FlAnnotate = annotate;
          exp->setLocation(getLocation());
          return exp;
        }
      }
    }
  }
  #ifdef PREOPTIMIZE_DEBUG
  std::cout<<"FallThrough (Not Recursive)\n";
  #endif /*PREOPTIMIZE_DEBUG*/
  return ExpressionPtr();
}

ExpressionPtr ClassConstantExpression::postOptimize(AnalysisResultPtr ar) {
  if (m_class) ar->postOptimize(m_class);
  return ExpressionPtr();
}

TypePtr ClassConstantExpression::inferTypes(AnalysisResultPtr ar,
                                            TypePtr type, bool coerce) {
  m_valid = false;
  ConstructPtr self = shared_from_this();

  if (m_class) {
    m_class->inferAndCheck(ar, NEW_TYPE(Any), false);
    return Type::Variant;
  }

  ClassScopePtr cls = ar->resolveClass(m_className);
  if (!cls || cls->isRedeclaring()) {
    if (cls) {
      m_redeclared = true;
      ar->getScope()->getVariables()->
        setAttribute(VariableTable::NeedGlobalPointer);
    }
    if (!cls && ar->isFirstPass()) {
      ar->getCodeError()->record(self, CodeError::UnknownClass, self);
    }
    return Type::Variant;
  }
  if (cls->getConstants()->isDynamic(m_varName) || cls->isVolatile()) {
    ar->getScope()->getVariables()->
      setAttribute(VariableTable::NeedGlobalPointer);
  }
  ClassScopePtr defClass = cls;
  ConstructPtr decl =
    cls->getConstants()->getDeclarationRecur(ar, m_varName, defClass);
  if (decl) { // No decl means an extension class.
    cls = defClass;
    string name = m_className + "::" + m_varName;
    ar->getDependencyGraph()->add(DependencyGraph::KindOfConstant,
                                  ar->getName(),
                                  name, shared_from_this(), name, decl);
    m_valid = true;
  }
  BlockScope *defScope;
  TypePtr t = cls->checkConst(m_varName, type, coerce, ar,
                              shared_from_this(),
                              cls->getBases(), defScope);
  if (defScope) {
    m_valid = true;
    m_defScope = defScope;
  }

  return t;
}

unsigned ClassConstantExpression::getCanonHash() const {
  int64 val =
    hash_string(Util::toLower(m_varName).c_str(), m_varName.size()) -
    hash_string(Util::toLower(m_className).c_str(), m_className.size());
  return ~unsigned(val) ^ unsigned(val >> 32);
}

bool ClassConstantExpression::canonCompare(ExpressionPtr e) const {
  return Expression::canonCompare(e) &&
    m_varName == static_cast<ClassConstantExpression*>(e.get())->m_varName &&
    m_className == static_cast<ClassConstantExpression*>(e.get())->m_className;
}


///////////////////////////////////////////////////////////////////////////////
// code generation functions

void ClassConstantExpression::outputPHP(CodeGenerator &cg,
                                        AnalysisResultPtr ar) {
  StaticClassName::outputPHP(cg, ar);
  cg_printf("::%s", m_varName.c_str());
}

void ClassConstantExpression::outputCPPImpl(CodeGenerator &cg,
                                            AnalysisResultPtr ar) {
  if (m_class) {
    cg_printf("get_class_constant(");
    if (m_class->is(KindOfScalarExpression)) {
      ASSERT(strcasecmp(dynamic_pointer_cast<ScalarExpression>(m_class)->
                        getString().c_str(), "static") == 0);
      cg_printf("FrameInjection::GetStaticClassName(info).data()");
    } else {
      cg_printf("get_static_class_name(");
      m_class->outputCPP(cg, ar);
      cg_printf(").data()");
    }
    cg_printf(", \"%s\")", m_varName.c_str());
    return;
  }

  bool outsideClass = !ar->checkClassPresent(m_origClassName);
  if (m_valid) {
    string trueClassName;

    ASSERT(m_defScope);
    ClassScope *cls = dynamic_cast<ClassScope*>(m_defScope);
    trueClassName = cls->getName();
    ASSERT(!trueClassName.empty());
    if (outsideClass) {
      cls->outputVolatileCheckBegin(cg, ar, m_origClassName);
    }
    ConstructPtr decl = m_defScope->getConstants()->getValue(m_varName);
    if (decl) {
      decl->outputCPP(cg, ar);
      if (cg.getContext() == CodeGenerator::CppImplementation ||
          cg.getContext() == CodeGenerator::CppParameterDefaultValueImpl) {
        cg_printf("(%s::%s)", m_className.c_str(), m_varName.c_str());
      } else {
        cg_printf("/* %s::%s */", m_className.c_str(), m_varName.c_str());
      }
    } else {
      if (cls->getConstants()->isDynamic(m_varName)) {
        cg_printf("%s%s::lazy_initializer(%s)->", Option::ClassPrefix,
                  cls->getId(cg).c_str(), cg.getGlobals(ar));
      }
      cg_printf("%s%s_%s", Option::ClassConstantPrefix, cls->getId(cg).c_str(),
                m_varName.c_str());
    }
    if (outsideClass) {
      cls->outputVolatileCheckEnd(cg);
    }
  } else if (m_redeclared) {
    if (outsideClass) {
      ClassScope::OutputVolatileCheckBegin(cg, ar, m_origClassName);
    }
    cg_printf("%s->%s%s->os_constant(\"%s\")", cg.getGlobals(ar),
              Option::ClassStaticsObjectPrefix,
              m_className.c_str(), m_varName.c_str());
    if (outsideClass) {
      ClassScope::OutputVolatileCheckEnd(cg);
    }
  } else {
    cg_printf("throw_fatal(\"unknown class constant %s::%s\")",
              m_className.c_str(), m_varName.c_str());
  }
}
