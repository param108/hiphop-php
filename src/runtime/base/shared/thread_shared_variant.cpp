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

#include <runtime/base/shared/thread_shared_variant.h>
#include <runtime/ext/ext_variable.h>
#include <runtime/ext/ext_apc.h>
#include <runtime/base/shared/shared_map.h>
#include <runtime/base/runtime_option.h>

using namespace std;

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

ThreadSharedVariant::ThreadSharedVariant(CVarRef source, bool serialized,
                                         bool inner /* = false */) {
  ASSERT(!serialized || source.isString());

  setOwner();
  m_ref = 1;

  switch (source.getType()) {
  case KindOfBoolean:
    {
      m_type = KindOfBoolean;
      m_data.num = source.toBoolean();
      break;
    }
  case KindOfByte:
  case KindOfInt16:
  case KindOfInt32:
  case KindOfInt64:
    {
      m_type = KindOfInt64;
      m_data.num = source.toInt64();
      break;
    }
  case KindOfDouble:
    {
      m_type = KindOfDouble;
      m_data.dbl = source.toDouble();
      break;
    }
  case KindOfStaticString:
  case KindOfString:
    {
      String s = source.toString();
      m_type = serialized ? KindOfObject : KindOfString;
      if (serialized) {
        // It is priming, and there might not be the right class definitions
        // for unserialization.
        s = apc_reserialize(s);
      }
      m_data.str = s->copy(true);
      break;
    }
  case KindOfArray:
    {
      m_type = KindOfArray;

      ArrayData *arr = source.getArrayData();

      if (!inner) {
        // only need to call hasInternalReference() on the toplevel array
        PointerSet seen;
        if (arr->hasInternalReference(seen)) {
          setSerializedArray();
          setShouldCache();
          String s = apc_serialize(source);
          m_data.str = new StringData(s.data(), s.size(), CopyString);
          break;
        }
      }

      size_t size = arr->size();
      if (arr->isVectorData()) {
        setIsVector();
        m_data.vec = new VectorData(size);
        uint i = 0;
        for (ArrayIter it(arr); !it.end(); it.next(), i++) {
          ThreadSharedVariant* val = createAnother(it.second(), false, true);
          if (val->shouldCache()) setShouldCache();
          m_data.vec->vals[i] = val;
        }
      } else {
        m_data.map = new ImmutableMap(size);
        uint i = 0;
        for (ArrayIter it(arr); !it.end(); it.next(), i++) {
          ThreadSharedVariant* key = createAnother(it.first(), false);
          ThreadSharedVariant* val = createAnother(it.second(), false, true);
          if (val->shouldCache()) setShouldCache();
          m_data.map->add(key, val);
        }
      }
      break;
    }
  default:
    {
      m_type = KindOfObject;
      setShouldCache();
      String s = apc_serialize(source);
      m_data.str = new StringData(s.data(), s.size(), CopyString);
      break;
    }
  }
}

Variant ThreadSharedVariant::toLocal() {
  ASSERT(getOwner());
  switch (m_type) {
  case KindOfBoolean:
    {
      return (bool)m_data.num;
    }
  case KindOfInt64:
    {
      return m_data.num;
    }
  case KindOfDouble:
    {
      return m_data.dbl;
    }
  case KindOfString:
    {
      if (m_data.str->isStatic()) return m_data.str;
      return NEW(StringData)(this);
    }
  case KindOfArray:
    {
      if (getSerializedArray()) {
        return apc_unserialize(String(m_data.str->data(), m_data.str->size(),
                                      AttachLiteral));
      }
      return NEW(SharedMap)(this);
    }
  default:
    {
      ASSERT(m_type == KindOfObject);
      return apc_unserialize(String(m_data.str->data(), m_data.str->size(),
                                    AttachLiteral));
    }
  }
}

void ThreadSharedVariant::dump(std::string &out) {
  out += "ref(";
  out += boost::lexical_cast<string>(m_ref);
  out += ") ";
  switch (m_type) {
  case KindOfBoolean:
    out += "boolean: ";
    out += m_data.num ? "true" : "false";
    break;
  case KindOfInt64:
    out += "int: ";
    out += boost::lexical_cast<string>(m_data.num);
    break;
  case KindOfDouble:
    out += "double: ";
    out += boost::lexical_cast<string>(m_data.dbl);
    break;
  case KindOfString:
    out += "string(";
    out += boost::lexical_cast<string>(stringLength());
    out += "): ";
    out += stringData();
    break;
  case KindOfArray:
    if (getSerializedArray()) {
      out += "array: ";
      out += m_data.str->data();
    } else {
      SharedMap(this).dump(out);
    }
    break;
  default:
    out += "object: ";
    out += m_data.str->data();
    break;
  }
  out += "\n";
}

ThreadSharedVariant::~ThreadSharedVariant() {
  switch (m_type) {
  case KindOfString:
  case KindOfObject:
    if (getOwner()) {
      m_data.str->destruct();
    }
    break;
  case KindOfArray:
    {
      if (getSerializedArray()) {
        if (getOwner()) {
          m_data.str->destruct();
        }
        break;
      }

      ASSERT(getOwner());
      if (getIsVector()) delete m_data.vec;
      else delete m_data.map;
    }
    break;
  default:
    break;
  }
}

///////////////////////////////////////////////////////////////////////////////

const char *ThreadSharedVariant::stringData() const {
  ASSERT(is(KindOfString));
  return m_data.str->data();
}

size_t ThreadSharedVariant::stringLength() const {
  ASSERT(is(KindOfString));
  return m_data.str->size();
}

size_t ThreadSharedVariant::arrSize() const {
  ASSERT(is(KindOfArray));
  if (getIsVector()) return m_data.vec->size;
  return m_data.map->size();
}

int ThreadSharedVariant::getIndex(CVarRef key) {
  ASSERT(is(KindOfArray));
  switch (key.getType()) {
  case KindOfByte:
  case KindOfInt16:
  case KindOfInt32:
  case KindOfInt64: {
    int64 num = key.getNumData();
    if (getIsVector()) {
      if (num < 0 || (size_t) num >= m_data.vec->size) return -1;
      return num;
    }
    return m_data.map->indexOf(num);
  }
  case KindOfStaticString:
  case KindOfString: {
    if (getIsVector()) return -1;
    StringData *sd = key.getStringData();
    return m_data.map->indexOf(sd);
  }
  default:
    // No other types are legitimate keys
    break;
  }
  return -1;
}

SharedVariant* ThreadSharedVariant::get(CVarRef key) {
  int idx = getIndex(key);
  if (idx != -1) {
    if (getIsVector()) return m_data.vec->vals[idx];
    return m_data.map->getValIndex(idx);
  }
  return NULL;
}

bool ThreadSharedVariant::exists(CVarRef key) {
  ASSERT(is(KindOfArray));
  int idx = getIndex(key);
  return idx != -1;
}

void ThreadSharedVariant::loadElems(ArrayData *&elems,
                                    const SharedMap &sharedMap,
                                    bool keepRef /* = false */) {
  ASSERT(is(KindOfArray));
  uint count = arrSize();
  ArrayInit ai(count, getIsVector(), keepRef);
  for (uint i = 0; i < count; i++) {
    if (getIsVector()) {
      ai.add((int64)i, sharedMap.getValue(i), true);
    } else {
      ai.add(m_data.map->getKeyIndex(i)->toLocal(), sharedMap.getValue(i),
             true);
    }
  }
  elems = ai.create();
  if (elems->isStatic()) elems = elems->copy();
}

ThreadSharedVariant *ThreadSharedVariant::createAnother
(CVarRef source, bool serialized, bool inner /* = false */) {
  SharedVariant *wrapped = source.getSharedVariant();
  if (wrapped) {
    wrapped->incRef();
    // static cast should be enough
    return (ThreadSharedVariant *)wrapped;
  }
  return new ThreadSharedVariant(source, serialized, inner);
}

void ThreadSharedVariant::getStats(SharedVariantStats *stats) {
  stats->initStats();
  stats->variantCount = 1;
  switch (m_type) {
  case KindOfBoolean:
  case KindOfInt64:
    stats->dataSize = sizeof(m_data.num);
    stats->dataTotalSize = sizeof(ThreadSharedVariant);
    break;
  case KindOfDouble:
    stats->dataSize = sizeof(m_data.dbl);
    stats->dataTotalSize = sizeof(ThreadSharedVariant);
    break;
  case KindOfString:
  case KindOfObject:
    if (m_data.str->isStatic()) {
      stats->dataSize = 0;
      stats->dataTotalSize = sizeof(ThreadSharedVariant);
      break;
    }
    stats->dataSize = m_data.str->size();
    stats->dataTotalSize = sizeof(ThreadSharedVariant) + sizeof(StringData) +
                           stats->dataSize;
    break;
  default:
    ASSERT(is(KindOfArray));
    if (getSerializedArray()) {
      stats->dataSize = m_data.str->size();
      stats->dataTotalSize = sizeof(ThreadSharedVariant) + sizeof(StringData) +
                             stats->dataSize;
      break;
    }
    if (getIsVector()) {
      stats->dataTotalSize = sizeof(ThreadSharedVariant) + sizeof(VectorData);
      stats->dataTotalSize += sizeof(ThreadSharedVariant*) * m_data.vec->size;
      for (size_t i = 0; i < m_data.vec->size; i++) {
        ThreadSharedVariant *v = m_data.vec->vals[i];
        SharedVariantStats childStats;
        v->getStats(&childStats);
        stats->addChildStats(&childStats);
      }
    } else {
      ImmutableMap *map = m_data.map;
      stats->dataTotalSize = sizeof(ThreadSharedVariant) + map->getStructSize();
      for (int i = 0; i < map->size(); i++) {
        SharedVariantStats childStats;
        map->getKeyIndex(i)->getStats(&childStats);
        stats->addChildStats(&childStats);
        map->getValIndex(i)->getStats(&childStats);
        stats->addChildStats(&childStats);
      }
    }
    break;
  }
}

///////////////////////////////////////////////////////////////////////////////
}
