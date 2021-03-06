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
// @generated by HipHop Compiler

#ifndef __GENERATED_php_classes_exception_h__
#define __GENERATED_php_classes_exception_h__

#include <runtime/base/hphp_system.h>
#include <php/classes/exception.fw.h>

// Declarations
#include <cls/UnexpectedValueException.h>
#include <cls/OverflowException.h>
#include <cls/OutOfBoundsException.h>
#include <cls/LogicException.h>
#include <cls/RangeException.h>
#include <cls/InvalidArgumentException.h>
#include <cls/UnderflowException.h>
#include <cls/OutOfRangeException.h>
#include <cls/BadMethodCallException.h>
#include <cls/RuntimeException.h>
#include <cls/Exception.h>
#include <cls/ErrorException.h>
#include <cls/BadFunctionCallException.h>
#include <cls/LengthException.h>
#include <cls/DomainException.h>

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

// Includes and Functions
Variant pm_php$classes$exception_php(bool incOnce = false, LVariableTable* variables = NULL, Globals *globals = get_globals());

// Redeclared Functions

// Dynamic Class Declarations
Object co_UnexpectedValueException(CArrRef params, bool init = true);
Object co_OverflowException(CArrRef params, bool init = true);
Object co_OutOfBoundsException(CArrRef params, bool init = true);
Object co_LogicException(CArrRef params, bool init = true);
Object co_RangeException(CArrRef params, bool init = true);
Object co_InvalidArgumentException(CArrRef params, bool init = true);
Object co_UnderflowException(CArrRef params, bool init = true);
Object co_OutOfRangeException(CArrRef params, bool init = true);
Object co_BadMethodCallException(CArrRef params, bool init = true);
Object co_RuntimeException(CArrRef params, bool init = true);
Object co_Exception(CArrRef params, bool init = true);
Object co_ErrorException(CArrRef params, bool init = true);
Object co_BadFunctionCallException(CArrRef params, bool init = true);
Object co_LengthException(CArrRef params, bool init = true);
Object co_DomainException(CArrRef params, bool init = true);

///////////////////////////////////////////////////////////////////////////////
}

#endif // __GENERATED_php_classes_exception_h__
