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

#ifndef __BASE_H__
#define __BASE_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <errno.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/poll.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <list>
#include <set>
#include <deque>
#include <exception>
#include <ext/hash_map>
#include <ext/hash_set>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <boost/foreach.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/filesystem/operations.hpp>

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////
// system includes

#if __WORDSIZE == 64
#define WORDSIZE_IS_64
#endif

typedef unsigned char uchar;
typedef char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;

#ifndef ULLONG_MAX
#define ULLONG_MAX 0xffffffffffffffffULL
#endif

///////////////////////////////////////////////////////////////////////////////
// stl classes

#define hphp_hash_map __gnu_cxx::hash_map
#define hphp_hash_set __gnu_cxx::hash_set
#define hphp_hash     __gnu_cxx::hash

struct ltstr {
  bool operator()(const char *s1, const char *s2) const {
    return strcmp(s1, s2) < 0;
  }
};

struct eqstr {
  bool operator()(const char* s1, const char* s2) const {
    return strcmp(s1, s2) == 0;
  }
};

struct stdltstr {
  bool operator()(const std::string &s1, const std::string &s2) const {
    return strcmp(s1.c_str(), s2.c_str()) < 0;
  }
};

struct stdltistr {
  bool operator()(const std::string &s1, const std::string &s2) const {
    return strcasecmp(s1.c_str(), s2.c_str()) < 0;
  }
};

struct string_hash {
  size_t operator()(const std::string &s) const {
    return __gnu_cxx::__stl_hash_string(s.c_str());
  }
};

template<typename T>
struct pointer_hash {
  size_t operator() (const T *const &p) const {
    return (size_t)p;
  }
};

struct int64_hash {
  size_t operator() (const int64 v) const {
    return (size_t)v;
  }
};

template<typename T>
class hphp_const_char_map :
    public hphp_hash_map<const char *, T, hphp_hash<const char *>, eqstr> {
};

template<typename T>
class hphp_string_map :
    public hphp_hash_map<std::string, T, string_hash> {
};

typedef hphp_hash_set<std::string, string_hash> hphp_string_set;
typedef hphp_hash_set<const char *, hphp_hash<const char *>,
                      eqstr> hphp_const_char_set;

typedef hphp_hash_map<void*, void*, pointer_hash<void> > PointerMap;
typedef hphp_hash_map<void*, int, pointer_hash<void> > PointerCounterMap;
typedef hphp_hash_set<void*, pointer_hash<void> > PointerSet;

typedef std::vector<std::string> StringVec;
typedef boost::shared_ptr<std::vector<std::string> > StringVecPtr;
typedef std::vector<std::pair<std::string, std::string> > StringPairVec;
typedef std::vector<StringPairVec> StringPairVecVec;

///////////////////////////////////////////////////////////////////////////////
// boost

// Let us always use hphp's definition of DECLARE_BOOST_TYPES, esp. when it is
// used as an external library.
#ifdef DECLARE_BOOST_TYPES
#undef DECLARE_BOOST_TYPES
#endif

#define DECLARE_BOOST_TYPES(classname)                                  \
  class classname;                                                      \
  typedef boost::shared_ptr<classname> classname ## Ptr;                \
  typedef boost::weak_ptr<classname> classname ## WeakPtr;              \
  typedef boost::shared_ptr<const classname> classname ## ConstPtr;     \
  typedef std::vector<classname ## Ptr> classname ## PtrVec;            \
  typedef std::set<classname ## Ptr> classname ## PtrSet;               \
  typedef std::list<classname ## Ptr> classname ## PtrList;             \
  typedef std::deque<classname ## Ptr> classname ## PtrQueue;           \
  typedef __gnu_cxx::hash_map<std::string, classname ## Ptr,            \
    string_hash> StringTo ## classname ## PtrMap;                       \
  typedef __gnu_cxx::hash_map<std::string, classname ## PtrVec,         \
    string_hash> StringTo ## classname ## PtrVecMap;                    \
  typedef __gnu_cxx::hash_map<std::string, classname ## PtrSet,         \
    string_hash> StringTo ## classname ## PtrSetMap;                    \

typedef boost::shared_ptr<FILE> FilePtr;

struct null_deleter {
  void operator()(void const *) const {
  }
};

struct file_closer {
  void operator()(FILE *f) const {
    if (f) fclose(f);
  }
};

///////////////////////////////////////////////////////////////////////////////
// ICC compat
#if !defined(__ICC) || __ICC>=1200
#define ATTRIBUTE_COLD __attribute__((cold))
#else
#define ATTRIBUTE_COLD
#endif

///////////////////////////////////////////////////////////////////////////////
// Fast Method Call generation control,
// #ifndef NOFMCGEN - ON by default
// #ifdef FMCGEN - OFF by default
#ifndef NOFMCGEN
#define FMC "#ifndef NOFMCGEN\n"
#else
#define FMC "#ifdef FMCGEN\n"
#endif

///////////////////////////////////////////////////////////////////////////////
// debugging

#include <assert.h>

#ifdef RELEASE
#ifndef ALWAYS_ASSERT
#define ASSERT(x)
#else
#define ASSERT(x) assert(x)
#endif
#else
#define ASSERT(x) assert(x)
#endif

///////////////////////////////////////////////////////////////////////////////
}

#endif // __BASE_H__
