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

#include <compiler/statement/class_statement.h>
#include <compiler/parser/hphp.tab.hpp>
#include <compiler/expression/expression_list.h>
#include <compiler/statement/statement_list.h>
#include <compiler/expression/scalar_expression.h>
#include <compiler/analysis/class_scope.h>
#include <compiler/analysis/file_scope.h>
#include <compiler/analysis/function_scope.h>
#include <compiler/analysis/analysis_result.h>
#include <compiler/analysis/dependency_graph.h>
#include <compiler/statement/method_statement.h>
#include <compiler/analysis/variable_table.h>
#include <compiler/analysis/constant_table.h>
#include <util/util.h>
#include <compiler/statement/interface_statement.h>
#include <compiler/option.h>
#include <sstream>
#include <algorithm>

using namespace HPHP;
using namespace std;
using namespace boost;

///////////////////////////////////////////////////////////////////////////////
// constructors/destructors

ClassStatement::ClassStatement
(STATEMENT_CONSTRUCTOR_PARAMETERS,
 int type, const string &name, const string &parent,
 ExpressionListPtr base, const string &docComment, StatementListPtr stmt)
  : InterfaceStatement(STATEMENT_CONSTRUCTOR_PARAMETER_VALUES,
                       name, base, docComment, stmt),
    m_type(type), m_ignored(false) {
  m_parent = Util::toLower(parent);
}

StatementPtr ClassStatement::clone() {
  ClassStatementPtr stmt(new ClassStatement(*this));
  stmt->m_stmt = Clone(m_stmt);
  stmt->m_base = Clone(m_base);
  return stmt;
}

///////////////////////////////////////////////////////////////////////////////
// parser functions

void ClassStatement::onParse(AnalysisResultPtr ar) {
  ClassScope::KindOf kindOf = ClassScope::KindOfObjectClass;
  switch (m_type) {
  case T_CLASS:     kindOf = ClassScope::KindOfObjectClass;   break;
  case T_ABSTRACT:  kindOf = ClassScope::KindOfAbstractClass; break;
  case T_FINAL:     kindOf = ClassScope::KindOfFinalClass;    break;
  default:
    ASSERT(false);
  }

  vector<string> bases;
  if (!m_parent.empty()) {
    bases.push_back(m_parent);
    ar->addNonFinal(m_parent);
  }
  if (m_base) m_base->getStrings(bases);
  StatementPtr stmt = dynamic_pointer_cast<Statement>(shared_from_this());
  ClassScopePtr classScope(new ClassScope(kindOf, m_originalName, m_parent,
                                          bases, m_docComment,
                                          stmt, ar->getFileScope()));
  m_classScope = classScope;
  if (!ar->getFileScope()->addClass(ar, classScope)) {
    m_ignored = true;
    return;
  }
  ar->recordClassSource(m_name, m_loc, ar->getFileScope()->getName());

  if (m_stmt) {
    ar->pushScope(classScope);
    bool seenConstruct = false;
    for (int i = 0; i < m_stmt->getCount(); i++) {
      MethodStatementPtr meth =
        dynamic_pointer_cast<MethodStatement>((*m_stmt)[i]);
      if (meth && meth->getName() == "__construct") {
        seenConstruct = true;
        break;
      }
    }
    for (int i = 0; i < m_stmt->getCount(); i++) {
      if (!seenConstruct) {
        MethodStatementPtr meth =
          dynamic_pointer_cast<MethodStatement>((*m_stmt)[i]);
        if (meth && classScope && meth->getName() == classScope->getName()
         && !meth->getModifiers()->isStatic()) {
          // class-name constructor
          classScope->setAttribute(ClassScope::classNameConstructor);
        }
      }
      IParseHandlerPtr ph = dynamic_pointer_cast<IParseHandler>((*m_stmt)[i]);
      ph->onParse(ar);
    }
    ar->popScope();
  }
}

///////////////////////////////////////////////////////////////////////////////
// static analysis functions

string ClassStatement::getName() const {
  return string("Class ") + m_classScope.lock()->getName();
}

void ClassStatement::analyzeProgramImpl(AnalysisResultPtr ar) {
  vector<string> bases;
  if (!m_parent.empty()) bases.push_back(m_parent);
  if (m_base) m_base->getStrings(bases);
  for (unsigned int i = 0; i < bases.size(); i++) {
    string className = bases[i];
    addUserClass(ar, bases[i]);
  }

  ClassScopePtr classScope = m_classScope.lock();
  if (hasHphpNote("Volatile")) {
    classScope->setVolatile();
  }

  checkVolatile(ar);

  if (m_stmt) {
    ar->pushScope(classScope);
    m_stmt->analyzeProgram(ar);
    ar->popScope();
  }
  if (ar->getPhase() != AnalysisResult::AnalyzeAll) return;
  DependencyGraphPtr dependencies = ar->getDependencyGraph();
  for (unsigned int i = 0; i < bases.size(); i++) {
    ClassScopePtr cls = ar->findClass(bases[i]);
    if (cls) {
      if ((!cls->isInterface() && (m_parent.empty() || i > 0 )) ||
          (cls->isInterface() && (!m_parent.empty() && i == 0 ))) {
        ar->getCodeError()->record(CodeError::InvalidDerivation,
                                   shared_from_this(), ConstructPtr(),
                                   cls->getOriginalName().c_str());
      }
      if (dependencies->checkCircle(DependencyGraph::KindOfClassDerivation,
                                    m_originalName,
                                    cls->getOriginalName())) {
        ar->getCodeError()->record(CodeError::InvalidDerivation,
                                   shared_from_this(), ConstructPtr(),
                                   cls->getOriginalName().c_str());
        m_parent = "";
        m_base = ExpressionListPtr();
        classScope->clearBases();
      } else if (cls->isUserClass()) {
        dependencies->add(DependencyGraph::KindOfClassDerivation,
                          ar->getName(),
                          m_originalName, shared_from_this(),
                          cls->getOriginalName(), cls->getStmt());
      }
    }
  }
}

StatementPtr ClassStatement::preOptimize(AnalysisResultPtr ar) {
  return InterfaceStatement::preOptimize(ar);
}

StatementPtr ClassStatement::postOptimize(AnalysisResultPtr ar) {
  return InterfaceStatement::postOptimize(ar);
}

void ClassStatement::inferTypes(AnalysisResultPtr ar) {
  if (m_stmt) {
    ClassScopePtr classScope = m_classScope.lock();
    ar->pushScope(classScope);
    m_stmt->inferTypes(ar);
    ar->popScope();
  }
}

///////////////////////////////////////////////////////////////////////////////
// code generation functions

void ClassStatement::getAllParents(AnalysisResultPtr ar,
                                   std::vector<std::string> &names) {
  if (!m_parent.empty()) {
    ClassScopePtr cls = ar->findClass(m_parent);
    if (cls) {
      if (!cls->isRedeclaring()) {
        cls->getAllParents(ar, names);
      }
      names.push_back(m_parent);
    }
  }

  if (m_base) {
    vector<string> bases;
    m_base->getStrings(bases);
    for (unsigned int i = 0; i < bases.size(); i++) {
      ClassScopePtr cls = ar->findClass(bases[i]);
      if (cls) {
        if (!cls->isRedeclaring()) {
          cls->getAllParents(ar, names);
        }
        names.push_back(bases[i]);
      }
    }
  }
}

void ClassStatement::outputPHP(CodeGenerator &cg, AnalysisResultPtr ar) {
  ClassScopePtr classScope = m_classScope.lock();
  if (!classScope->isUserClass()) return;
  if (ar) ar->pushScope(classScope);

  switch (m_type) {
  case T_CLASS:                              break;
  case T_ABSTRACT: cg_printf("abstract ");   break;
  case T_FINAL:    cg_printf("final ");      break;
  default:
    ASSERT(false);
  }
  cg_printf("class %s", m_name.c_str());

  if (!m_parent.empty()) {
    cg_printf(" extends %s", m_parent.c_str());
  }

  if (m_base) {
    cg_printf(" implements ");
    m_base->outputPHP(cg, ar);
  }

  cg_indentBegin(" {\n");
  m_classScope.lock()->outputPHP(cg, ar);
  if (m_stmt) m_stmt->outputPHP(cg, ar);
  cg_indentEnd("}\n");

  if (ar) ar->popScope();
}

bool ClassStatement::hasImpl() const {
  ClassScopePtr cls = m_classScope.lock();
  return cls->isVolatile() ||
    cls->getVariables()->getAttribute(VariableTable::ContainsDynamicStatic);
}

void ClassStatement::outputCPPClassDecl(CodeGenerator &cg,
                                        AnalysisResultPtr ar,
                                        const char *clsName,
                                        const char *originalName,
                                        const char *parent) {
  ClassScopePtr classScope = m_classScope.lock();
  VariableTablePtr variables = classScope->getVariables();
  ConstantTablePtr constants = classScope->getConstants();
  if (variables->hasAllJumpTables() && constants->hasJumpTable() &&
      classScope->hasAllJumpTables()) {
    cg_printf("DECLARE_CLASS(%s, %s, %s)\n", clsName, originalName, parent);
    return;
  }

  // Now we start to break down DECLARE_CLASS into lines of code that could
  // be generated differently...

  cg_printf("DECLARE_CLASS_COMMON(%s, %s)\n", clsName, originalName);
  cg_printf("DECLARE_INVOKE_EX(%s, %s, %s)\n",
            clsName, originalName, parent);

  cg.printSection("DECLARE_STATIC_PROP_OPS");
  cg_printf("public:\n");
  if (classScope->needStaticInitializer()) {
    cg_printf("static void os_static_initializer();\n");
  }
  if (variables->hasJumpTable(VariableTable::JumpTableClassStaticGetInit)) {
    cg_printf("static Variant os_getInit(CStrRef s);\n");
  } else {
    cg_printf("#define OMIT_JUMP_TABLE_CLASS_STATIC_GETINIT_%s 1\n", clsName);
  }
  if (variables->hasJumpTable(VariableTable::JumpTableClassStaticGet)) {
    cg_printf("static Variant os_get(CStrRef s);\n");
  } else {
    cg_printf("#define OMIT_JUMP_TABLE_CLASS_STATIC_GET_%s 1\n", clsName);
  }
  if (variables->hasJumpTable(VariableTable::JumpTableClassStaticLval)) {
    cg_printf("static Variant &os_lval(CStrRef s);\n");
  } else {
    cg_printf("#define OMIT_JUMP_TABLE_CLASS_STATIC_LVAL_%s 1\n", clsName);
  }
  if (constants->hasJumpTable()) {
    cg_printf("static Variant os_constant(const char *s);\n");
  } else {
    cg_printf("#define OMIT_JUMP_TABLE_CLASS_CONSTANT_%s 1\n", clsName);
  }

  cg.printSection("DECLARE_INSTANCE_PROP_OPS");
  cg_printf("public:\n");

  if (variables->hasJumpTable(VariableTable::JumpTableClassGetArray)) {
    cg_printf("virtual void o_getArray(Array &props) const;\n");
  } else {
    cg_printf("#define OMIT_JUMP_TABLE_CLASS_GETARRAY_%s 1\n", clsName);
  }
  if (variables->hasJumpTable(VariableTable::JumpTableClassSetArray)) {
    cg_printf("virtual void o_setArray(CArrRef props);\n");
  } else {
    cg_printf("#define OMIT_JUMP_TABLE_CLASS_SETARRAY_%s 1\n", clsName);
  }

  if (variables->hasJumpTable(VariableTable::JumpTableClassRealProp)) {
    cg_printf("virtual Variant *o_realProp(CStrRef s, int flags,\n");
    cg_printf("                            CStrRef context = null_string) "
              "const;\n");
  } else {
    cg_printf("#define OMIT_JUMP_TABLE_CLASS_realProp_%s 1\n", clsName);
  }
  if (variables->hasNonStaticPrivate()) {
    cg_printf("Variant *o_realPropPrivate(CStrRef s, int flags) const;\n");
  } else {
    cg_printf("#define OMIT_JUMP_TABLE_CLASS_realProp_PRIVATE_%s 1\n", clsName);
  }

  cg.printSection("DECLARE_INSTANCE_PUBLIC_PROP_OPS");
  cg_printf("public:\n");
  if (variables->hasJumpTable(VariableTable::JumpTableClassRealPropPublic)) {
    cg_printf("virtual Variant *o_realPropPublic(CStrRef s, "
              "int flags) const;\n");
  } else {
    cg_printf("#define OMIT_JUMP_TABLE_CLASS_realProp_PUBLIC_%s 1\n", clsName);
  }

  cg.printSection("DECLARE_COMMON_INVOKE");
  if (classScope->hasJumpTable(ClassScope::JumpTableStaticInvoke)) {
    cg_printf("static Variant os_invoke(const char *c, "
              "MethodIndex methodIndex,\n");
    cg_printf("                         const char *s, CArrRef ps, int64 h, "
              "bool f = true);\n");
  } else {
    cg_printf("#define OMIT_JUMP_TABLE_CLASS_STATIC_INVOKE_%s 1\n", clsName);
  }
  if (classScope->hasJumpTable(ClassScope::JumpTableInvoke)) {
    cg_printf("virtual Variant o_invoke(MethodIndex methodIndex, "
              "const char *s, CArrRef ps,\n");
    cg_printf("                         int64 h, bool f = true);\n");
    cg_printf("virtual Variant o_invoke_few_args(MethodIndex methodIndex, "
              "const char *s,\n");
    cg_printf("                                  int64 h, int count,\n");
    cg_printf("                                  "
              "INVOKE_FEW_ARGS_DECL_ARGS);\n");
  } else {
    cg_printf("#define OMIT_JUMP_TABLE_CLASS_INVOKE_%s 1\n", clsName);
  }

  cg_printf("\n");
  cg_printf("public:\n");
}

void ClassStatement::outputCPPImpl(CodeGenerator &cg, AnalysisResultPtr ar) {
  ClassScopePtr classScope = m_classScope.lock();
  if (cg.getContext() == CodeGenerator::NoContext) {
    if (classScope->isRedeclaring()) {
      cg_printf("g->%s%s = ClassStaticsPtr(NEW(%s%s)());\n",
                Option::ClassStaticsObjectPrefix,
                cg.formatLabel(m_name).c_str(),
                Option::ClassStaticsPrefix, classScope->getId(cg).c_str());
      cg_printf("g->%s%s = &%s%s;\n",
                Option::ClassStaticsCallbackPrefix,
                cg.formatLabel(m_name).c_str(),
                Option::ClassWrapperFunctionPrefix,
                classScope->getId(cg).c_str());
    }
    if (classScope->isVolatile()) {
      cg_printf("g->CDEC(%s) = true;\n", m_name.c_str());
    }
    const vector<string> &bases = classScope->getBases();
    for (vector<string>::const_iterator it = bases.begin();
         it != bases.end(); ++it) {
      ClassScopePtr base = ar->findClass(*it);
      if (base && base->isVolatile()) {
        cg_printf("checkClassExists(");
        cg_printString(base->getOriginalName(), ar);
        string lname = Util::toLower(base->getOriginalName());
        cg_printf(", &%s->CDEC(%s), %s->FVF(__autoload));\n",
                  cg.getGlobals(ar), cg.formatLabel(lname).c_str(),
                  cg.getGlobals(ar));
      }
    }
    return;
  }

  if (cg.getContext() != CodeGenerator::CppForwardDeclaration) {
    printSource(cg);
  }

  ar->pushScope(classScope);
  string clsNameStr = classScope->getId(cg);
  const char *clsName = clsNameStr.c_str();
  bool redeclared = classScope->isRedeclaring();
  switch (cg.getContext()) {
  case CodeGenerator::CppForwardDeclaration:
    if (Option::GenerateCPPMacros) {
      cg_printf("FORWARD_DECLARE_CLASS(%s);\n", clsName);
      if (redeclared) {
        cg_printf("FORWARD_DECLARE_REDECLARED_CLASS(%s);\n", clsName);
      }
    }
    if (m_stmt) {
      cg.setContext(CodeGenerator::CppClassConstantsDecl);
      m_stmt->outputCPP(cg, ar);
      cg.setContext(CodeGenerator::CppForwardDeclaration);
    }
    break;
  case CodeGenerator::CppDeclaration:
    {
      bool system = cg.getOutput() == CodeGenerator::SystemCPP;
      ClassScopePtr parCls;
      if (!m_parent.empty()) {
        parCls = ar->findClass(m_parent);
        if (parCls && parCls->isRedeclaring()) parCls.reset();
      }
      if (Option::GenerateCppLibCode) {
        cg.printDocComment(classScope->getDocComment());
      }
      cg_printf("class %s%s", Option::ClassPrefix, clsName);
      if (!m_parent.empty() && classScope->derivesDirectlyFrom(ar, m_parent)) {
        if (!parCls) {
          cg_printf(" : public DynamicObjectData");
        } else {
          cg_printf(" : public %s%s", Option::ClassPrefix,
                    parCls->getId(cg).c_str());
        }
      } else {
        if (classScope->derivesFromRedeclaring()) {
          cg_printf(" : public DynamicObjectData");
        } else if (system) {
          cg_printf(" : public ExtObjectData");
        } else {
          cg_printf(" : public ObjectData");
        }
      }
      if (m_base && Option::UseVirtualDispatch) {
        for (int i = 0; i < m_base->getCount(); i++) {
          ScalarExpressionPtr exp =
            dynamic_pointer_cast<ScalarExpression>((*m_base)[i]);
          const char *intf = exp->getString().c_str();
          ClassScopePtr intfClassScope = ar->findClass(intf);
          if (intfClassScope && !intfClassScope->isRedeclaring() &&
              classScope->derivesDirectlyFrom(ar, intf) &&
              (!parCls || !parCls->derivesFrom(ar, intf, true, false))) {
            string id = intfClassScope->getId(cg);
            cg_printf(", public %s%s", Option::ClassPrefix, id.c_str());
          }
        }
      }
      cg_indentBegin(" {\n");
      cg_printf("public:\n");

      cg.printSection("Properties");
      classScope->getVariables()->outputCPPPropertyDecl(cg, ar,
          classScope->derivesFromRedeclaring());

      if (Option::GenerateCppLibCode) {
        cg.printSection("Methods");
        classScope->outputMethodWrappers(cg, ar);
        cg.printSection(">>>>>>>>>> Internal Implementation <<<<<<<<<<");
        cg_printf("// NOTE: Anything below is subject to change. "
                  "Use everything above instead.\n");
      }

      cg.printSection("Class Map");
      if (Option::GenerateCPPMacros) {
        // Get all of this class's ancestors
        vector<string> bases;
        getAllParents(ar, bases);
        // Eliminate duplicates
        sort(bases.begin(), bases.end());
        bases.erase(unique(bases.begin(), bases.end()), bases.end());

        cg_indentBegin("BEGIN_CLASS_MAP(%s)\n",
                       classScope->getOriginalName().c_str());
        for (unsigned int i = 0; i < bases.size(); i++) {
          ClassScopePtr baseCls = ar->findClass(bases[i]);
          cg_printf("PARENT_CLASS(%s)\n",
                    (baseCls ? baseCls->getOriginalName() : bases[i]).c_str());
        }
        if (classScope->derivesFromRedeclaring()) {
          cg_printf("CLASS_MAP_REDECLARED()\n");
        }
        cg_indentEnd("END_CLASS_MAP(%s)\n",
                     classScope->getOriginalName().c_str());
      }

      if (Option::GenerateCPPMacros) {
        bool dyn = (!parCls && !m_parent.empty()) ||
          classScope->derivesFromRedeclaring() ==
          ClassScope::DirectFromRedeclared;
        bool idyn = parCls && classScope->derivesFromRedeclaring() ==
          ClassScope::IndirectFromRedeclared;
        bool redec = classScope->isRedeclaring();
        if (!classScope->derivesFromRedeclaring()) {
          outputCPPClassDecl(cg, ar, clsName, m_originalName.c_str(),
                             parCls ? parCls->getId(cg).c_str()
                                    : "ObjectData");
        } else {
          cg_printf("DECLARE_DYNAMIC_CLASS(%s, %s, %s)\n", clsName,
                    m_originalName.c_str(),
                    dyn || !parCls ? "DynamicObjectData" :
                    parCls->getId(cg).c_str());
        }
        if (system || Option::EnableEval >= Option::LimitedEval) {
          cg_printf("DECLARE_INVOKES_FROM_EVAL\n");
        }

        bool hasGet = classScope->getAttribute(
          ClassScope::HasUnknownPropGetter);
        bool hasSet = classScope->getAttribute(
          ClassScope::HasUnknownPropSetter);

        if (dyn || idyn || redec || hasGet || hasSet) {
          if (redec) {
            cg_printf("DECLARE_ROOT;\n");
             if (!dyn && !idyn) {
               cg_printf("private: ObjectData* root;\n");
               cg_printf("public:\n");
               cg_printf("virtual ObjectData *getRoot() { return root; }\n");
             }
          }

          string conInit = "";
          if (dyn) {
            conInit = ":DynamicObjectData(\"" + m_parent + "\", r)";
          } else if (idyn) {
            conInit = ":" + string(Option::ClassPrefix) + parCls->getId(cg) +
              "(r?r:this)";
          } else if (redec) {
            conInit = ":root(r?r:this)";
          }

          cg_indentBegin("%s%s(%s)%s {%s",
                         Option::ClassPrefix, clsName,
                         conInit.empty() ? "" : "ObjectData* r = NULL",
                         conInit.c_str(),
                         hasGet || hasSet ? "\n" : "");
          if (hasGet) cg_printf("setAttribute(UseGet);\n");
          if (hasSet) cg_printf("setAttribute(UseSet);\n");
          cg_indentEnd("}\n");
        }
      }

      cg_printf("void init();\n");

      if (classScope->needLazyStaticInitializer()) {
        cg_printf("static GlobalVariables *lazy_initializer"
                  "(GlobalVariables *g);\n");
      }

      if (!classScope->getAttribute(ClassScope::HasConstructor)) {
        FunctionScopePtr func = classScope->findFunction(ar, "__construct",
                                                         false);
        if (func && !func->isAbstract() && !classScope->isInterface()) {
          ar->pushScope(func);
          func->outputCPPCreateDecl(cg, ar);
          ar->popScope();
        }
      }
      if (classScope->getAttribute(ClassScope::HasDestructor)) {
        cg_printf("public: virtual void destruct();\n");
      }

      // doCall
      if (classScope->getAttribute(ClassScope::HasUnknownMethodHandler)) {
        cg_printf("Variant doCall(Variant v_name, Variant v_arguments, "
                  "bool fatal);\n");
      }

      if (classScope->isRedeclaring() &&
          !classScope->derivesFromRedeclaring()) {
        cg_printf("Variant doRootCall(Variant v_name, Variant v_arguments, "
                  "bool fatal);\n");
      }

      if (m_stmt) m_stmt->outputCPP(cg, ar);
      {
        set<string> done;
        classScope->outputCPPStaticMethodWrappers(cg, ar, done, clsName);
      }

      if (cg.getOutput() == CodeGenerator::SystemCPP &&
          ar->isBaseSysRsrcClass(clsName) &&
          !classScope->hasProperty("rsrc")) {
        cg_printf("public: Variant %srsrc;\n", Option::PropertyPrefix);
      }

      cg_indentEnd("};\n");

      if (redeclared) {
        cg_indentBegin("class %s%s : public ClassStatics {\n",
                       Option::ClassStaticsPrefix, clsName);
        cg_printf("public:\n");
        cg_printf("DECLARE_OBJECT_ALLOCATION(%s%s);\n",
                  Option::ClassStaticsPrefix, clsName);
        cg_printf("%s%s() : ClassStatics(%d) {}\n",
                  Option::ClassStaticsPrefix, clsName,
                  classScope->getRedeclaringId());
        cg_indentBegin("Variant %sgetInit(CStrRef s) {\n",
                       Option::ObjectStaticPrefix);
        cg_printf("return %s%s::%sgetInit(s);\n", Option::ClassPrefix,
                  clsName, Option::ObjectStaticPrefix);
        cg_indentEnd("}\n");
        cg_indentBegin("Variant %sget(CStrRef s) {\n",
                       Option::ObjectStaticPrefix);
        cg_printf("return %s%s::%sget(s);\n", Option::ClassPrefix,
                  clsName, Option::ObjectStaticPrefix);
        cg_indentEnd("}\n");
        cg_indentBegin("Variant &%slval(CStrRef s) {\n",
                  Option::ObjectStaticPrefix);
        cg_printf("return %s%s::%slval(s);\n", Option::ClassPrefix,
                  clsName, Option::ObjectStaticPrefix);
        cg_indentEnd("}\n");
        cg_indentBegin("Variant %sinvoke(const char *c, "
                       "MethodIndex methodIndex,"
                       "const char *s, "
                       "CArrRef params, int64 hash = -1, bool fatal = true) "
                       "{\n",
                       Option::ObjectStaticPrefix);
        cg_printf("return %s%s::%sinvoke(c, methodIndex, s, params, hash, "
                  "fatal);\n",
                  Option::ClassPrefix, clsName,
                  Option::ObjectStaticPrefix);
        cg_indentEnd("}\n");
        cg_indentBegin("Object create(CArrRef params, bool init = true, "
                       "ObjectData* root = NULL) {\n");
        cg_printf("return Object((NEW(%s%s)(root))->"
                  "dynCreate(params, init));\n",
                  Option::ClassPrefix, clsName);
        cg_indentEnd("}\n");
        cg_indentBegin("Variant %sconstant(const char* s) {\n",
                       Option::ObjectStaticPrefix);
        cg_printf("return %s%s::%sconstant(s);\n", Option::ClassPrefix,
                  clsName, Option::ObjectStaticPrefix);
        cg_indentEnd("}\n");
        cg_indentBegin("Variant %sinvoke_from_eval(const char *c, "
                       "const char *s, Eval::VariableEnvironment &env, "
                       "const Eval::FunctionCallExpression *call, "
                       "int64 hash = -1, bool fatal = true) "
                       "{\n",
                       Option::ObjectStaticPrefix);
        cg_printf("return %s%s::%sinvoke_from_eval(c, s, env, call, hash, "
                  "fatal);\n",
                  Option::ClassPrefix, clsName,
                  Option::ObjectStaticPrefix);
        cg_indentEnd("}\n");
        cg_indentEnd("};\n");
      }

      classScope->outputCPPGlobalTableWrappersDecl(cg, ar);
    }
    break;
  case CodeGenerator::CppImplementation:
    if (m_stmt) {
      cg.setContext(CodeGenerator::CppClassConstantsImpl);
      m_stmt->outputCPP(cg, ar);
      cg.setContext(CodeGenerator::CppImplementation);
    }

    classScope->outputCPPSupportMethodsImpl(cg, ar);

    if (redeclared) {
      cg_printf("IMPLEMENT_OBJECT_ALLOCATION(%s%s);\n",
                Option::ClassStaticsPrefix, clsName);
    }

    cg_indentBegin("void %s%s::init() {\n",
                   Option::ClassPrefix, clsName);
    if (!m_parent.empty()) {
      if (classScope->derivesFromRedeclaring() ==
          ClassScope::DirectFromRedeclared) {
        cg_printf("parent->init();\n");
      } else {

        ClassScopePtr parCls = ar->findClass(m_parent);
        cg_printf("%s%s::init();\n", Option::ClassPrefix,
                  parCls->getId(cg).c_str());
      }
    }
    if (classScope->getVariables()->
        getAttribute(VariableTable::NeedGlobalPointer)) {
      cg.printDeclareGlobals();
    }
    cg.setContext(CodeGenerator::CppConstructor);
    if (m_stmt) m_stmt->outputCPP(cg, ar);

    // This is lame. Exception base class needs to prepare stacktrace outside
    // of its PHP constructor. Every subclass of exception also needs this
    // stacktrace, so we're adding an artificial __init__ in exception.php
    // and calling it here.
    if (m_name == "exception") {
      cg_printf("{CountableHelper h(this); t___init__();}\n");
    }

    cg_indentEnd("}\n");

    if (classScope->needStaticInitializer()) {
      cg_indentBegin("void %s%s::os_static_initializer() {\n",
                     Option::ClassPrefix, clsName);
      cg.printDeclareGlobals();
      cg.setContext(CodeGenerator::CppStaticInitializer);
      if (m_stmt) m_stmt->outputCPP(cg, ar);
      cg_indentEnd("}\n");
      cg_indentBegin("void %s%s() {\n",
                     Option::ClassStaticInitializerPrefix, clsName);
      cg_printf("%s%s::os_static_initializer();\n",  Option::ClassPrefix,
                clsName);
      cg_indentEnd("}\n");
    }
    if (classScope->needLazyStaticInitializer()) {
      cg_indentBegin("GlobalVariables *%s%s::lazy_initializer("
                     "GlobalVariables *g) {\n", Option::ClassPrefix, clsName);
      cg_indentBegin("if (!g->%s%s) {\n",
                     Option::ClassStaticInitializerFlagPrefix, clsName);
      cg_printf("g->%s%s = true;\n", Option::ClassStaticInitializerFlagPrefix,
                clsName);
      cg.setContext(CodeGenerator::CppLazyStaticInitializer);
      if (m_stmt) m_stmt->outputCPP(cg, ar);
      cg_indentEnd("}\n");
      cg_printf("return g;\n");
      cg_indentEnd("}\n");
    }
    cg.setContext(CodeGenerator::CppImplementation);
    if (m_stmt) m_stmt->outputCPP(cg, ar);

    break;
  case CodeGenerator::CppFFIDecl:
  case CodeGenerator::CppFFIImpl:
    if (m_stmt) m_stmt->outputCPP(cg, ar);
    break;
  case CodeGenerator::JavaFFI:
    {
      if (classScope->isRedeclaring()) break;

      // TODO support PHP namespaces, once HPHP supports it
      string packageName = Option::JavaFFIRootPackage;
      string packageDir = packageName;
      Util::replaceAll(packageDir, ".", "/");

      string outputDir = ar->getOutputPath() + "/" + Option::FFIFilePrefix +
        packageDir + "/";
      Util::mkdir(outputDir);

      // uses a different cg to generate a separate file for each PHP class
      // also, uses the original capitalized class name
      string clsFile = outputDir + getOriginalName() + ".java";
      ofstream fcls(clsFile.c_str());
      CodeGenerator cgCls(&fcls, CodeGenerator::FileCPP);
      cgCls.setContext(CodeGenerator::JavaFFI);

      cgCls.printf("package %s;\n\n", packageName.c_str());
      cgCls.printf("import hphp.*;\n\n");

      printSource(cgCls);

      string clsModifier;
      switch (m_type) {
      case T_CLASS:
        break;
      case T_ABSTRACT:
        clsModifier = "abstract ";
        break;
      case T_FINAL:
        clsModifier = "final ";
        break;
      }
      cgCls.printf("public %sclass %s ", clsModifier.c_str(),
                   getOriginalName().c_str());

      ClassScopePtr parCls;
      if (!m_parent.empty()) parCls = ar->findClass(m_parent);
      if (!m_parent.empty() && classScope->derivesDirectlyFrom(ar, m_parent)
          && parCls && parCls->isUserClass() && !parCls->isRedeclaring()) {
        // system classes are not supported in static FFI translation
        // they shouldn't appear as superclasses as well
        cgCls.printf("extends %s", parCls->getOriginalName().c_str());
      }
      else {
        cgCls.printf("extends HphpObject");
      }
      if (m_base) {
        bool first = true;
        for (int i = 0; i < m_base->getCount(); i++) {
          ScalarExpressionPtr exp =
            dynamic_pointer_cast<ScalarExpression>((*m_base)[i]);
          const char *intf = exp->getString().c_str();
          ClassScopePtr intfClassScope = ar->findClass(intf);
          if (intfClassScope && classScope->derivesFrom(ar, intf, false, false)
           && intfClassScope->isUserClass()) {
            if (first) {
              cgCls.printf(" implements ");
              first = false;
            }
            else {
              cgCls.printf(", ");
            }
            cgCls.printf(intfClassScope->getOriginalName().c_str());
          }
        }
      }

      cgCls.indentBegin(" {\n");

      // constructor for initializing the variant pointer
      cgCls.printf("protected %s(long ptr) { super(ptr); }\n\n",
                   getOriginalName().c_str());

      FunctionScopePtr cons = classScope->findConstructor(ar, true);
      if (cons && !cons->isAbstract() || m_type != T_ABSTRACT) {
        // if not an abstract class and not having an explicit constructor,
        // adds a default constructor
        outputJavaFFIConstructor(cgCls, ar, cons);
      }

      if (m_stmt) m_stmt->outputCPP(cgCls, ar);
      cgCls.indentEnd("}\n");

      fcls.close();
    }
    break;
  case CodeGenerator::JavaFFICppDecl:
  case CodeGenerator::JavaFFICppImpl:
    {
      if (classScope->isRedeclaring()) break;

      if (m_stmt) m_stmt->outputCPP(cg, ar);
      FunctionScopePtr cons = classScope->findConstructor(ar, true);
      if (cons && !cons->isAbstract() || m_type != T_ABSTRACT) {
        outputJavaFFICPPCreator(cg, ar, cons);
      }
    }
    break;
  default:
    ASSERT(false);
    break;
  }

  ar->popScope();
}

void ClassStatement::outputJavaFFIConstructor(CodeGenerator &cg,
                                              AnalysisResultPtr ar,
                                              FunctionScopePtr cons) {
  int ac = cons ? cons->getMaxParamCount() : 0;
  bool varArgs = cons && cons->isVariableArgument();

  // generates the constructor
  cg_printf("public %s(", getOriginalName().c_str());
  ostringstream args;
  ostringstream params;
  bool first = true;
  for (int i = 0; i < ac; i++) {
    if (first) {
      first = false;
    }
    else {
      cg_printf(", ");
      args << ", ";
      params << ", ";
    }
    cg_printf("HphpVariant a%d", i);
    args << "a" << i << ".getVariantPtr()";
    params << "long a" << i;
  }
  if (varArgs) {
    if (!first) {
      cg_printf(", ");
      args << ", ";
      params << ", ";
    }
    cg_printf("HphpVariant va");
    args << "va.getVariantPtr()";
    params << "long va";
  }
  cg_indentBegin(") {\n");
  cg_printf("this(create(%s));\n", args.str().c_str());
  cg_indentEnd("}\n\n");

  // generates the native method stub for creating the object
  cg_printf("private static native long create(%s);\n\n",
            params.str().c_str());
}

void ClassStatement::outputJavaFFICPPCreator(CodeGenerator &cg,
                                             AnalysisResultPtr ar,
                                             FunctionScopePtr cons) {
  ClassScopePtr cls = m_classScope.lock();
  string packageName = Option::JavaFFIRootPackage;
  int ac = cons ? cons->getMaxParamCount() : 0;
  bool varArgs = cons && cons->isVariableArgument();
  const char *clsName = getOriginalName().c_str();

  string mangledName = "Java_" + packageName + "_" + clsName + "_create";
  Util::replaceAll(mangledName, ".", "_");

  cg_printf("JNIEXPORT jlong JNICALL\n");
  cg_printf("%s(JNIEnv *env, jclass cls", mangledName.c_str());

  ostringstream args;
  bool first = true;
  if (varArgs) {
    args << ac << " + (((Variant *)va)->isNull() ? 0"
               << " : ((Variant *)va)->getArrayData()->size())";
    first = false;
  }
  for (int i = 0; i < ac; i++) {
    if (first) first = false;
    else {
      args << ", ";
    }
    cg_printf(", jlong a%d", i);
    args << "*(Variant *)a" << i;
  }
  if (varArgs) {
    if (!first) {
      args << ", ";
    }
    cg_printf(", jlong va");
    args << "((Variant *)va)->toArray()";
  }

  if (cg.getContext() == CodeGenerator::JavaFFICppDecl) {
    // java_stubs.h
    cg_printf(");\n\n");
    return;
  }

  cg_indentBegin(") {\n");
  cg_printf("ObjectData *obj = (NEW(%s%s)())->create(%s);\n",
            Option::ClassPrefix, cls->getId(cg).c_str(), args.str().c_str());
  cg_printf("return (jlong)(NEW(Variant)(obj));\n");
  cg_indentEnd("}\n\n");
}
