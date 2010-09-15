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

#include <runtime/eval/ast/object_property_expression.h>
#include <runtime/eval/ast/name.h>
#include <runtime/eval/runtime/variable_environment.h>
#include <runtime/eval/parser/hphp.tab.hpp>

namespace HPHP {
namespace Eval {
///////////////////////////////////////////////////////////////////////////////

ObjectPropertyExpression::ObjectPropertyExpression(EXPRESSION_ARGS,
                                                   ExpressionPtr obj,
                                                   NamePtr name)
  : LvalExpression(EXPRESSION_PASS), m_obj(obj), m_name(name) {
}

Variant ObjectPropertyExpression::eval(VariableEnvironment &env) const {
  Variant obj(m_obj->eval(env));
  String name(m_name->get(env));
  SET_LINE;
  return obj.o_get(name);
}

Variant ObjectPropertyExpression::evalExist(VariableEnvironment &env) const {
  Variant obj(m_obj->evalExist(env));
  String name(m_name->get(env));
  SET_LINE;
  return obj.o_get(name, false);
}

Variant &ObjectPropertyExpression::lval(VariableEnvironment &env) const {
  // How annoying. Sometimes object is an lval and should be treated as such
  // and sometimes it isn't eg method call.
  const LvalExpression *lobj = m_obj->toLval();
  if (lobj) {
    Variant &lv = lobj->lval(env);
    String name(m_name->get(env));
    SET_LINE;
    return lv.o_lval(name, get_globals()->__lvalProxy);
  } else {
    Variant obj(m_obj->eval(env));
    String name(m_name->get(env));
    SET_LINE;
    return obj.o_lval(name, get_globals()->__lvalProxy);
  }
}

bool ObjectPropertyExpression::weakLval(VariableEnvironment &env,
                                        Variant* &v) const {
  Variant *obj;

  const LvalExpression *lobj = m_obj->toLval();
  if (lobj) {
    bool ok = lobj->weakLval(env, obj);
    String name(m_name->get(env));
    if (!ok || !SET_LINE_EXPR) return false;
    Variant tmp;
    v = &obj->o_unsetLval(name, tmp);
    return v != &tmp;
  }

  m_obj->eval(env);
  m_name->get(env);
  return false;
}

Variant ObjectPropertyExpression::set(VariableEnvironment &env, CVarRef val)
  const {
  const LvalExpression *lobj = m_obj->toLval();
  if (lobj) {
    Variant &lv = lobj->lval(env);
    String name(m_name->get(env));
    SET_LINE;
    lv.o_set(name, val);
  } else {
    Variant obj(m_obj->eval(env));
    String name(m_name->get(env));
    SET_LINE;
    obj.o_set(name, val);
  }
  return val;
}

Variant ObjectPropertyExpression::setOp(VariableEnvironment &env, int op,
                                        CVarRef rhs) const {
  Variant *vobj;
  Variant obj;
  const LvalExpression *lobj = m_obj->toLval();
  if (lobj) {
    vobj = &lobj->lval(env);
  } else {
    obj = m_obj->eval(env);
    vobj = &obj;
  }
  String name(m_name->get(env));
  SET_LINE;
  switch (op) {
  case T_PLUS_EQUAL:
    return vobj->o_assign_op<Variant, T_PLUS_EQUAL>(name, rhs);
  case T_MINUS_EQUAL:
    return vobj->o_assign_op<Variant, T_MINUS_EQUAL>(name, rhs);
  case T_MUL_EQUAL:
    return vobj->o_assign_op<Variant, T_MUL_EQUAL>(name, rhs);
  case T_DIV_EQUAL:
    return vobj->o_assign_op<Variant, T_DIV_EQUAL>(name, rhs);
  case T_CONCAT_EQUAL:
    return vobj->o_assign_op<Variant, T_CONCAT_EQUAL>(name, rhs);
  case T_MOD_EQUAL:
    return vobj->o_assign_op<Variant, T_MOD_EQUAL>(name, rhs);
  case T_AND_EQUAL:
    return vobj->o_assign_op<Variant, T_AND_EQUAL>(name, rhs);
  case T_OR_EQUAL:
    return vobj->o_assign_op<Variant, T_OR_EQUAL>(name, rhs);
  case T_XOR_EQUAL:
    return vobj->o_assign_op<Variant, T_XOR_EQUAL>(name, rhs);
  case T_SL_EQUAL:
    return vobj->o_assign_op<Variant, T_SL_EQUAL>(name, rhs);
  case T_SR_EQUAL:
    return vobj->o_assign_op<Variant, T_SR_EQUAL>(name, rhs);
  case T_INC:
    return vobj->o_assign_op<Variant, T_INC>(name, rhs);
  case T_DEC:
    return vobj->o_assign_op<Variant, T_DEC>(name, rhs);
  default:
    ASSERT(false);
  }
  return rhs;
}

bool ObjectPropertyExpression::exist(VariableEnvironment &env, int op) const {
  Object obj(toObject(m_obj->evalExist(env)));
  String name(m_name->get(env));
  SET_LINE;
  if (op == T_ISSET) {
    return obj->o_isset(name);
  } else {
    return obj->o_empty(name);
  }
}

void ObjectPropertyExpression::unset(VariableEnvironment &env) const {
  const LvalExpression *lobj = m_obj->toLval();
  Variant *obj;
  if (lobj && lobj->weakLval(env, obj)) {
    String name(m_name->get(env));
    toObject(*obj)->o_unset(name);
  } else {
    Object obj(toObject(m_obj->evalExist(env)));
    String name(m_name->get(env));
    obj->o_unset(name);
  }
}

NamePtr ObjectPropertyExpression::getProperty() const { return m_name; }

void ObjectPropertyExpression::dump() const {
  m_obj->dump();
  printf("->");
  m_name->dump();
}

///////////////////////////////////////////////////////////////////////////////
}
}

