/*
 * Copyright 2003-2005 Sun Microsystems, Inc.  All Rights Reserved.
 * Copyright 2007, 2008 Red Hat, Inc.
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

// Implementation of class atomic

#ifdef ARM
typedef int (__kernel_cmpxchg_t)(int oldval, int newval, volatile int *ptr);
#define __kernel_cmpxchg (*(__kernel_cmpxchg_t *) 0xffff0fc0)

static inline int arm_compare_and_swap(volatile int *ptr,
                                       int oldval,
                                       int newval) {
  int old = *ptr;
  __kernel_cmpxchg(oldval, newval, (volatile int*) ptr);
  return old;
}

static inline int arm_add_and_fetch(volatile int *dest, int add_value)
{
  arm_compare_and_swap(dest, *dest, *dest + add_value);
  return *dest;
}

static inline int arm_lock_test_and_set(volatile int *ptr, int newval)
{
  int old = *ptr;
  arm_compare_and_swap(ptr, *ptr, newval);
  return old;
}
#endif // ARM

inline void Atomic::store(jint store_value, volatile jint* dest)
{
  *dest = store_value;
}

inline void Atomic::store_ptr(intptr_t store_value, intptr_t* dest)
{
  Unimplemented();
}

inline jint Atomic::add(jint add_value, volatile jint* dest)
{
#ifdef ARM
  return arm_add_and_fetch(dest, add_value);
#else
  return __sync_add_and_fetch(dest, add_value);
#endif // ARM
}

inline intptr_t Atomic::add_ptr(intptr_t add_value, volatile intptr_t* dest)
{
#ifdef ARM
  return arm_add_and_fetch(dest, add_value);
#else
  return __sync_add_and_fetch(dest, add_value);
#endif // ARM
}

inline void* Atomic::add_ptr(intptr_t add_value, volatile void* dest)
{
  return (void *) add_ptr(add_value, (volatile intptr_t *) dest);
}

inline void Atomic::inc(volatile jint* dest)
{
  add(1, dest);
}

inline void Atomic::inc_ptr(volatile intptr_t* dest)
{
  add_ptr(1, dest);
}

inline void Atomic::inc_ptr(volatile void* dest)
{
  add_ptr(1, dest);
}

inline void Atomic::dec(volatile jint* dest)
{
  add(-1, dest);
}

inline void Atomic::dec_ptr(volatile intptr_t* dest)
{
  add_ptr(-1, dest);
}

inline void Atomic::dec_ptr(volatile void* dest)
{
  add_ptr(-1, dest);
}

inline jint Atomic::xchg(jint exchange_value, volatile jint* dest)
{
#ifdef ARM
  return arm_lock_test_and_set(dest, exchange_value);
#else
  // __sync_lock_test_and_set is a bizarrely named atomic exchange
  // operation.  Note that some platforms only support this with the
  // limitation that the only valid value to store is the immediate
  // constant 1.  There is a test for this in JNI_CreateJavaVM().
  return __sync_lock_test_and_set (dest, exchange_value);
#endif // ARM
}

inline intptr_t Atomic::xchg_ptr(intptr_t exchange_value,
                                 volatile intptr_t* dest) {
#ifdef ARM
  return arm_lock_test_and_set(dest, exchange_value);
#else
  return __sync_lock_test_and_set (dest, exchange_value);
#endif // ARM
}

inline void* Atomic::xchg_ptr(void* exchange_value, volatile void* dest)
{
  return (void *) xchg_ptr((intptr_t) exchange_value,
                           (volatile intptr_t*) dest);
}

inline jint Atomic::cmpxchg(jint exchange_value,
                            volatile jint* dest,
                            jint compare_value) {
#ifdef ARM
  return arm_compare_and_swap(dest, compare_value, exchange_value);
#else
  return __sync_val_compare_and_swap(dest, compare_value, exchange_value);
#endif // ARM
}

inline jlong Atomic::cmpxchg(jlong exchange_value,
                             volatile jlong* dest,
                             jlong compare_value) {

  return __sync_val_compare_and_swap(dest, compare_value, exchange_value);
}

inline intptr_t Atomic::cmpxchg_ptr(intptr_t exchange_value,
                                    volatile intptr_t* dest,
                                    intptr_t compare_value) {
#ifdef ARM
  return arm_compare_and_swap(dest, compare_value, exchange_value);
#else
  return __sync_val_compare_and_swap(dest, compare_value, exchange_value);
#endif // ARM
}

inline void* Atomic::cmpxchg_ptr(void* exchange_value,
                                 volatile void* dest,
                                 void* compare_value) {

  return (void *) cmpxchg_ptr((intptr_t) exchange_value,
                              (volatile intptr_t*) dest,
                              (intptr_t) compare_value);
}
