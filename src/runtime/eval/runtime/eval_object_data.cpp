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

#include <runtime/eval/runtime/eval_object_data.h>
#include <runtime/eval/ast/method_statement.h>
#include <runtime/eval/ast/class_statement.h>
#include <runtime/base/hphp_system.h>
#include <runtime/eval/runtime/eval_state.h>

namespace HPHP {
namespace Eval {

/////////////////////////////////////////////////////////////////////////////
// constructor/destructor

IMPLEMENT_OBJECT_ALLOCATION_CLS(HPHP::Eval,EvalObjectData)

EvalObjectData::EvalObjectData(ClassEvalState &cls, const char* pname,
                               ObjectData* r /* = NULL */)
: DynamicObjectData(pname, r ? r : this), m_cls(cls) {
  if (pname) setRoot(root); // For ext classes
  if (r == NULL) {
    RequestEvalState::registerObject(this);
  }
  if (getMethodStatement("__get")) setAttribute(UseGet);
  if (getMethodStatement("__set")) setAttribute(UseSet);
}

// Only used for cloning and so should not register object
EvalObjectData::EvalObjectData(ClassEvalState &cls) :
  DynamicObjectData(NULL, this), m_cls(cls) {
}

ObjectData *EvalObjectData::dynCreate(CArrRef params, bool ini /* = true */) {
  init();
  if (ini) {
    dynConstruct(params);
  }
  return this;
}

void EvalObjectData::init() {
  m_cls.getClass()->initializeObject(this);
  DynamicObjectData::init();
}

void EvalObjectData::dynConstruct(CArrRef params) {
  const MethodStatement *ms = m_cls.getConstructor();
  if (ms) {
    ms->invokeInstance(Object(root), params);
  } else {
    DynamicObjectData::dynConstruct(params);
  }
}

void EvalObjectData::dynConstructFromEval(VariableEnvironment &env,
                                          const FunctionCallExpression *call) {
  const MethodStatement *ms = m_cls.getConstructor();
  if (ms) {
    ms->invokeInstanceDirect(Object(root), env, call);
  } else {
    DynamicObjectData::dynConstructFromEval(env, call);
  }
}

void EvalObjectData::destruct() {
  const MethodStatement *ms;
  incRefCount();
  if (!inCtorDtor() && (ms = getMethodStatement("__destruct"))) {
    setInDtor();
    try {
      ms->invokeInstance(Object(root), Array(), false);
    } catch (...) {
      handle_destructor_exception();
    }
  }
  DynamicObjectData::destruct();
  if (root == this) {
    RequestEvalState::deregisterObject(this);
  }
}

Array EvalObjectData::o_toArray() const {
  Array values(DynamicObjectData::o_toArray());
  Array props(Array::Create());
  m_cls.getClass()->toArray(props, values);
  if (!values.empty()) {
    props += values;
  }
  return props;
}

Variant *EvalObjectData::o_realProp(CStrRef s, int flags,
                                    CStrRef context /* = null_string */) const {
  CStrRef c = context.isNull() ? FrameInjection::GetClassName(false) : context;
  if (Variant *priv =
      const_cast<Array&>(m_privates).lvalPtr(c, flags & RealPropWrite, false)) {
    if (Variant *ret = priv->lvalPtr(s, flags & RealPropWrite, false)) {
      return ret;
    }
  }
  int mods;
  if (!(flags & RealPropUnchecked) &&
      !m_cls.getClass()->attemptPropertyAccess(s, c, mods)) {
    return NULL;
  }
  return DynamicObjectData::o_realProp(s, flags);
}

Variant EvalObjectData::o_getError(CStrRef prop, CStrRef context) {
  CStrRef c = context.isNull() ? FrameInjection::GetClassName(false) : context;
  int mods;
  if (!m_cls.getClass()->attemptPropertyAccess(prop, c, mods)) {
    m_cls.getClass()->failPropertyAccess(prop, c, mods);
  } else {
    DynamicObjectData::o_getError(prop, context);
  }
  return null;
}

Variant EvalObjectData::o_setError(CStrRef prop, CStrRef context) {
  CStrRef c = context.isNull() ? FrameInjection::GetClassName(false) : context;
  int mods;
  if (!m_cls.getClass()->attemptPropertyAccess(prop, c, mods)) {
    m_cls.getClass()->failPropertyAccess(prop, c, mods);
  }
  return null;
}

void EvalObjectData::o_getArray(Array &props) const {
  String zero("\0", 1, AttachLiteral);
  for (ArrayIter it(m_privates); !it.end(); it.next()) {
    String prefix(zero);
    prefix += it.first();
    prefix += zero;
    for (ArrayIter it2(it.second()); !it2.end(); it2.next()) {
      CVarRef v = it2.secondRef();
      if (v.isInitialized()) {
        props.set(prefix + it2.first(), v.isReferenced() ? ref(v) : v);
      }
    }
  }
  DynamicObjectData::o_getArray(props);
}

void EvalObjectData::o_setArray(CArrRef props) {
  for (ArrayIter it(props); !it.end(); it.next()) {
    String k = it.first();
    if (!k.empty() && k.charAt(0) == '\0') {
      int subLen = k.find('\0', 1) + 1;
      String cls = k.substr(1, subLen - 2);
      String key = k.substr(subLen);
      props->load(k, o_lval(key, Variant(), cls));
    }
  }
  DynamicObjectData::o_setArray(props);
}

void EvalObjectData::o_setPrivate(const char *cls, const char *s, int64 hash,
                                  CVarRef v) {
  m_privates.lvalAt(cls).set(s, v, hash);
}

CStrRef EvalObjectData::o_getClassName() const {
  if (m_class_name.isNull()) {
    // an object can never live longer than its class
    const std::string &clsName = m_cls.getClass()->name();
    m_class_name.assign(clsName.c_str(), clsName.size(), AttachLiteral);
  }
  return m_class_name;
}

const MethodStatement
*EvalObjectData::getMethodStatement(const char* name) const {
  const hphp_const_char_imap<const MethodStatement*> &meths =
    m_cls.getMethodTable();
  hphp_const_char_imap<const MethodStatement*>::const_iterator it =
    meths.find(name);
  if (it != meths.end()) {
    return it->second;
  }
  return NULL;
}

bool EvalObjectData::o_instanceof(const char *s) const {
  return m_cls.getClass()->subclassOf(s) ||
    (!parent.isNull() && parent->o_instanceof(s));
}

Variant EvalObjectData::o_invoke(MethodIndex methodIndex,
                                 const char *s,
                                 CArrRef params, int64 hash,
                                 bool fatal /* = true */) {
  // should only come here through EvalObjectData::o_invoke_mil,
  // which already handled meths.find(s), 's' is going away here
  // so avoid using it
  return DynamicObjectData::o_invoke(methodIndex, s, params, hash, fatal);
}

Variant EvalObjectData::o_invoke_mil( const char *s,
                                      CArrRef params, int64 hash,
                                      bool fatal /* = true */) {
  const hphp_const_char_imap<const MethodStatement*> &meths =
    m_cls.getMethodTable();
  hphp_const_char_imap<const MethodStatement*>::const_iterator it =
    meths.find(s);
  if (it != meths.end()) {
    if (it->second) {
      return it->second->invokeInstance(Object(root), params);
    } else {
      return DynamicObjectData::o_invoke_mil(s, params, hash, fatal);
    }
  } else {
    return doCall(s, params, fatal);
  }
}


Variant EvalObjectData::o_invoke_ex(const char *clsname,
                                    MethodIndex methodIndex,
                                    const char *s,
                                       CArrRef params, int64 hash,
                                       bool fatal /* = false */) {
  // we should come here only thru o_invoke_ex_mil
  // and it already handled this case
  ASSERT( !(m_cls.getClass()->subclassOf(clsname)) );
  return DynamicObjectData::o_invoke_ex(clsname, methodIndex, s, params, hash,
                                        fatal);
}

Variant EvalObjectData::o_invoke_ex_mil(const char *clsname,
                                        const char *s,
                                        CArrRef params, int64 hash,
                                        bool fatal /* = false */) {
  if (m_cls.getClass()->subclassOf(clsname)) {
    bool foundClass;
    const MethodStatement *ms = RequestEvalState::findMethod(clsname, s,
                                                             foundClass);
    if (ms) {
      return ms->invokeInstance(Object(root), params);
    } else {
      // Possibly builtin class has this method
      const hphp_const_char_imap<const MethodStatement*> &meths =
        m_cls.getMethodTable();
      if (meths.find(s) == meths.end()) {
        // Absolutely nothing in the hierarchy has this method
        return doCall(s, params, fatal);
      }
    }
    return DynamicObjectData::o_invoke_mil(s, params, hash, fatal);
  }
  return DynamicObjectData::o_invoke_ex_mil(clsname, s, params,
                                        hash, fatal);
}

Variant EvalObjectData::o_invoke_few_args(MethodIndex methodIndex,
                                          const char *s,
                                          int64 hash, int count,
                                          INVOKE_FEW_ARGS_IMPL_ARGS) {
  // o_invoke_few_args needs to pass 's' (the method name)
  // to o_invoke for eval processing.  Thus, there is not a usable
  // methodIndex version of EvalObjectData::o_invoke_few_args.
  ASSERT(0);
  return "";
}

Variant EvalObjectData::o_invoke_few_args_mil(const char *s,
                                              int64 hash, int count,
                                              INVOKE_FEW_ARGS_IMPL_ARGS) {
  switch (count) {
  case 0: {
    return o_invoke_mil(s, Array(), hash);
  }
  case 1: {
    Array params(ArrayInit(1, true).set(a0).create());
    return o_invoke_mil(s, params, hash);
  }
  case 2: {
    Array params(ArrayInit(2, true).set(a0).set(a1).create());
    return o_invoke_mil(s, params, hash);
  }
  case 3: {
    Array params(ArrayInit(3, true).set(a0).set(a1).set(a2).create());
    return o_invoke_mil(s, params, hash);
  }
#if INVOKE_FEW_ARGS_COUNT > 3
  case 4: {
    Array params(ArrayInit(4, true).set(a0).set(a1).set(a2).set(a3).create());
    return o_invoke_mil(s, params, hash);
  }
  case 5: {
    Array params(ArrayInit(5, true).set(a0).set(a1).set(a2).
                                    set(a3).set(a4).create());
    return o_invoke_mil(s, params, hash);
  }
  case 6: {
    Array params(ArrayInit(6, true).set(a0).set(a1).set(a2).
                                    set(a3).set(a4).set(a5).create());
    return o_invoke_mil(s, params, hash);
  }
#endif
#if INVOKE_FEW_ARGS_COUNT > 6
  case 7: {
    Array params(ArrayInit(7, true).set(a0).set(a1).set(a2).
                                    set(a3).set(a4).set(a5).
                                    set(a6).create());
    return o_invoke_mil(s, params, hash);
  }
  case 8: {
    Array params(ArrayInit(8, true).set(a0).set(a1).set(a2).
                                    set(a3).set(a4).set(a5).
                                    set(a6).set(a7).create());
    return o_invoke_mil(s, params, hash);
  }
  case 9: {
    Array params(ArrayInit(9, true).set(a0).set(a1).set(a2).
                                    set(a3).set(a4).set(a5).
                                    set(a6).set(a7).set(a8).create());
    return o_invoke_mil(s, params, hash);
  }
  case 10: {
    Array params(ArrayInit(10, true).set(a0).set(a1).set(a2).
                                     set(a3).set(a4).set(a5).
                                     set(a6).set(a7).set(a8).
                                     set(a9).create());
    return o_invoke_mil(s, params, hash);
  }
#endif
  default:
    ASSERT(false);
  }
  return null;
}

Variant EvalObjectData::doCall(Variant v_name, Variant v_arguments,
                               bool fatal) {
  const MethodStatement *ms = getMethodStatement("__call");
  if (ms) {
    if (v_arguments.isNull()) {
      v_arguments = Array::Create();
    }
    return ms->invokeInstance(Object(root),
                              CREATE_VECTOR2(v_name, v_arguments), false);
  } else {
    return DynamicObjectData::doCall(v_name, v_arguments, fatal);
  }
}

Variant EvalObjectData::t___destruct() {
  const MethodStatement *ms = getMethodStatement("__destruct");
  if (ms) {
    return ms->invokeInstance(Object(root), Array(), false);
  } else {
    return DynamicObjectData::t___destruct();
  }
}
Variant EvalObjectData::t___set(Variant v_name, Variant v_value) {
  if (v_value.isReferenced()) {
    v_value.setContagious();
  }
  const MethodStatement *ms = getMethodStatement("__set");
  if (ms) {
    return ms->invokeInstance(Object(root), CREATE_VECTOR2(v_name, v_value),
        false);
  } else {
    return DynamicObjectData::t___set(v_name, v_value);
  }
}
Variant EvalObjectData::t___get(Variant v_name) {
  const MethodStatement *ms = getMethodStatement("__get");
  if (ms) {
    return ms->invokeInstance(Object(root), CREATE_VECTOR1(v_name), false);
  } else {
    return DynamicObjectData::t___get(v_name);
  }
}
bool EvalObjectData::t___isset(Variant v_name) {
  const MethodStatement *ms = getMethodStatement("__isset");
  if (ms) {
    return ms->invokeInstance(Object(root), CREATE_VECTOR1(v_name), false);
  } else {
    return DynamicObjectData::t___isset(v_name);
  }
}
Variant EvalObjectData::t___unset(Variant v_name) {
  const MethodStatement *ms = getMethodStatement("__unset");
  if (ms) {
    return ms->invokeInstance(Object(root), CREATE_VECTOR1(v_name), false);
  } else {
    return DynamicObjectData::t___unset(v_name);
  }
}

bool EvalObjectData::php_sleep(Variant &ret) {
  ret = t___sleep();
  return getMethodStatement("__sleep");
}

Variant EvalObjectData::t___sleep() {
  const MethodStatement *ms = getMethodStatement("__sleep");
  if (ms) {
    return ms->invokeInstance(Object(root), Array(), false);
  } else {
    return DynamicObjectData::t___sleep();
  }
}
Variant EvalObjectData::t___wakeup() {
  const MethodStatement *ms = getMethodStatement("__wakeup");
  if (ms) {
    return ms->invokeInstance(Object(root), Array(), false);
  } else {
    return DynamicObjectData::t___wakeup();
  }
}
Variant EvalObjectData::t___set_state(Variant v_properties) {
  const MethodStatement *ms = getMethodStatement("__set_state");
  if (ms) {
    return ms->invokeInstance(Object(root), CREATE_VECTOR1(v_properties),
        false);
  } else {
    return DynamicObjectData::t___set_state(v_properties);
  }
}
String EvalObjectData::t___tostring() {
  const MethodStatement *ms = getMethodStatement("__tostring");
  if (ms) {
    return ms->invokeInstance(Object(root), Array(), false);
  } else {
    return DynamicObjectData::t___tostring();
  }
}
Variant EvalObjectData::t___clone() {
  const MethodStatement *ms = getMethodStatement("__clone");
  if (ms) {
    return ms->invokeInstance(Object(root), Array(), false);
  } else {
    return DynamicObjectData::t___clone();
  }
}

ObjectData* EvalObjectData::cloneImpl() {
  EvalObjectData *e = NEW(EvalObjectData)(m_cls);
  if (!parent.isNull()) {
    e->setParent(parent->clone());
  } else {
    cloneSet(e);
  }
  e->m_privates = m_privates;
  // Registration is done here because the clone constructor is not
  // passed root.
  if (root == this) {
    RequestEvalState::registerObject(e);
  }
  return e;
}

Variant &EvalObjectData::___offsetget_lval(Variant v_name) {
  const MethodStatement *ms = getMethodStatement("offsetget");
  if (ms) {
    Variant &v = get_globals()->__lvalProxy;
    v = ms->invokeInstance(Object(root), CREATE_VECTOR1(v_name), false);
    return v;
  } else {
    return DynamicObjectData::___offsetget_lval(v_name);
  }
}


///////////////////////////////////////////////////////////////////////////////
}
}
