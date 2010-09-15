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

#ifndef __HPHP_MACROS_H__
#define __HPHP_MACROS_H__

namespace HPHP {

///////////////////////////////////////////////////////////////////////////////
// class macros

#define LITSTR_INIT(str)    (true ? (str) : ("" str "")), (sizeof(str)-1)
#define LITSTR(index, str)  (literalStrings[index])
#define NAMSTR(nam, str)    (nam)

#define GET_THIS()         fi.getThis()
#define GET_THIS_ARROW()   fi.getThisForArrow()->
#define GET_THIS_DOT()     fi.getThisForArrow().

#define FORWARD_DECLARE_CLASS(cls)                      \
  class c_##cls;                                        \
  typedef SmartObject<c_##cls> p_##cls;                 \

#define FORWARD_DECLARE_INTERFACE(cls)                  \
  class c_##cls;                                        \
  typedef SmartInterface<c_##cls> p_##cls               \

#define FORWARD_DECLARE_GENERIC_INTERFACE(cls)          \
  class c_##cls;                                        \
  typedef Object               p_##cls                  \

#define FORWARD_DECLARE_REDECLARED_CLASS(cls)           \
  class cs_##cls                                        \

#define BEGIN_CLASS_MAP(cls)                            \
  public:                                               \
  virtual bool o_instanceof(const char *s) const {      \
    if (!s || !*s) return false;                        \
    if (strcasecmp(s, #cls) == 0) return true;          \

#define PARENT_CLASS(parent)                            \
    if (strcasecmp(s, #parent) == 0) return true;       \

#define CLASS_MAP_REDECLARED()                          \
    if (parent->o_instanceof(s)) return true;           \

#define RECURSIVE_PARENT_CLASS(parent)                  \
    if (strcasecmp(s, #parent) == 0) return true;       \
    if (c_##parent::o_instanceof(s)) return true;       \

#define END_CLASS_MAP(cls)                              \
    return false;                                       \
  }                                                     \

#define INVOKE_FEW_ARGS_COUNT 6

#define INVOKE_FEW_ARGS_DECL3                                           \
                            CVarRef a0 = null_variant,                  \
                            CVarRef a1 = null_variant,                  \
                            CVarRef a2 = null_variant
#define INVOKE_FEW_ARGS_DECL6                                           \
                            INVOKE_FEW_ARGS_DECL3,                      \
                            CVarRef a3 = null_variant,                  \
                            CVarRef a4 = null_variant,                  \
                            CVarRef a5 = null_variant
#define INVOKE_FEW_ARGS_DECL10                                          \
                            INVOKE_FEW_ARGS_DECL6,                      \
                            CVarRef a6 = null_variant,                  \
                            CVarRef a7 = null_variant,                  \
                            CVarRef a8 = null_variant,                  \
                            CVarRef a9 = null_variant
#define INVOKE_FEW_ARGS_IMPL3                                           \
                            CVarRef a0, CVarRef a1, CVarRef a2
#define INVOKE_FEW_ARGS_IMPL6                                           \
                            INVOKE_FEW_ARGS_IMPL3, CVarRef a3, CVarRef a4, \
                            CVarRef a5
#define INVOKE_FEW_ARGS_IMPL10                                          \
                            INVOKE_FEW_ARGS_IMPL6, CVarRef a6, CVarRef a7, \
                            CVarRef a8, CVarRef a9

#define INVOKE_FEW_ARGS_PASS3 a0, a1, a2
#define INVOKE_FEW_ARGS_PASS6 INVOKE_FEW_ARGS_PASS3, a3, a4, a5
#define INVOKE_FEW_ARGS_PASS10 INVOKE_FEW_PARGS_PASS6, a6, a7, a8, a9

#if INVOKE_FEW_ARGS_COUNT == 3
#define INVOKE_FEW_ARGS_DECL_ARGS INVOKE_FEW_ARGS_DECL3
#define INVOKE_FEW_ARGS_PASS_ARGS INVOKE_FEW_ARGS_PASS3
#define INVOKE_FEW_ARGS_IMPL_ARGS INVOKE_FEW_ARGS_IMPL3
#elif INVOKE_FEW_ARGS_COUNT == 6
#define INVOKE_FEW_ARGS_DECL_ARGS INVOKE_FEW_ARGS_DECL6
#define INVOKE_FEW_ARGS_PASS_ARGS INVOKE_FEW_ARGS_PASS6
#define INVOKE_FEW_ARGS_IMPL_ARGS INVOKE_FEW_ARGS_IMPL6
#elif INVOKE_FEW_ARGS_COUNT == 10
#define INVOKE_FEW_ARGS_DECL_ARGS INVOKE_FEW_ARGS_DECL10
#define INVOKE_FEW_ARGS_PASS_ARGS INVOKE_FEW_ARGS_PASS10
#define INVOKE_FEW_ARGS_IMPL_ARGS INVOKE_FEW_ARGS_IMPL10
#else
#error Bad INVOKE_FEW_ARGS_COUNT
#endif

#define DECLARE_STATIC_PROP_OPS                                         \
  public:                                                               \
  static void os_static_initializer();                                  \
  static Variant os_getInit(CStrRef s);                                 \
  static Variant os_get(CStrRef s);                                     \
  static Variant &os_lval(CStrRef s);                                   \
  static Variant os_constant(const char *s);                            \

#define DECLARE_INSTANCE_PROP_OPS                                       \
  public:                                                               \
  virtual Variant *o_realProp(CStrRef prop, int flags,                  \
                        CStrRef context = null_string) const;           \
  Variant *o_realPropPrivate(CStrRef s, int flags) const;               \
  virtual void o_getArray(Array &props) const;                          \
  virtual void o_setArray(CArrRef props);                               \

#define DECLARE_INSTANCE_PUBLIC_PROP_OPS                                \
  public:                                                               \
  virtual Variant *o_realPropPublic(CStrRef s, int flags) const;        \

#define DECLARE_COMMON_INVOKES                                          \
  static Variant os_invoke(const char *c, MethodIndex methodIndex,      \
                           const char *s,                               \
                           CArrRef ps, int64 h, bool f = true);         \
  virtual Variant o_invoke(MethodIndex methodIndex, const char *s,      \
                           CArrRef ps, int64 h,                         \
                           bool f = true);                              \
  virtual Variant o_invoke_few_args(MethodIndex methodIndex,            \
                                    const char *s, int64 h, int count,  \
                                    INVOKE_FEW_ARGS_DECL_ARGS);         \

#define DECLARE_INVOKE_EX(cls, originalName, parent)                    \
  virtual Variant o_invoke_ex(const char *clsname,                      \
                              MethodIndex methodIndex, const char *s,   \
                              CArrRef ps, int64 h, bool f = true) {     \
    if (clsname && strcasecmp(clsname, #originalName) == 0) {           \
      return c_##cls::o_invoke(methodIndex, s, ps, h, f);               \
    }                                                                   \
    return c_##parent::o_invoke_ex(clsname, methodIndex, s, ps, h, f);  \
  }                                                                     \

#define DECLARE_CLASS_COMMON(cls, originalName)                         \
  DECLARE_OBJECT_ALLOCATION(c_##cls)                                    \
  protected:                                                            \
  ObjectData *cloneImpl();                                              \
  void cloneSet(c_##cls *cl);                                           \
  public:                                                               \
  static const char *GetClassName() { return #originalName; }           \
  static StaticString s_class_name;                                     \
  virtual CStrRef o_getClassName() const { return s_class_name; }       \

#define DECLARE_CLASS(cls, originalName, parent)                        \
  DECLARE_CLASS_COMMON(cls, originalName)                               \
  DECLARE_STATIC_PROP_OPS                                               \
  DECLARE_INSTANCE_PROP_OPS                                             \
  DECLARE_INSTANCE_PUBLIC_PROP_OPS                                      \
  DECLARE_COMMON_INVOKES                                                \
  DECLARE_INVOKE_EX(cls, originalName, parent)                          \
  public:                                                               \

#define DECLARE_DYNAMIC_CLASS(cls, originalName, parent)                \
  DECLARE_CLASS_COMMON(cls, originalName)                               \
  DECLARE_STATIC_PROP_OPS                                               \
  DECLARE_INSTANCE_PROP_OPS                                             \
  DECLARE_COMMON_INVOKES                                                \
  DECLARE_INVOKE_EX(cls, originalName, parent)                          \
  public:                                                               \


#define DECLARE_INVOKES_FROM_EVAL                                       \
  static Variant os_invoke_from_eval(const char *c, const char *s,      \
                                     Eval::VariableEnvironment &env,    \
                                     const Eval::FunctionCallExpression *call,\
                                     int64 hash,                        \
                                     bool fatal /* = true */);          \
  Variant o_invoke_from_eval(const char *s,                             \
                             Eval::VariableEnvironment &env,            \
                             const Eval::FunctionCallExpression *call,  \
                             int64 hash,                                \
                             bool fatal /* = true */);

#define DECLARE_ROOT                                                    \
  Variant o_root_invoke(MethodIndex methodIndex, const char *s,         \
                        CArrRef ps, int64 h, bool f = true) {           \
    return root->o_invoke(methodIndex, s, ps, h, f);                    \
  }                                                                     \
  Variant o_root_invoke_few_args(MethodIndex methodIndex, const char *s,\
                                 int64 h, int count,                    \
                                 INVOKE_FEW_ARGS_DECL_ARGS) {           \
    return root->o_invoke_few_args(methodIndex, s, h, count,            \
                                   INVOKE_FEW_ARGS_PASS_ARGS);          \
  }

#define CLASS_CHECK(exp) (checkClassExists(s, g), (exp))

#define IMPLEMENT_CLASS(cls)                                            \
  StaticString c_##cls::s_class_name(c_##cls::GetClassName());          \
  IMPLEMENT_OBJECT_ALLOCATION(c_##cls)

//////////////////////////////////////////////////////////////////////////////
// jump table entries

#define HASH_GUARD(code, f)                                             \
  if (hash == code && !strcasecmp(s, #f))
#define HASH_GUARD_LITSTR(code, str)                                    \
  if (hash == code && (str.data() == s || !strcasecmp(s, str.data())))
#define HASH_GUARD_STRING(code, f)                                      \
  if (hash == code && !strcasecmp(s.data(), #f))
#define HASH_EXISTS_STRING(code, str, len)                              \
  if (hash == code && s.length() == len &&                              \
      memcmp(s.data(), str, len) == 0) return true
#define HASH_REALPROP_STRING(code, str, len, prop)                      \
  if (hash == code && s.length() == len &&                              \
      memcmp(s.data(), str, len) == 0)                                  \
    return const_cast<Variant*>(&m_##prop)
#define HASH_REALPROP_TYPED_STRING(code, str, len, prop)                \
  if (!(flags&(RealPropCreate|RealPropWrite)) &&                        \
      hash == code && s.length() == len &&                              \
      memcmp(s.data(), str, len) == 0)                                  \
    return g->__realPropProxy = m_##prop,&g->__realPropProxy
#define HASH_INITIALIZED(code, name, str)                               \
  if (hash == code && strcmp(s, str) == 0)                              \
    return isInitialized(name)
#define HASH_INITIALIZED_STRING(code, name, str, len)                   \
  if (hash == code && s.length() == len &&                              \
      memcmp(s.data(), str, len) == 0) return isInitialized(name)
#define HASH_INITIALIZED_LITSTR(code, index, name, len)                 \
do { \
  const char *s1 = s.data();                                            \
  const char *s2 = literalStrings[index].data();                        \
  if ((s1 == s2) ||                                                     \
      (hash == code && s.length() == len &&                             \
      memcmp(s1, s2, len) == 0)) return isInitialized(name);            \
} while (0)
#define HASH_INITIALIZED_NAMSTR(code, str, name, len)                   \
do { \
  const char *s1 = s.data();                                            \
  const char *s2 = str.data();                                          \
  if ((s1 == s2) ||                                                     \
      (hash == code && s.length() == len &&                             \
      memcmp(s1, s2, len) == 0)) return isInitialized(name);            \
} while (0)
#define HASH_RETURN(code, name, str)                                    \
  if (hash == code && strcmp(s, str) == 0) return name
#define HASH_RETURN_STRING(code, name, str, len)                        \
  if (hash == code && s.length() == len &&                              \
      memcmp(s.data(), str, len) == 0) return name
#define HASH_RETURN_LITSTR(code, index, name, len)                      \
do { \
  const char *s1 = s.data();                                            \
  const char *s2 = literalStrings[index].data();                        \
  if ((s1 == s2) ||                                                     \
      (hash == code && s.length() == len &&                             \
      memcmp(s1, s2, len) == 0)) return name;                           \
} while (0)
#define HASH_RETURN_NAMSTR(code, str, name, len)                        \
do { \
  const char *s1 = s.data();                                            \
  const char *s2 = str.data();                                          \
  if ((s1 == s2) ||                                                     \
      (hash == code && s.length() == len &&                             \
      memcmp(s1, s2, len) == 0)) return name;                           \
} while (0)

#define HASH_SET_STRING(code, name, str, len)                           \
  if (hash == code && s.length() == len &&                              \
      memcmp(s.data(), str, len) == 0) { name = v; return null; }
#define HASH_INDEX(code, str, index)                                    \
  if (hash == code && strcmp(s, #str) == 0) { return index;}

#define HASH_INVOKE(code, f)                                            \
  if (hash == code && !strcasecmp(s, #f)) return i_ ## f(params)
#define HASH_INVOKE_REDECLARED(code, f)                                 \
  if (hash == code && !strcasecmp(s, #f)) return g->i_ ## f(params)
#define HASH_INVOKE_METHOD(code, f)                                     \
  if (hash == code && !strcasecmp(s, #f)) return o_i_ ## f(params)
#define HASH_INVOKE_CONSTRUCTOR(code, f, id)                            \
  if (hash == code && !strcasecmp(s, #f)) return o_i_ ## id(params)
#define HASH_INVOKE_STATIC_METHOD(code, f, methodIndex)                 \
  if (hash == code && !strcasecmp(s, #f))                               \
     return cw_ ## f.os_invoke(#f, methodIndex, method, params, -1, fatal)
#define HASH_INVOKE_STATIC_METHOD_VOLATILE(code, f, methodIndex)        \
  if (hash == code && !strcasecmp(s, #f))                               \
    return CLASS_CHECK(cw_ ## f.os_invoke(#f, methodIndex,              \
                       method, params, -1, fatal))
#define HASH_INVOKE_STATIC_METHOD_REDECLARED(code, f, methodIndex)      \
  if (hash == code && !strcasecmp(s, #f))                               \
    return CLASS_CHECK(g->cso_ ## f->os_invoke(#f, methodIndex,         \
                       method, params, -1, fatal))
#define HASH_GET_OBJECT_STATIC_CALLBACKS(code, f)                       \
  if (hash == code && !strcasecmp(s, #f)) return &cw_ ## f
#define HASH_GET_OBJECT_STATIC_CALLBACKS_VOLATILE(code, f)              \
  if (hash == code && !strcasecmp(s, #f))                               \
    return CLASS_CHECK(&cw_ ## f)
#define HASH_GET_OBJECT_STATIC_CALLBACKS_REDECLARED(code, f)            \
  if (hash == code && !strcasecmp(s, #f))                               \
    return CLASS_CHECK(g->cwo_ ## f)
#define HASH_GET_CLASS_VAR_INIT(code, f)                                \
  if (hash == code && !strcasecmp(s, #f))                               \
    return cw_ ## f.os_getInit(var)
#define HASH_GET_CLASS_VAR_INIT_VOLATILE(code, f)                       \
  if (hash == code && !strcasecmp(s, #f))                               \
    return CLASS_CHECK(cw_ ## f.os_getInit(var))
#define HASH_GET_CLASS_VAR_INIT_REDECLARED(code, f)                     \
  if (hash == code && !strcasecmp(s, #f))                               \
    return CLASS_CHECK(g->cso_ ## f->os_getInit(var))
#define HASH_CREATE_OBJECT(code, f)                                     \
  if (hash == code && !strcasecmp(s, #f)) return co_ ## f(params, init)
#define HASH_CREATE_OBJECT_VOLATILE(code, f)                            \
  if (hash == code && !strcasecmp(s, #f))                               \
    return CLASS_CHECK(co_ ## f(params, init))
#define HASH_CREATE_OBJECT_REDECLARED(code, f)                          \
  if (hash == code && !strcasecmp(s, #f))                               \
    return CLASS_CHECK(g->cso_ ## f->create(params, init, root))
#define HASH_INCLUDE(code, file, fun)                                   \
  if (hash == code && !strcmp(file, s.c_str())) {                       \
    return pm_ ## fun(once, variables);                                 \
  }
#define HASH_INVOKE_FROM_EVAL(code, f)                                  \
  if (hash == code && !strcasecmp(s, #f)) return ei_ ## f(env, caller)
#define HASH_INVOKE_REDECLARED_FROM_EVAL(code, f)                       \
  if (hash == code && !strcasecmp(s, #f)) return g->ei_ ## f(env_caller)

///////////////////////////////////////////////////////////////////////////////
// global variable macros

#ifdef DIRECT_GLOBAL_VARIABLES

#define BEGIN_GVS()
#define GVS(s) gv_##s;
#define END_GVS(c)
#define GV(s) s

#else

#define BEGIN_GVS() enum _gv_enums_ {
#define GVS(s) gv_##s,
#define END_GVS(c) }; Variant gv[c];
#define GV(s) gv[GlobalVariables::gv_##s]

#endif

// Class declared flags

#define BEGIN_CDECS() enum _cdec_enums_ {
#define DEF_CDEC(s) cdec_##s,
#define END_CDECS(c) }; bool cdec[c];
#define CDEC(s) cdec[GlobalVariables::cdec_##s]

// Function declared flags
#define FVF_PREFIX "fvf_"
#define FVF(s) fvf_##s
///////////////////////////////////////////////////////////////////////////////
// code instrumentation or injections

#define DECLARE_THREAD_INFO                      \
  ThreadInfo *info __attribute__((__unused__)) = \
    ThreadInfo::s_threadInfo.get();

#ifdef INFINITE_LOOP_DETECTION
#define LOOP_COUNTER(n) int lc##n = 0;
#define LOOP_COUNTER_CHECK(n)                                   \
  if ((++lc##n & 1023) == 0) {                                  \
    RequestInjection ti(info);                                  \
    if (lc##n > 1000000) throw_infinite_loop_exception();       \
  }

#else
#define LOOP_COUNTER(n)
#define LOOP_COUNTER_CHECK(n)
#endif

#ifdef INFINITE_RECURSION_DETECTION
#define RECURSION_INJECTION RecursionInjection ri(info);
#else
#define RECURSION_INJECTION
#endif

#ifdef REQUEST_TIMEOUT_DETECTION
#define REQUEST_TIMEOUT_INJECTION RequestInjection ti(info);
#else
#define REQUEST_TIMEOUT_INJECTION
#endif

#ifdef HOTPROFILER
#define HOTPROFILER_INJECTION(n) ProfilerInjection pi(info, #n);
#ifndef HOTPROFILER_NO_BUILTIN
#define HOTPROFILER_INJECTION_BUILTIN(n) ProfilerInjection pi(info, #n);
#else
#define HOTPROFILER_INJECTION_BUILTIN(n)
#endif
#else
#define HOTPROFILER_INJECTION(n)
#define HOTPROFILER_INJECTION_BUILTIN(n)
#endif

// Stack frame injection is also for correctness, and cannot be disabled.
#define FRAME_INJECTION(c, n) FrameInjection fi(info, c, #n);
#define FRAME_INJECTION_FLAGS(c, n, f) FrameInjection fi(info, c, #n, NULL, f);
#define FRAME_INJECTION_WITH_THIS(c, n) FrameInjection fi(info, c, #n, this);

#ifdef ENABLE_FULL_SETLINE
#define LINE(n, e) (set_line(n), e)
#else
#define LINE(n, e) (fi.setLine(n), e)
#endif

// Get global variables from thread info.
#define DECLARE_GLOBAL_VARIABLES_INJECTION(g)       \
  GlobalVariables *g __attribute__((__unused__)) =  \
    info->m_globals;
#define DECLARE_SYSTEM_GLOBALS_INJECTION(g)         \
  SystemGlobals *g __attribute__((__unused__)) =    \
    (SystemGlobals *)info->m_globals;

#define CHECK_ONCE(n)                             \
  {                                               \
    bool &alreadyRun = g->run_ ## n;              \
    if (alreadyRun) { if (incOnce) return true; } \
    else alreadyRun = true;                       \
    if (!variables) variables = g;                \
  }

// code injected into beginning of every function/method
#define FUNCTION_INJECTION(n)                   \
  DECLARE_THREAD_INFO                           \
  RECURSION_INJECTION                           \
  REQUEST_TIMEOUT_INJECTION                     \
  HOTPROFILER_INJECTION(n)                      \
  FRAME_INJECTION(empty_string, n)              \
  DECLARE_GLOBAL_VARIABLES_INJECTION(g)         \

#define STATIC_METHOD_INJECTION(c, n)           \
  DECLARE_THREAD_INFO                           \
  RECURSION_INJECTION                           \
  REQUEST_TIMEOUT_INJECTION                     \
  HOTPROFILER_INJECTION(n)                      \
  FRAME_INJECTION(s_class_name, n)              \
  DECLARE_GLOBAL_VARIABLES_INJECTION(g)         \

#define INSTANCE_METHOD_INJECTION(c, n)         \
  DECLARE_THREAD_INFO                           \
  RECURSION_INJECTION                           \
  REQUEST_TIMEOUT_INJECTION                     \
  HOTPROFILER_INJECTION(n)                      \
  FRAME_INJECTION_WITH_THIS(s_class_name, n)    \
  DECLARE_GLOBAL_VARIABLES_INJECTION(g)         \

#define PSEUDOMAIN_INJECTION(n, esc)               \
  GlobalVariables *g = (GlobalVariables *)globals; \
  CHECK_ONCE(esc)                                  \
  DECLARE_THREAD_INFO                              \
  RECURSION_INJECTION                              \
  REQUEST_TIMEOUT_INJECTION                        \
  HOTPROFILER_INJECTION(n)                         \
  FRAME_INJECTION_FLAGS(empty_string, n, FrameInjection::PseudoMain) \

// code injected into every builtin function/method
#define FUNCTION_INJECTION_BUILTIN(n)           \
  DECLARE_THREAD_INFO                           \
  RECURSION_INJECTION                           \
  REQUEST_TIMEOUT_INJECTION                     \
  HOTPROFILER_INJECTION_BUILTIN(n)              \
  FRAME_INJECTION_FLAGS(empty_string, n, FrameInjection::BuiltinFunction) \
  DECLARE_SYSTEM_GLOBALS_INJECTION(g)           \

#define STATIC_METHOD_INJECTION_BUILTIN(c, n)   \
  DECLARE_THREAD_INFO                           \
  RECURSION_INJECTION                           \
  REQUEST_TIMEOUT_INJECTION                     \
  HOTPROFILER_INJECTION_BUILTIN(n)              \
  FRAME_INJECTION(s_class_name, n)              \
  DECLARE_SYSTEM_GLOBALS_INJECTION(g)           \

#define INSTANCE_METHOD_INJECTION_BUILTIN(c, n) \
  if (!o_id) throw_instance_method_fatal(#n);   \
  DECLARE_THREAD_INFO                           \
  RECURSION_INJECTION                           \
  REQUEST_TIMEOUT_INJECTION                     \
  HOTPROFILER_INJECTION_BUILTIN(n)              \
  FRAME_INJECTION_WITH_THIS(s_class_name, n)    \
  DECLARE_SYSTEM_GLOBALS_INJECTION(g)           \

#define PSEUDOMAIN_INJECTION_BUILTIN(n, esc)    \
  SystemGlobals *g = (SystemGlobals *)globals;  \
  CHECK_ONCE(esc)                               \
  DECLARE_THREAD_INFO                           \
  RECURSION_INJECTION                           \
  REQUEST_TIMEOUT_INJECTION                     \
  HOTPROFILER_INJECTION(n)                      \
  FRAME_INJECTION_FLAGS(empty_string, n, FrameInjection::PseudoMain) \

#define INTERCEPT_INJECTION_ALWAYS(name, func, args, rr)                \
  static char intercepted = -1;                                         \
  if (intercepted) {                                                    \
    Variant r, h = get_intercept_handler(name, &intercepted);           \
    if (!h.isNull() && handle_intercept(h, func, args, r)) return rr;   \
  }                                                                     \

#ifdef ENABLE_INTERCEPT
#define INTERCEPT_INJECTION(func, args, rr)       \
  INTERCEPT_INJECTION_ALWAYS(func, func, args, rr)
#else
#define INTERCEPT_INJECTION(func, args, rr)
#endif

#ifdef ENABLE_LATE_STATIC_BINDING

#define STATIC_CLASS_NAME_CALL(s, exp)                             \
  (FrameInjection::StaticClassNameHelper(info, s), exp)            \

#define BIND_CLASS_DOT  bindClass(info).
#define BIND_CLASS_ARROW(T) bindClass<c_##T>(info)->
#define INVOKE_STATIC_METHOD invoke_static_method_bind
#define INVOKE_STATIC_METHOD_MIL invoke_static_method_bind_mil

#else

#define STATIC_CLASS_NAME_CALL(s, exp) exp
#define BIND_CLASS_DOT
#define BIND_CLASS_ARROW(T)
#define INVOKE_STATIC_METHOD invoke_static_method
#define INVOKE_STATIC_METHOD_MIL invoke_static_method_mil

#endif

// for collecting function/method parameter type information at runtime
#define RTTI_INJECTION(v, id)                   \
  do {                                          \
    unsigned int *counter = getRTTICounter(id); \
    if (counter) {                              \
      counter[getDataTypeIndex(v.getType())]++; \
    }                                           \
  } while (0)

// causes a division by zero error at compile time if the assertion fails
// NOTE: use __LINE__, instead of __COUNTER__, for better compatibility
#define CT_CONCAT_HELPER(a, b) a##b
#define CT_CONCAT(a, b) CT_CONCAT_HELPER(a, b)
#define CT_ASSERT(cond) \
  enum { CT_CONCAT(compile_time_assert_, __LINE__) = 1/(!!(cond)) }

#define CT_ASSERT_DESCENDENT_OF_OBJECTDATA(T)   \
  do {                                          \
    if (false) {                                \
      ObjectData * dummy = NULL;                \
      if (static_cast<T*>(dummy)) {}            \
    }                                           \
  } while(0)                                    \

//////////////////////////////////////////////////////////////////////////////
}

#endif // __HPHP_MACROS_H__
