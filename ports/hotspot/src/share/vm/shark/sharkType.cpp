/*
 * Copyright 1999-2007 Sun Microsystems, Inc.  All Rights Reserved.
 * Copyright 2008 Red Hat, Inc.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 */

#include "incls/_precompiled.incl"
#include "incls/_sharkType.cpp.incl"

using namespace llvm;

const PointerType*  SharkType::_cpCacheEntry_type;
const FunctionType* SharkType::_interpreter_entry_type;
const PointerType*  SharkType::_itableOffsetEntry_type;
const PointerType*  SharkType::_klass_type;
const PointerType*  SharkType::_methodOop_type;
const ArrayType*    SharkType::_monitor_type;
const PointerType*  SharkType::_oop_type;
const FunctionType* SharkType::_shark_entry_type;
const PointerType*  SharkType::_thread_type;
const PointerType*  SharkType::_zeroStack_type;

const Type* SharkType::_to_stackType_tab[T_CONFLICT + 1];
const Type* SharkType::_to_arrayType_tab[T_CONFLICT + 1];

void SharkType::initialize()
{
  // VM types
  _cpCacheEntry_type = PointerType::getUnqual(
    ArrayType::get(Type::Int8Ty, sizeof(ConstantPoolCacheEntry)));
  
  _itableOffsetEntry_type = PointerType::getUnqual(
    ArrayType::get(Type::Int8Ty, itableOffsetEntry::size() * wordSize));
  
  _klass_type = PointerType::getUnqual(
    ArrayType::get(Type::Int8Ty, sizeof(Klass)));
  
  _methodOop_type = PointerType::getUnqual(
    ArrayType::get(Type::Int8Ty, sizeof(methodOopDesc)));
  
  _monitor_type = ArrayType::get(
      Type::Int8Ty,
      frame::interpreter_frame_monitor_size() * wordSize);
  
  _oop_type = PointerType::getUnqual(
    ArrayType::get(Type::Int8Ty, sizeof(oopDesc)));

  _thread_type = PointerType::getUnqual(
    ArrayType::get(Type::Int8Ty, sizeof(JavaThread)));

  _zeroStack_type = PointerType::getUnqual(
    ArrayType::get(Type::Int8Ty, sizeof(ZeroStack)));

  std::vector<const Type*> params;
  params.push_back(methodOop_type());
  params.push_back(thread_type());
  _interpreter_entry_type = FunctionType::get(Type::VoidTy, params, false);

  params.clear();
  params.push_back(methodOop_type());
  params.push_back(intptr_type());
  params.push_back(thread_type());
  _shark_entry_type = FunctionType::get(Type::VoidTy, params, false);

  // Java types a) on the stack and in fields, and b) in arrays
  for (int i = 0; i < T_CONFLICT + 1; i++) {
    switch (i) {
    case T_BOOLEAN:
      _to_stackType_tab[i] = jint_type();
      _to_arrayType_tab[i] = jboolean_type();
      break;
      
    case T_BYTE:
      _to_stackType_tab[i] = jint_type();
      _to_arrayType_tab[i] = jbyte_type();
      break;

    case T_CHAR:
      _to_stackType_tab[i] = jint_type();
      _to_arrayType_tab[i] = jchar_type();
      break;

    case T_SHORT:
      _to_stackType_tab[i] = jint_type();
      _to_arrayType_tab[i] = jshort_type();
      break;

    case T_INT:
      _to_stackType_tab[i] = jint_type();
      _to_arrayType_tab[i] = jint_type();
      break;

    case T_LONG:
      _to_stackType_tab[i] = jlong_type();
      _to_arrayType_tab[i] = jlong_type();
      break;

    case T_FLOAT:
      _to_stackType_tab[i] = jfloat_type();
      _to_arrayType_tab[i] = jfloat_type();
      break;

    case T_DOUBLE:
      _to_stackType_tab[i] = jdouble_type();
      _to_arrayType_tab[i] = jdouble_type();
      break;

    case T_OBJECT:
    case T_ARRAY:
      _to_stackType_tab[i] = jobject_type();
      _to_arrayType_tab[i] = jobject_type();
      break;
    }
  }
}
