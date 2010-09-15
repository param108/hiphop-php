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

#ifndef __TYPE_H__
#define __TYPE_H__

#include <compiler/hphp.h>
#include <util/json.h>

#define NEW_TYPE(s) TypePtr(new Type(Type::KindOf ## s))

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

class CodeGenerator;
DECLARE_BOOST_TYPES(Expression);
DECLARE_BOOST_TYPES(AnalysisResult);
DECLARE_BOOST_TYPES(Type);

class Type : public JSON::ISerializable {
public:
  typedef int KindOf;

  static const KindOf KindOfVoid    = 0x0001;
  static const KindOf KindOfBoolean = 0x0002;
  static const KindOf KindOfByte    = 0x0004;
  static const KindOf KindOfInt16   = 0x0008;
  static const KindOf KindOfInt32   = 0x0010;
  static const KindOf KindOfInt64   = 0x0020;
  static const KindOf KindOfDouble  = 0x0040;
  static const KindOf KindOfString  = 0x0080;
  static const KindOf KindOfArray   = 0x0100;
  static const KindOf KindOfObject  = 0x0200;   // with classname
  static const KindOf KindOfVariant = 0xFFFF;

  static const KindOf KindOfNumeric = (KindOf)(KindOfDouble | KindOfInt64
                                    | KindOfInt32 | KindOfInt16 | KindOfByte);
  static const KindOf KindOfPrimitive = (KindOf)(KindOfNumeric | KindOfString);
  static const KindOf KindOfPlusOperand = (KindOf)(KindOfNumeric | KindOfArray);
  static const KindOf KindOfSequence = (KindOf)(KindOfString | KindOfArray);
  static const KindOf KindOfSome = (KindOf)0x7FFE;
  static const KindOf KindOfAny = (KindOf)0x7FFF;

  /**
   * Inferred types: types that a variable or a constant is sure to be.
   */
  static TypePtr Boolean;
  static TypePtr Byte;
  static TypePtr Int16;
  static TypePtr Int32;
  static TypePtr Int64;
  static TypePtr Double;
  static TypePtr String;
  static TypePtr Array;
  static TypePtr Variant;

  /**
   * Uncertain types: types that are ambiguous yet.
   */
  static TypePtr CreateObjectType(const std::string &classname);

  /**
   * For inferred, return static type objects; for uncertain, create new
   * ones.
   */
  static TypePtr GetType(KindOf kindOf);

  /**
   * Whether a type can be used as another type.
   */
  static bool IsLegalCast(AnalysisResultPtr ar, TypePtr from, TypePtr to);

  /**
   * Find the intersection between two sets of types.
   */
  static TypePtr Intersection(AnalysisResultPtr ar, TypePtr from, TypePtr to);

  /**
   * Whether or not a cast is needed during code generation.
   */
  static bool IsCastNeeded(AnalysisResultPtr ar, TypePtr from, TypePtr to);

  /**
   * When a variable's type is t1, and it's used as t2, do we need to
   * coerce variable's type? Normally, if t2 can be legally casted to t1,
   * this returns false.
   */
  static bool IsCoercionNeeded(AnalysisResultPtr ar, TypePtr t1, TypePtr t2);

  /**
   * When a variable is assigned with two types, what type a variable
   * should be?
   */
  static TypePtr Coerce(AnalysisResultPtr ar, TypePtr type1, TypePtr type2);
  static TypePtr Union(AnalysisResultPtr ar, TypePtr type1, TypePtr type2);

  /**
   * When two types have been inferred for an expression, what type
   * should it be?
   */
  static TypePtr Inferred(AnalysisResultPtr ar, TypePtr type1, TypePtr type2);

  /**
   * Whether or not two types are the same.
   */
  static bool SameType(TypePtr type1, TypePtr type2);

  /**
   * Testing type conversion for constants.
   */
  static bool IsBadTypeConversion(AnalysisResultPtr ar, TypePtr from,
                                  TypePtr to, bool coercing);
  static bool IsExactType(KindOf kindOf);

private:
  Type(KindOf kindOf, const std::string &name);

public:
  /**
   * KindOf testing.
   */
  Type(KindOf kindOf);
  bool is(KindOf kindOf) const { return m_kindOf == kindOf;}
  bool isExactType() const { return IsExactType(m_kindOf); }
  bool mustBe(KindOf kindOf) const { return !(m_kindOf & ~kindOf); }
  bool couldBe(KindOf kindOf) const { return m_kindOf & kindOf; }

  KindOf getKindOf() const { return m_kindOf;}
  bool isInteger() const;
  bool isSpecificObject() const;
  bool isNonConvertibleType() const; // other types cannot convert to them
  bool isPrimitive() const {
    return IsExactType(m_kindOf) && (m_kindOf <= KindOfDouble);
  }
  bool isNoObjectInvolved() const;
  const std::string &getName() const { return m_name;}
  static TypePtr combinedPrimType(TypePtr t1, TypePtr t2);

  /**
   * Generate type specifier in C++.
   */
  std::string getCPPDecl(CodeGenerator &cg, AnalysisResultPtr ar);
  void outputCPPDecl(CodeGenerator &cg, AnalysisResultPtr ar);

  /**
   * Generate type conversion in C++.
   */
  void outputCPPCast(CodeGenerator &cg, AnalysisResultPtr ar);

  /**
   * Generate variable initialization code.
   */
  const char *getCPPInitializer();

  /**
   * Type hint names in PHP.
   */
  std::string getPHPName();

  /**
   * Debug dump.
   */
  std::string toString() const;
  static void Dump(TypePtr type, const char *fmt = "%s");
  static void Dump(ExpressionPtr exp);

  /**
   * Implements JSON::ISerializable.
   */
  virtual void serialize(JSON::OutputStream &out) const;

  /**
   * For stats reporting.
   */
  void count(std::map<std::string, int> &counts);

private:
  KindOf m_kindOf;
  std::string m_name;
};

///////////////////////////////////////////////////////////////////////////////
}
#endif // __TYPE_H__
