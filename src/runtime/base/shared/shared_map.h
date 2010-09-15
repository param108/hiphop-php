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

#ifndef __SHARED_MAP_H__
#define __SHARED_MAP_H__

#include <util/shared_memory_allocator.h>
#include <runtime/base/shared/shared_variant.h>
#include <runtime/base/array/array_data.h>
#include <runtime/base/complex_types.h>
#include <runtime/base/builtin_functions.h>

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

/**
 * Wrapper for a shared memory map.
 */
class SharedMap : public ArrayData {
public:
  SharedMap(SharedVariant* source);

  ~SharedMap() {
    m_arr->decRef();
  }

  virtual SharedVariant *getSharedVariant() const { return m_arr; }

  ssize_t size() const {
    return m_arr->arrSize();
  }

  Variant getKey(ssize_t pos) const {
    return m_arr->getKey(pos);
  }

  Variant getValue(ssize_t pos) const {
    SharedVariant* v = m_arr->getValue(pos);
    return v ? getLocal(v) : null;
  }

  bool exists(int64 k) const {
    return exists(Variant(k));
  }
  bool exists(litstr k) const {
    return exists(Variant(k));
  }
  bool exists(CStrRef k) const {
    return exists(Variant(k));
  }
  bool exists(CVarRef k) const;

  bool idxExists(ssize_t idx) const {
    return idx < size();
  }

  Variant get(int64 k, bool error = false) const {
    return get(Variant(k), error);
  }
  Variant get(litstr k, bool error = false) const {
    return get(Variant(k), error);
  }
  Variant get(CStrRef k, bool error = false) const {
    return get(Variant(k), error);
  }
  Variant get(CVarRef k, bool error = false) const;

  ssize_t getIndex(int64 k) const {
    return getIndex(Variant(k));
  }
  ssize_t getIndex(litstr k) const {
    return getIndex(Variant(k));
  }
  ssize_t getIndex(CStrRef k) const {
    return getIndex(Variant(k));
  }
  ssize_t getIndex(CVarRef k) const {
    return m_arr->getIndex(k);
  }

  ArrayData *lval(Variant *&ret, bool copy);
  virtual ArrayData *lval(int64   k, Variant *&ret, bool copy,
                          bool checkExist = false);
  virtual ArrayData *lval(litstr  k, Variant *&ret, bool copy,
                          bool checkExist = false);
  virtual ArrayData *lval(CStrRef k, Variant *&ret, bool copy,
                          bool checkExist = false);
  virtual ArrayData *lval(CVarRef k, Variant *&ret, bool copy,
                          bool checkExist = false);

  ArrayData *set(int64   k, CVarRef v, bool copy);
  ArrayData *set(litstr  k, CVarRef v, bool copy);
  ArrayData *set(CStrRef k, CVarRef v, bool copy);
  ArrayData *set(CVarRef k, CVarRef v, bool copy);

  ArrayData *remove(int64   k, bool copy);
  ArrayData *remove(litstr  k, bool copy);
  ArrayData *remove(CStrRef k, bool copy);
  ArrayData *remove(CVarRef k, bool copy);

  ArrayData *copy() const;

  ArrayData *append(CVarRef v, bool copy);
  ArrayData *append(const ArrayData *elems, ArrayOp op, bool copy);

  ArrayData *prepend(CVarRef v, bool copy);

  /**
   * Memory allocator methods.
   */
  DECLARE_SMART_ALLOCATION(SharedMap, SmartAllocatorImpl::NeedRestore);
  bool calculate(int &size) { return true;}
  void backup(LinearAllocator &allocator) {
    m_arr->incRef(); // protect it
  }
  void restore(const char *&data) { m_arr->incRef();}
  void sweep() { m_arr->decRef();}

  virtual ArrayData *escalate(bool mutableIteration = false) const;

private:
  SharedVariant *m_arr;
  mutable Array m_localCache;

  Variant getLocal(SharedVariant *sv) const {
    ASSERT(sv);
    if (!sv->shouldCache()) return sv->toLocal();
    int64 key = (int64)sv;
    key = ((key & 0xfll) << 60) | (key >> 4);
    Variant v = m_localCache.rvalAt(key);
    if (v.isNull()) {
      v = sv->toLocal();
      if (!v.isNull()) m_localCache.set(key, v);
    }
    return v;
  }
};

///////////////////////////////////////////////////////////////////////////////
}

#endif // __SHARED_MAP_H__
