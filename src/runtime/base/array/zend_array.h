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

#ifndef __HPHP_ZEND_ARRAY_H__
#define __HPHP_ZEND_ARRAY_H__

#include <runtime/base/types.h>
#include <runtime/base/array/array_data.h>
#include <runtime/base/memory/smart_allocator.h>
#include <runtime/base/complex_types.h>

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

class ArrayInit;

class ZendArray : public ArrayData {
public:
  friend class ArrayInit;

  ZendArray(uint nSize = 0);
  virtual ~ZendArray();

  virtual ssize_t size() const { return m_nNumOfElements;}

  virtual Variant getKey(ssize_t pos) const;
  virtual Variant getValue(ssize_t pos) const;
  virtual void fetchValue(ssize_t pos, Variant & v) const;
  virtual CVarRef getValueRef(ssize_t pos) const;
  virtual bool isVectorData() const;
  virtual bool supportValueRef() const { return true; }

  virtual ssize_t iter_begin() const;
  virtual ssize_t iter_end() const;
  virtual ssize_t iter_advance(ssize_t prev) const;
  virtual ssize_t iter_rewind(ssize_t prev) const;

  virtual Variant reset();
  virtual Variant prev();
  virtual Variant current() const;
  virtual Variant next();
  virtual Variant end();
  virtual Variant key() const;
  virtual Variant value(ssize_t &pos) const;
  virtual Variant each();

  virtual bool exists(int64   k) const;
  virtual bool exists(litstr  k) const;
  virtual bool exists(CStrRef k) const;
  virtual bool exists(CVarRef k) const;

  virtual bool idxExists(ssize_t idx) const;

  virtual Variant get(int64   k, bool error = false) const;
  virtual Variant get(litstr  k, bool error = false) const;
  virtual Variant get(CStrRef k, bool error = false) const;
  virtual Variant get(CVarRef k, bool error = false) const;

  virtual void load(CVarRef k, Variant &v) const;

  Variant fetch(CStrRef k) const;

  virtual ssize_t getIndex(int64 k) const;
  virtual ssize_t getIndex(litstr k) const;
  virtual ssize_t getIndex(CStrRef k) const;
  virtual ssize_t getIndex(CVarRef k) const;

  virtual ArrayData *lval(Variant *&ret, bool copy);
  virtual ArrayData *lval(int64   k, Variant *&ret, bool copy,
                          bool checkExist = false);
  virtual ArrayData *lval(litstr  k, Variant *&ret, bool copy,
                          bool checkExist = false);
  virtual ArrayData *lval(CStrRef k, Variant *&ret, bool copy,
                          bool checkExist = false);
  virtual ArrayData *lval(CVarRef k, Variant *&ret, bool copy,
                          bool checkExist = false);
  virtual ArrayData *lvalPtr(CStrRef k, Variant *&ret, bool copy,
                             bool create);

  virtual ArrayData *set(int64   k, CVarRef v, bool copy);
  virtual ArrayData *set(litstr  k, CVarRef v, bool copy);
  virtual ArrayData *set(CStrRef k, CVarRef v, bool copy);
  virtual ArrayData *set(CVarRef k, CVarRef v, bool copy);

  virtual ArrayData *add(int64   k, CVarRef v, bool copy);
  virtual ArrayData *add(CStrRef k, CVarRef v, bool copy);
  virtual ArrayData *add(CVarRef k, CVarRef v, bool copy);
  virtual ArrayData *addLval(int64   k, Variant *&ret, bool copy);
  virtual ArrayData *addLval(CStrRef k, Variant *&ret, bool copy);
  virtual ArrayData *addLval(CVarRef k, Variant *&ret, bool copy);

  virtual ArrayData *remove(int64   k, bool copy);
  virtual ArrayData *remove(litstr  k, bool copy);
  virtual ArrayData *remove(CStrRef k, bool copy);
  virtual ArrayData *remove(CVarRef k, bool copy);

  virtual ArrayData *copy() const;
  virtual ArrayData *append(CVarRef v, bool copy);
  virtual ArrayData *append(const ArrayData *elems, ArrayOp op, bool copy);
  virtual ArrayData *pop(Variant &value);
  virtual ArrayData *dequeue(Variant &value);
  virtual ArrayData *prepend(CVarRef v, bool copy);
  virtual void renumber();
  virtual void onSetStatic();

  virtual void getFullPos(FullPos &pos);
  virtual bool setFullPos(const FullPos &pos);
  virtual CVarRef currentRef();
  virtual CVarRef endRef();

  class Bucket {
  public:
    Bucket();
    Bucket(CVarRef d);

    // These two constructors should never be called directly, they are
    // only called from generated code.
    Bucket(StringData *k, CVarRef d) :
      key(k), data(d) {
      ASSERT(k->isLiteral());
      ASSERT(k->isStatic());
      h = k->getPrecomputedHash();
    }
    Bucket(int64 k, CVarRef d) : h(k), key(NULL), data(d) { }

    ~Bucket();

    int64       h;
    StringData *key;
    Variant     data;
    Bucket     *pListNext;
    Bucket     *pListLast;
    Bucket     *pNext;

    /**
     * Memory allocator methods.
     */
    DECLARE_SMART_ALLOCATION_NOCALLBACKS(Bucket);
    void dump();
  };

  // This constructor should never be called directly, it is only called
  // from generated code.
  ZendArray(uint nSize, unsigned long n, Bucket *bkts[]);

private:
  uint             m_nTableSize;
  uint             m_nTableMask;
  uint             m_nNumOfElements;
  unsigned long    m_nNextFreeElement;
  Bucket         * m_pListHead;
  Bucket         * m_pListTail;
  Bucket         **m_arBuckets;
  char             m_siPastEnd;
  char             m_linear;

  Bucket *find(int64 h) const;
  Bucket *find(const char *k, int len, int64 prehash) const;

  Bucket ** findForErase(int64 h) const;
  Bucket ** findForErase(const char *k, int len, int64 prehash) const;
  Bucket ** findForErase(Bucket * bucketPtr) const;

  bool nextInsert(CVarRef data);
  bool addLvalImpl(int64 h, Variant **pDest, bool doFind = true);
  bool addLvalImpl(StringData *key, int64 h, Variant **pDest,
                   bool doFind = true);
  bool addVal(int64 h, CVarRef data);
  bool addVal(StringData *key, CVarRef data);

  bool update(int64 h, CVarRef data);
  bool update(litstr key, CVarRef data);
  bool update(StringData *key, CVarRef data);

  void erase(Bucket ** prev);
  ZendArray *copyImpl() const;

  void resize();
  void rehash();

  void prepareBucketHeadsForWrite();

  /**
   * Memory allocator methods.
   */
  DECLARE_SMART_ALLOCATION(ZendArray, SmartAllocatorImpl::NeedRestoreOnce);
  bool calculate(int &size);
  void backup(LinearAllocator &allocator);
  void restore(const char *&data);
  void sweep();
};

class StaticEmptyZendArray : public ZendArray {
public:
  StaticEmptyZendArray() { setStatic();}

  static ZendArray *Get() { return &s_theEmptyArray; }

private:
  static StaticEmptyZendArray s_theEmptyArray;
};

///////////////////////////////////////////////////////////////////////////////
}

#endif // __HPHP_ZEND_ARRAY_H__
