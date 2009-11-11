/*
 * Copyright 1999-2007 Sun Microsystems, Inc.  All Rights Reserved.
 * Copyright 2008, 2009 Red Hat, Inc.
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

class LLVMValue : public AllStatic {
 public:
  static llvm::ConstantInt* jbyte_constant(jbyte value)
  {
    return llvm::ConstantInt::get(SharkType::jbyte_type(), value, true);
  }
  static llvm::ConstantInt* jint_constant(jint value)
  {
    return llvm::ConstantInt::get(SharkType::jint_type(), value, true);
  }
  static llvm::ConstantInt* jlong_constant(jlong value)
  {
    return llvm::ConstantInt::get(SharkType::jlong_type(), value, true);
  }
  static llvm::ConstantFP* jfloat_constant(jfloat value)
  {
#if SHARK_LLVM_VERSION >= 26
    return llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(value));
#else
    return llvm::ConstantFP::get(SharkType::jfloat_type(), value);
#endif
  }
  static llvm::ConstantFP* jdouble_constant(jdouble value)
  {
#if SHARK_LLVM_VERSION >= 26
    return llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(value));
#else
    return llvm::ConstantFP::get(SharkType::jdouble_type(), value);
#endif
  }
  static llvm::ConstantPointerNull* null()
  {
    return llvm::ConstantPointerNull::get(SharkType::oop_type());
  }

 public:
  static llvm::ConstantInt* bit_constant(int value)
  {
#if SHARK_LLVM_VERSION >= 26
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(llvm::getGlobalContext()), value, false);
#else
    return llvm::ConstantInt::get(llvm::Type::Int1Ty, value, false);
#endif
  }
  static llvm::ConstantInt* intptr_constant(intptr_t value)
  {
    return llvm::ConstantInt::get(SharkType::intptr_type(), value, false);
  }
};
