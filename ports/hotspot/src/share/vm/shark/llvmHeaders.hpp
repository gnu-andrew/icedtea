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

#ifdef assert
  #undef assert
#endif

#include <llvm/Argument.h>
#include <llvm/Constants.h>
#include <llvm/DerivedTypes.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Instructions.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#if SHARK_LLVM_VERSION < 27
#include <llvm/ModuleProvider.h>
#endif
#include <llvm/Support/IRBuilder.h>
#include <llvm/System/Threading.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/Type.h>
#include <llvm/ExecutionEngine/JITMemoryManager.h>
#include <llvm/Support/CommandLine.h>
#if SHARK_LLVM_VERSION >= 27
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Support/Debug.h>
#include <llvm/System/Host.h>
#endif

#include <map>

#ifdef assert
  #undef assert
#endif

// from hotspot/src/share/vm/utilities/debug.hpp
#ifdef ASSERT
  #define assert(p, msg)                                          \
    if (!(p)) {                                                  \
      report_assertion_failure(__FILE__, __LINE__,               \
                              "assert(" XSTR(p) ",\"" msg "\")");\
      BREAKPOINT;                                                \
    }
#else
  #define assert(p, msg)
#endif
