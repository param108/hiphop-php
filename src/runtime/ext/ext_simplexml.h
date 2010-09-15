/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010 Facebook, Inc. (http://www.facebook.com)          |
   | Copyright (c) 1997-2010 The PHP Group                                |
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

#ifndef __EXT_SIMPLEXML_H__
#define __EXT_SIMPLEXML_H__

// >>>>>> Generated by idl.php. Do NOT modify. <<<<<<

#include <runtime/base/base_includes.h>
#include <runtime/ext/ext_simplexml_include.h>
namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

Variant f_simplexml_load_string(CStrRef data, CStrRef class_name = "SimpleXMLElement", int64 options = 0, CStrRef ns = "", bool is_prefix = false);
Variant f_simplexml_load_file(CStrRef filename, CStrRef class_name = "SimpleXMLElement", int64 options = 0, CStrRef ns = "", bool is_prefix = false);
Variant f_libxml_get_errors();
Variant f_libxml_get_last_error();
void f_libxml_clear_errors();
bool f_libxml_use_internal_errors(CVarRef use_errors = null_variant);
void f_libxml_set_streams_context(CObjRef streams_context);
bool f_libxml_disable_entity_loader(bool disable = true);

///////////////////////////////////////////////////////////////////////////////
// class SimpleXMLElement

FORWARD_DECLARE_CLASS(SimpleXMLElement);
class c_SimpleXMLElement :
      public ExtObjectDataFlags<ObjectData::UseGet|
                                ObjectData::UseSet|
                                ObjectData::UseUnset> {
 public:
  BEGIN_CLASS_MAP(SimpleXMLElement)
  PARENT_CLASS(ArrayAccess)
  PARENT_CLASS(IteratorAggregate)
  PARENT_CLASS(Countable)
  END_CLASS_MAP(SimpleXMLElement)
  DECLARE_CLASS(SimpleXMLElement, SimpleXMLElement, ObjectData)
  DECLARE_INVOKES_FROM_EVAL
  ObjectData* dynCreate(CArrRef params, bool init = true);

  // need to implement
  public: c_SimpleXMLElement();
  public: ~c_SimpleXMLElement();
  public: void t___construct(CStrRef data, int64 options = 0, bool data_is_url = false, CStrRef ns = "", bool is_prefix = false);
  public: bool t_offsetexists(CVarRef index);
  public: Variant t_offsetget(CVarRef index);
  public: void t_offsetset(CVarRef index, CVarRef newvalue);
  public: void t_offsetunset(CVarRef index);
  public: Variant t_getiterator();
  public: int64 t_count();
  public: Variant t_xpath(CStrRef path);
  public: bool t_registerxpathnamespace(CStrRef prefix, CStrRef ns);
  public: Variant t_asxml(CStrRef filename = "");
  public: Array t_getnamespaces(bool recursive = false);
  public: Array t_getdocnamespaces(bool recursive = false);
  public: Object t_children(CStrRef ns = "", bool is_prefix = false);
  public: String t_getname();
  public: Object t_attributes(CStrRef ns = "", bool is_prefix = false);
  public: Variant t_addchild(CStrRef qname, CStrRef value = null_string, CStrRef ns = null_string);
  public: void t_addattribute(CStrRef qname, CStrRef value = null_string, CStrRef ns = null_string);
  public: String t___tostring();
  public: Variant t___get(Variant name);
  public: Variant t___set(Variant name, Variant value);
  public: bool t___isset(Variant name);
  public: Variant t___unset(Variant name);
  public: Variant t___destruct();
  public: void __attach_attributes();

  // implemented by HPHP
  public: c_SimpleXMLElement *create(String data, int64 options = 0, bool data_is_url = false, String ns = "", bool is_prefix = false);
  public: void dynConstruct(CArrRef Params);
  public: void dynConstructFromEval(Eval::VariableEnvironment &env,
                                    const Eval::FunctionCallExpression *call);
  public: virtual void destruct();

 public:
  Object m_doc;
  xmlNodePtr m_node;
  Variant m_children;
  Variant m_attributes;
  Array m_array;
  bool m_is_text;
  bool m_free_text;
  bool m_is_attribute;
  bool m_is_children;
  bool m_is_property;
  virtual bool o_toBoolean() const;
  virtual int64 o_toInt64() const;
  virtual double o_toDouble() const;
  virtual Array o_toArray() const;
  virtual Variant *___lval(Variant v_name);
 private:
  xmlXPathContextPtr m_xpath;
 public:
  void __populate_m_array();
};

///////////////////////////////////////////////////////////////////////////////
// class LibXMLError

FORWARD_DECLARE_CLASS(LibXMLError);
class c_LibXMLError : public ExtObjectData {
 public:
  BEGIN_CLASS_MAP(LibXMLError)
  END_CLASS_MAP(LibXMLError)
  DECLARE_CLASS(LibXMLError, LibXMLError, ObjectData)
  DECLARE_INVOKES_FROM_EVAL
  ObjectData* dynCreate(CArrRef params, bool init = true);

  // need to implement
  public: c_LibXMLError();
  public: ~c_LibXMLError();
  public: void t___construct();
  public: Variant t___destruct();

  // implemented by HPHP
  public: c_LibXMLError *create();
  public: void dynConstruct(CArrRef Params);
  public: void dynConstructFromEval(Eval::VariableEnvironment &env,
                                    const Eval::FunctionCallExpression *call);
  public: virtual void destruct();

};

///////////////////////////////////////////////////////////////////////////////
// class SimpleXMLElementIterator

FORWARD_DECLARE_CLASS(SimpleXMLElementIterator);
class c_SimpleXMLElementIterator : public ExtObjectData, public Sweepable {
 public:
  BEGIN_CLASS_MAP(SimpleXMLElementIterator)
  PARENT_CLASS(Iterator)
  END_CLASS_MAP(SimpleXMLElementIterator)
  DECLARE_CLASS(SimpleXMLElementIterator, SimpleXMLElementIterator, ObjectData)
  DECLARE_INVOKES_FROM_EVAL
  ObjectData* dynCreate(CArrRef params, bool init = true);

  // need to implement
  public: c_SimpleXMLElementIterator();
  public: ~c_SimpleXMLElementIterator();
  public: void t___construct();
  public: Variant t_current();
  public: Variant t_key();
  public: Variant t_next();
  public: Variant t_rewind();
  public: Variant t_valid();
  public: Variant t___destruct();

  // implemented by HPHP
  public: c_SimpleXMLElementIterator *create();
  public: void dynConstruct(CArrRef Params);
  public: void dynConstructFromEval(Eval::VariableEnvironment &env,
                                    const Eval::FunctionCallExpression *call);
  public: virtual void destruct();

public:
  void reset_iterator(c_SimpleXMLElement *parent);

  c_SimpleXMLElement *m_parent;
  ArrayIter *m_iter1;
  ArrayIter *m_iter2;
  Object     m_temp;
};

///////////////////////////////////////////////////////////////////////////////
}

#endif // __EXT_SIMPLEXML_H__
