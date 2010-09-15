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

#include <compiler/expression/expression.h>
#include <compiler/analysis/code_error.h>
#include <compiler/parser/parser.h>
#include <compiler/parser/hphp.tab.hpp>
#include <util/util.h>
#include <compiler/analysis/class_scope.h>
#include <compiler/analysis/function_scope.h>
#include <compiler/expression/scalar_expression.h>
#include <compiler/expression/constant_expression.h>
#include <compiler/expression/expression_list.h>
#include <compiler/expression/simple_variable.h>
#include <compiler/expression/assignment_expression.h>
#include <compiler/expression/array_pair_expression.h>
#include <compiler/expression/unary_op_expression.h>
#include <compiler/analysis/constant_table.h>
#include <compiler/analysis/variable_table.h>
#include <compiler/expression/function_call.h>
#include <compiler/analysis/file_scope.h>
#include <util/hash.h>

using namespace HPHP;
using namespace std;
using namespace boost;

///////////////////////////////////////////////////////////////////////////////

Expression::Expression(LocationPtr loc, KindOf kindOf)
  : Construct(loc), m_kindOf(kindOf), m_context(RValue),
    m_originalScopeSet(false), m_canon_id(0), m_canonPtr(), m_error(0),
    m_unused(false) {
}

ExpressionPtr Expression::replaceValue(ExpressionPtr rep) {
  if (hasContext(Expression::RefValue) &&
      isRefable(true) && !rep->isRefable(true)) {
    /*
      An assignment isRefable, but the rhs may not be. Need this to
      prevent "bad pass by reference" errors.
    */
    ExpressionListPtr el(new ExpressionList(getLocation(),
                                            Expression::KindOfExpressionList,
                                            ExpressionList::ListKindWrapped));
    el->addElement(rep);
    rep = el;
  }
  rep->copyContext(m_context & ~DeadStore);
  if (TypePtr t1 = getType()) {
    if (TypePtr t2 = rep->getType()) {
      if (!Type::SameType(t1, t2)) {
        rep->setExpectedType(t1);
      }
    }
  }

  return rep;
}

void Expression::copyContext(int contexts) {
  unsigned val = contexts;
  while (val) {
    unsigned next = val & (val - 1);
    unsigned low = val ^ next; // lowest set bit
    setContext((Context)low);
    val = next;
  }
}

void Expression::clearContext() {
  unsigned val = m_context;
  while (val) {
    unsigned next = val & (val - 1);
    unsigned low = val ^ next; // lowest set bit
    clearContext((Context)low);
    val = next;
  }
}

void Expression::deepCopy(ExpressionPtr exp) {
  exp->m_actualType = m_actualType;
  exp->m_expectedType = m_expectedType;
  exp->m_canon_id = 0;
  exp->m_unused = false;
  exp->m_canonPtr.reset();
  exp->m_replacement.reset();
};

bool Expression::hasSubExpr(ExpressionPtr sub) const {
  if (this == sub.get()) return true;
  for (int i = getKidCount(); i--; ) {
    ExpressionPtr kid = getNthExpr(i);
    if (kid && kid->hasSubExpr(sub)) return true;
  }
  return false;
}

void Expression::addElement(ExpressionPtr exp) {
  ASSERT(false);
}

void Expression::insertElement(ExpressionPtr exp, int index /* = 0 */) {
  ASSERT(false);
}

ExpressionPtr Expression::unneededHelper(AnalysisResultPtr ar) {
  ExpressionListPtr elist = ExpressionListPtr
    (new ExpressionList(getLocation(), Expression::KindOfExpressionList,
                        ExpressionList::ListKindWrapped));

  bool change = false;
  for (int i=0, n = getKidCount(); i < n; i++) {
    ExpressionPtr kid = getNthExpr(i);
    if (kid && kid->getContainedEffects()) {
      ExpressionPtr rep = kid->unneeded(ar);
      if (rep != kid) change = true;
      if (rep->is(Expression::KindOfExpressionList)) {
        for (int j=0, m = rep->getKidCount(); j < m; j++) {
          elist->addElement(rep->getNthExpr(j));
        }
      } else {
        elist->addElement(rep);
      }
    }
  }

  if (change) {
    ar->incOptCounter();
  }

  int n = elist->getCount();
  ASSERT(n);
  if (n == 1) {
    return elist->getNthExpr(0);
  } else {
    return elist;
  }
}

ExpressionPtr Expression::unneeded(AnalysisResultPtr ar) {
  if (getLocalEffects() || is(KindOfScalarExpression)) {
    return static_pointer_cast<Expression>(shared_from_this());
  }
  if (!getContainedEffects()) {
    ar->incOptCounter();
    return ScalarExpressionPtr
      (new ScalarExpression(getLocation(),
                            Expression::KindOfScalarExpression,
                            T_LNUMBER, string("0")));
  }

  return unneededHelper(ar);
}

///////////////////////////////////////////////////////////////////////////////

bool Expression::IsIdentifier(const string &value) {
  if (value.empty()) {
    return false;
  }
  unsigned char ch = value[0];
  if ((ch < 'a' || ch > 'z') && (ch < 'A' || ch > 'Z') &&
      ch < '\x7f' && ch != '_') {
    return false;
  }
  for (unsigned int i = 1; i < value.size(); i++) {
    unsigned char ch = value[i];
    if (((ch < 'a' || ch > 'z') && (ch < 'A' || ch > 'Z') &&
         (ch < '0' || ch > '9') && ch < '\x7f' && ch != '_')) {
      return false;
    }
  }
  return true;
}

TypePtr Expression::getType() {
  if (m_expectedType) return m_expectedType;
  if (m_actualType) return m_actualType;
  return NEW_TYPE(Any);
}

TypePtr Expression::propagateTypes(AnalysisResultPtr ar, TypePtr inType) {
  ExpressionPtr e = this->m_canonPtr;
  TypePtr ret = inType;

  while (e) {
    TypePtr inferred = Type::Inferred(ar, ret, e->m_actualType);
    if (!inferred) {
      assert(ar->getPhase() != AnalysisResult::LastInference);
    } else {
      ret = inferred;
    }
    e = e->m_canonPtr;
  }

  return ret;
}

void Expression::analyzeProgram(AnalysisResultPtr ar) {
  if (ar->isFirstPass()) {
    m_originalScope = ar->getClassScope();
    m_originalScopeSet = true;
  }
}

ClassScopePtr Expression::getOriginalScope(AnalysisResultPtr ar) {
  if (!m_originalScopeSet) {
    m_originalScopeSet = true;
    m_originalScope = ar->getClassScope();
  }
  return m_originalScope.lock();
}

TypePtr Expression::inferAndCheck(AnalysisResultPtr ar, TypePtr type,
                                  bool coerce) {
  TypePtr actualType = inferTypes(ar, type, coerce);
  if (type->is(Type::KindOfSome) || type->is(Type::KindOfAny)) {
    m_actualType = actualType;
    m_expectedType.reset();
    return actualType;
  }
  return checkTypesImpl(ar, type, actualType, coerce);
}

TypePtr Expression::checkTypesImpl(AnalysisResultPtr ar, TypePtr expectedType,
                                   TypePtr actualType, bool coerce) {
  TypePtr ret;
  actualType = propagateTypes(ar, actualType);
  if (coerce) {
    ret = Type::Coerce(ar, expectedType, actualType);
    setTypes(actualType, expectedType);
  } else {
    if (!Type::IsLegalCast(ar, actualType, expectedType)) {
      ar->getCodeError()->record(shared_from_this(), expectedType->getKindOf(),
                                 actualType->getKindOf());
    }
    ret = Type::Intersection(ar, actualType, expectedType);
    setTypes(actualType, ret);
  }
  return ret;
}

void Expression::setTypes(TypePtr actualType, TypePtr expectedType) {
  m_actualType = actualType;
  if (!Type::SameType(expectedType, actualType)) {
    m_expectedType = expectedType;
  } else {
    // Clear expected type since expectedType == actualType
    m_expectedType.reset();
  }

  // This is a special case where Type::KindOfObject means any object.
  if (m_expectedType && m_expectedType->is(Type::KindOfObject) &&
      !m_expectedType->isSpecificObject() &&
      m_actualType->isSpecificObject()) {
    m_expectedType.reset();
  }
}

void Expression::setDynamicByIdentifier(AnalysisResultPtr ar,
                                        const std::string &value) {
  string id = Util::toLower(value);
  size_t c = id.find("::");
  FunctionScopePtr fi;
  ClassScopePtr ci;
  if (c != 0 && c != string::npos && c+2 < id.size()) {
    string cl = id.substr(0, c);
    string fn = id.substr(c+2);
    if (IsIdentifier(cl) && IsIdentifier(fn)) {
      ci = ar->findClass(cl);
      if (ci) {
        fi = ci->findFunction(ar, fn, false);
        if (fi) fi->setDynamic();
      }
    }
  } else if (IsIdentifier(id)) {
    fi = ar->findFunction(id);
    if (fi) {
      fi->setDynamic();
    }
    ClassScopePtr ci = ar->findClass(id, AnalysisResult::MethodName);
    if (ci) {
      fi = ci->findFunction(ar, id, false);
      if (fi) {
        fi->setDynamic();
      }
    }
  }
}

bool Expression::CheckNeeded(AnalysisResultPtr ar,
                             ExpressionPtr variable, ExpressionPtr value) {
  // if the value may involve object, consider the variable as "needed"
  // so that objects are not destructed prematurely.
  bool needed = true;
  if (value) {
    while (value->is(KindOfAssignmentExpression)) {
      value = dynamic_pointer_cast<AssignmentExpression>(value)->getValue();
    }
    if (value->isScalar()) {
      needed = false;
    } else {
      TypePtr type = value->getType();
      if (type && (type->is(Type::KindOfSome) || type->is(Type::KindOfAny))) {
        type = value->getActualType();
      }
      if (type && type->isNoObjectInvolved()) needed = false;
    }
  }
  if (variable->is(Expression::KindOfSimpleVariable)) {
    SimpleVariablePtr var =
      dynamic_pointer_cast<SimpleVariable>(variable);
    const std::string &name = var->getName();
    VariableTablePtr variables = ar->getScope()->getVariables();
    if (needed) {
      variables->addNeeded(name);
    } else {
      needed = variables->isNeeded(name);
    }
  }
  return needed;
}

TypePtr Expression::inferAssignmentTypes(AnalysisResultPtr ar, TypePtr type,
                                         bool coerce, ExpressionPtr variable,
                                         ExpressionPtr
                                         value /* =ExpressionPtr() */) {
  TypePtr ret = type;
  if (value) {
    ret = value->inferAndCheck(ar, NEW_TYPE(Some), false);
  }

  BlockScopePtr scope = ar->getScope();
  if (variable->is(Expression::KindOfConstantExpression)) {
    // ...as in ClassConstant statement
    ConstantExpressionPtr exp =
      dynamic_pointer_cast<ConstantExpression>(variable);
    BlockScope *defScope = NULL;
    std::vector<std::string> bases;
    scope->getConstants()->check(exp->getName(), ret, true, ar, variable,
                                 bases, defScope);
  } else if (variable->is(Expression::KindOfDynamicVariable)) {
    // simptodo: not too sure about this
    ar->getFileScope()->setAttribute(FileScope::ContainsLDynamicVariable);
  } else if (variable->is(Expression::KindOfSimpleVariable)) {
    SimpleVariablePtr var = dynamic_pointer_cast<SimpleVariable>(variable);
    if (var->getName() == "this" && ar->getClassScope()) {
      if (ar->isFirstPass()) {
        ar->getCodeError()->record(variable, CodeError::ReassignThis,
                                   variable);
      }
    }
    if (ar->getPhase() == AnalysisResult::LastInference && value) {
      if (!value->getExpectedType()) {
        value->setExpectedType(variable->getActualType());
      }
    }
  }

  if (ar->getPhase() == AnalysisResult::LastInference) {
    CheckNeeded(ar, variable, value);
  }

  m_implementedType.reset();
  TypePtr vt = variable->inferAndCheck(ar, ret, true);
  if (!coerce && type->is(Type::KindOfAny)) {
    ret = vt;
  } else {
    TypePtr it = variable->is(KindOfObjectPropertyExpression) ?
      Type::Variant :
      variable->getImplementedType();
    if (!it) it = vt;
    if (!Type::SameType(it, ret)) {
      m_implementedType = it;
    }
  }

  return ret;
}

ExpressionPtr Expression::MakeConstant(AnalysisResultPtr ar,
                                       LocationPtr loc,
                                       const std::string &value) {
  ConstantExpressionPtr exp(new ConstantExpression(loc,
                            Expression::KindOfConstantExpression,
                            value));
  if (value == "true" || value == "false") {
    if (ar->getPhase() >= AnalysisResult::PostOptimize) {
      exp->m_actualType = Type::Boolean;
    }
  } else if (value == "null") {
    if (ar->getPhase() >= AnalysisResult::PostOptimize) {
      exp->m_actualType = Type::Variant;
    }
  } else {
    ASSERT(false);
  }
  return exp;
}

void Expression::CheckPassByReference(AnalysisResultPtr ar,
                                      ExpressionPtr param) {
  if (param->hasContext(Expression::RefValue) &&
      !param->isRefable(true)) {
    param->setError(Expression::BadPassByRef);
    ar->getCodeError()->record(CodeError::BadPassByReference, param);
  }
}

unsigned Expression::getCanonHash() const {
  int64 val = hash_int64(getKindOf());
  for (int i = getKidCount(); i--; ) {
    ExpressionPtr k = getNthExpr(i);
    if (k) {
      val = hash_int64(val ^ (((int64)k->getKindOf()<<32)+k->getCanonID()));
    }
  }

  return (unsigned)val ^ (unsigned)(val >> 32);
}

bool Expression::canonCompare(ExpressionPtr e) const {
  if (e->getKindOf() != getKindOf()) {
    return false;
  }

  int c1 = getContext();
  int c2 = e->getContext();
  if ((c1 ^ c2) & ExistContext) {
    return false;
  }

  int kk = getKidCount();
  if (kk != e->getKidCount()) {
    return false;
  }

  for (int i = kk; i--; ) {
    ExpressionPtr k1 = getNthExpr(i);
    ExpressionPtr k2 = e->getNthExpr(i);

    if (k1 != k2) {
      if (!k1 || !k2) {
        return false;
      }
      if (k1->getCanonID() != k2->getCanonID()) {
        return false;
      }
    }
  }

  return true;
}

ExpressionPtr Expression::fetchReplacement() {
  ExpressionPtr t = m_replacement;
  m_replacement.reset();
  return t;
}

bool Expression::isUnquotedScalar() const {
  if (!is(KindOfScalarExpression)) return false;
  return !((ScalarExpression*)this)->isQuoted();
}

ExpressionPtr Expression::MakeScalarExpression(AnalysisResultPtr ar,
                                               LocationPtr loc,
                                               CVarRef value) {
  if (value.isArray()) {
    ExpressionListPtr el(new ExpressionList(loc,
                                            KindOfExpressionList,
                                            ExpressionList::ListKindParam));

    for (ArrayIter iter(value); iter; ++iter) {
      ExpressionPtr k(MakeScalarExpression(ar, loc, iter.first()));
      ExpressionPtr v(MakeScalarExpression(ar, loc, iter.second()));
      if (!k || !v) return ExpressionPtr();
      ArrayPairExpressionPtr ap(
        new ArrayPairExpression(loc, KindOfArrayPairExpression,
                                k, v, false));
      el->addElement(ap);
    }
    if (!el->getCount()) el.reset();
    return ExpressionPtr(
      new UnaryOpExpression(loc, KindOfUnaryOpExpression,
                            el, T_ARRAY, true));
  } else if (value.isNull()) {
    return MakeConstant(ar, loc, "null");
  } else if (value.isBoolean()) {
    return MakeConstant(ar, loc, value ? "true" : "false");
  } else {
    return ScalarExpressionPtr
      (new ScalarExpression(loc,
                            Expression::KindOfScalarExpression,
                            value));
  }
}

///////////////////////////////////////////////////////////////////////////////

bool Expression::outputLineMap(CodeGenerator &cg, AnalysisResultPtr ar,
                               bool force /* = false */) {
  switch (cg.getOutput()) {
  case CodeGenerator::TrimmedPHP:
    if (cg.getStream(CodeGenerator::MapFile) &&
        cg.usingStream(CodeGenerator::PrimaryStream)) {
      cg.useStream(CodeGenerator::MapFile);
      cg_printf("%d => '%s:%d',", cg.getLineNo(CodeGenerator::PrimaryStream),
                getLocation()->file, getLocation()->line1);
      cg.useStream(CodeGenerator::PrimaryStream);
    }
    break;
  case CodeGenerator::ClusterCPP:
    {
      if (!force &&
          !(getLocalEffects() & (Construct::CanThrow|
                                 Construct::AccessorEffect|
                                 Construct::DiagnosticEffect))) {
        return false;
      }
      int line = cg.getLineNo(CodeGenerator::PrimaryStream);
      LocationPtr loc = getLocation();
      if (loc) {
        ar->recordSourceInfo(cg.getFileName(), line, loc);
        if (cg.getPHPLineNo() != loc->line1) {
          cg.setPHPLineNo(loc->line1);
          cg_printf("LINE(%d,", loc->line1);
          return true;
        }
      }
    }
    break;
  default:
    break;
  }
  return false;
}

void Expression::outputCPPCast(CodeGenerator &cg, AnalysisResultPtr ar) {
  if (m_expectedType) {
    m_expectedType->outputCPPCast(cg, ar);
  }
}

void Expression::outputCPPDecl(CodeGenerator &cg, AnalysisResultPtr ar) {
  TypePtr type = m_actualType;
  if (!type) type = Type::Variant;
  type->outputCPPDecl(cg, ar);
}

std::string Expression::genCPPTemp(CodeGenerator &cg, AnalysisResultPtr ar) {
  std::ostringstream os;
  os << Option::TempPrefix << cg.createNewLocalId(ar);
  return os.str();
}

void Expression::preOutputStash(CodeGenerator &cg, AnalysisResultPtr ar,
                                int state) {
  if (hasCPPTemp() || isScalar()) return;

  bool killCast = false;

  TypePtr srcType = m_actualType;
  TypePtr dstType = m_expectedType;
  if (m_implementedType && srcType &&
      !Type::SameType(m_implementedType, srcType)) {
    if (dstType) {
      if (!hasContext(LValue) &&
          Type::IsCastNeeded(ar, m_implementedType, srcType) &&
          !Type::IsCastNeeded(ar, m_implementedType, dstType) &&
          !Type::SameType(m_implementedType, dstType) &&
          !dstType->is(Type::KindOfAny) && !dstType->is(Type::KindOfSome)) {
        dstType.reset();
      }
    }
    srcType = m_implementedType;
    if (!dstType) dstType = m_actualType;
  }

  bool isLvalue = (m_context & LValue);
  bool isTemp = isTemporary();
  if (dstType && srcType && !isLvalue &&
      Type::IsCastNeeded(ar, srcType, dstType)) {
    isTemp = true;
  } else {
    killCast = true;
    dstType = srcType;
  }

  if (dstType) {
    switch (dstType->getKindOf()) {
    case Type::KindOfAny:
    case Type::KindOfSome:
      dstType = Type::Variant;
      break;
    default:
      break;
    }
  }

  bool constRef = dstType &&
    ((m_context & (RefValue|RefParameter)) ||
     (isTemp && !dstType->isPrimitive()) ||
     (isLvalue && dynamic_cast<FunctionCall*>(this)));

  ar->wrapExpressionBegin(cg);
  // make LINE macro separate to not interfere with persistance of expression.
  // and because nested LINE macros dont work
  if (outputLineMap(cg, ar)) cg_printf("0);\n");

  if (constRef) {
    cg_printf("const ");
  }

  if (dstType) {
    dstType->outputCPPDecl(cg, ar);
    std::string t = genCPPTemp(cg, ar);
    const char *ref = (isLvalue || constRef) ? "&" : "";
    /*
      Note that double parens are necessary:
      type_name1 tmp27(type_name2(foo));
      type_name1 tmp28((type_name2(foo)));

      tmp27 is a function returning a type_name1 taking a parameter of
      type type_name2 (not what we want!)

      tmp28 is an object of type type_name1, initialized by type_name2(foo)
    */
    cg_printf(" %s%s((", ref, t.c_str());

    /*
      In c++ a temporary bound to a reference gets the lifetime of the
      reference.
      So "const Variant &tmp27 = foo()" generates a temporary, and binds it
      to v, with the lifetime of v.
      But "const Variant &tmp27 = ref(foo())", creates a temporary, binds it
      to ref's parameter, returns a reference to that, and binds that to v;
      but the temporary gets destroyed at the end of the statement.

      So we clear the ref context here, and output the ref when we output the
      use of the temporary (top of outputCPP, below), but in that case, we also
      need to set the lval context, otherwise, rvalAt would be generated for
      array elements and object properties.
    */
    int save = m_context;
    if (m_context & RefValue) {
      m_context &= ~RefValue;
      if (is(KindOfObjectPropertyExpression) ||
          (is(KindOfArrayElementExpression) &&
           !(m_context & InvokeArgument))) {
        m_context |= LValue;
        m_context &= ~NoLValueWrapper;
      }
    }
    TypePtr et = m_expectedType;
    TypePtr at = m_actualType;
    TypePtr it = m_implementedType;
    if (killCast) {
      m_actualType = dstType;
      m_implementedType.reset();
      m_expectedType.reset();
    }
    outputCPP(cg, ar);
    if (killCast) {
      m_actualType = at;
      m_expectedType = et;
      m_implementedType = it;
    }
    m_context = save;

    cg_printf("));\n");
    if ((isLvalue || hasContext(DeepReference)) && constRef) {
      dstType->outputCPPDecl(cg, ar);
      cg_printf(" &%s_lv = const_cast<", t.c_str());
      dstType->outputCPPDecl(cg, ar);
      cg_printf("&>(%s);\n", t.c_str());
      t += "_lv";
    }

    m_cppTemp = t;
  } else {
    if (outputCPPUnneeded(cg, ar)) {
      cg_printf(";\n");
    }
    m_cppTemp = "null";
  }
}

bool Expression::preOutputCPP(CodeGenerator &cg, AnalysisResultPtr ar,
                              int state) {
  if (isScalar()) {
    return false;
  }

  bool stashAll = state & StashAll;
  state &= ~StashAll;
  bool ret = (state & FixOrder) != 0;
  int kidState = (state & ~(StashKidVars|StashVars|FixOrder));
  if (state & StashKidVars) kidState |= StashVars;
  int lastEffect = -1, i;
  int n = getKidCount();
  if (hasEffect()) {
    int j;
    ExpressionPtr lastExpr;
    for (i = j = 0; i < n; i++) {
      ExpressionPtr k = getNthExpr(i);
      if (k && !k->isScalar()) {
        if (k->hasEffect()) {
          lastEffect = i;
          lastExpr = k;
        }
        j++;
      }
    }
    if (lastEffect >= 0 && (j > 1 || lastExpr->isTemporary())) {
      kidState |= FixOrder;
      if (stashAll) {
        lastEffect = n - 1;
      }
      ret = true;
    }
  }

  if (!ret || ar->inExpression()) {
    bool skipLast = !stashAll;
    int lastState = kidState | StashByRef;
    if (stashAll) lastState |= StashVars;
    for (i = 0; i <= lastEffect; i++) {
      ExpressionPtr k = getNthExpr(i);
      if (k && !k->isScalar()) {
        int s = kidState;
        if (i == n - 1 && skipLast) s = 0;
        bool noEffect = false;
        if (m_kindOf == KindOfExpressionList) {
          cg.setItemIndex(i);
          ExpressionList *el = static_cast<ExpressionList*>(this);
          if (i >= el->getOutputCount()) {
            s = lastState = 0;
            noEffect = true;
          }
        }
        if (k->is(KindOfSimpleVariable)) {
          skipLast = false;
        }
        if (k->preOutputCPP(cg, ar, i == lastEffect ? lastState : s)) {
          ret = true;
          if (!ar->inExpression()) break;
        }
        if (noEffect) {
          k->outputCPPUnneeded(cg, ar);
          k->setCPPTemp("0");
          cg_printf(";\n");
        }
      }
    }
  }

  if (state & FixOrder) {
    if (ar->inExpression()) {
      preOutputStash(cg, ar, state);
    }
  }

  return ret;
}

bool Expression::preOutputOffsetLHS(CodeGenerator &cg,
                                    AnalysisResultPtr ar,
                                    int state) {
  if (!(m_context & (OprLValue | DeepOprLValue |
                     AssignmentLHS | DeepAssignmentLHS |
                     ExistContext | UnsetContext)) ||
      !(state & FixOrder)) {
    return Expression::preOutputCPP(cg, ar, state);
  }

  if (ExpressionPtr e0 = getNthExpr(0)) {
    e0->preOutputCPP(cg, ar, state & ~(StashVars));
  }

  if (ExpressionPtr e1 = getNthExpr(1)) {
    e1->preOutputCPP(cg, ar, state & ~(StashVars));
  }

  return true;
}

bool Expression::outputCPPBegin(CodeGenerator &cg, AnalysisResultPtr ar) {
  ar->setInExpression(true);
  return preOutputCPP(cg, ar, 0);
}

bool Expression::outputCPPEnd(CodeGenerator &cg, AnalysisResultPtr ar) {
  bool ret = ar->wrapExpressionEnd(cg);
  ar->setInExpression(false);
  return ret;
}

void Expression::outputCPPInternal(CodeGenerator &cg, AnalysisResultPtr ar) {
  if (!m_cppTemp.empty()) {
    bool ref = (m_context & RefValue) &&
      !(m_context & NoRefWrapper) &&
      isRefable();
    if (ref) cg_printf("ref(");
    cg_printf("%s", m_cppTemp.c_str());
    if (ref) cg_printf(")");
    return;
  }

  TypePtr srcType = m_actualType;
  TypePtr dstType = m_expectedType;
  if (m_implementedType && srcType &&
      !Type::SameType(m_implementedType, srcType)) {
    if (dstType) {
      if (!hasContext(LValue) &&
          Type::IsCastNeeded(ar, m_implementedType, srcType) &&
          !Type::IsCastNeeded(ar, m_implementedType, dstType) &&
          !Type::SameType(m_implementedType, dstType) &&
          !dstType->is(Type::KindOfAny) && !dstType->is(Type::KindOfSome)) {
        dstType.reset();
      }
    }
    srcType = m_implementedType;
    if (!dstType) dstType = m_actualType;
  }

  if (hasError(Expression::BadPassByRef)) {
    cg_printf("throw_fatal(\"bad pass by reference\")");
    return;
  }

  int closeParen = 0;
  if (dstType && srcType && ((m_context & LValue) == 0) &&
      Type::IsCastNeeded(ar, srcType, dstType)) {
    dstType->outputCPPCast(cg, ar);
    cg_printf("(");
    closeParen++;
    outputCPPImpl(cg, ar);
  } else {
    if (((m_context & RefValue) != 0) && ((m_context & NoRefWrapper) == 0) &&
        isRefable()) {
      cg_printf("ref(");
      closeParen++;
    }
    if (is(Expression::KindOfArrayElementExpression)) {
      if (((m_context & LValue) && !(m_context & NoLValueWrapper)) ||
          ((m_context & RefValue) && !(m_context & InvokeArgument))) {
        cg_printf("lval(");
        closeParen++;
      }
    }
    outputCPPImpl(cg, ar);
  }
  for (int i = 0; i < closeParen; i++) {
    cg_printf(")");
  }

  string comment;
  if (is(Expression::KindOfScalarExpression)) {
    ScalarExpressionPtr exp =
      dynamic_pointer_cast<ScalarExpression>(shared_from_this());
      comment = exp->getComment();
  } else if (is(Expression::KindOfConstantExpression)) {
    ConstantExpressionPtr exp =
      dynamic_pointer_cast<ConstantExpression>(shared_from_this());
      comment = exp->getComment();
  }

  if (!comment.empty()) {
    if (cg.inComments()) {
      cg_printf(" (%s)", comment.c_str());
    } else {
      cg_printf(" /* %s */", comment.c_str());
    }
  }
}

bool Expression::outputCPPUnneeded(CodeGenerator &cg, AnalysisResultPtr ar) {
  if (hasEffect() && m_cppTemp.empty()) {
    setUnused(true);
    outputCPP(cg, ar);
    return true;
  }
  return false;
}

void Expression::outputCPP(CodeGenerator &cg, AnalysisResultPtr ar) {
  bool inExpression = ar->inExpression();
  bool wrapped = false;
  TypePtr et = m_expectedType;
  TypePtr at = m_actualType;
  TypePtr it = m_implementedType;
  if (isUnused()) {
    if (et) {
      m_expectedType.reset();
    }
    if (it) {
      m_implementedType.reset();
      m_actualType = it;
    }
  }

  if (!inExpression) {
    ar->setInExpression(true);
    wrapped = preOutputCPP(cg, ar, 0);
  }

  bool linemap = outputLineMap(cg, ar);
  outputCPPInternal(cg, ar);
  if (linemap) cg_printf(")");

  m_implementedType = it;
  m_actualType = at;
  m_expectedType = et;

  if (!inExpression) {
    if (wrapped) cg_printf(";");
    ar->wrapExpressionEnd(cg);
    ar->setInExpression(inExpression);
  }
}

void Expression::outputCPPExistTest(CodeGenerator &cg, AnalysisResultPtr ar,
                                    int op) {
  switch (op) {
  case T_ISSET:  cg_printf("isset("); break;
  case T_EMPTY:  cg_printf("empty("); break;
  default: ASSERT(false);
  }
  outputCPP(cg, ar);
  cg_printf(")");
}
void Expression::outputCPPUnset(CodeGenerator &cg, AnalysisResultPtr ar) {
  cg_printf("unset(");
  outputCPP(cg, ar);
  cg_printf(")");
}
