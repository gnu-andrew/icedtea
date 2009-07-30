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

#include "incls/_precompiled.incl"
#include "incls/_sharkRuntime.cpp.incl"

using namespace llvm;

JRT_ENTRY(int, SharkRuntime::find_exception_handler(JavaThread* thread,
                                                    int*        indexes,
                                                    int         num_indexes))
{
  constantPoolHandle pool(thread, method(thread)->constants());
  KlassHandle exc_klass(thread, ((oop) tos_at(thread, 0))->klass());

  for (int i = 0; i < num_indexes; i++) {
    klassOop tmp = pool->klass_at(indexes[i], CHECK_0);
    KlassHandle chk_klass(thread, tmp);

    if (exc_klass() == chk_klass())
      return i;

    if (exc_klass()->klass_part()->is_subtype_of(chk_klass()))
      return i;
  }

  return -1;
}
JRT_END

JRT_ENTRY(void, SharkRuntime::monitorenter(JavaThread*      thread,
                                           BasicObjectLock* lock))
{
  if (PrintBiasedLockingStatistics)
    Atomic::inc(BiasedLocking::slow_path_entry_count_addr());

  Handle object(thread, lock->obj());  
  assert(Universe::heap()->is_in_reserved_or_null(object()), "should be");
  if (UseBiasedLocking) {
    // Retry fast entry if bias is revoked to avoid unnecessary inflation
    ObjectSynchronizer::fast_enter(object, lock->lock(), true, CHECK);
  } else {
    ObjectSynchronizer::slow_enter(object, lock->lock(), CHECK);
  }
  assert(Universe::heap()->is_in_reserved_or_null(lock->obj()), "should be");
}
JRT_END

JRT_ENTRY(void, SharkRuntime::monitorexit(JavaThread*      thread,
                                          BasicObjectLock* lock))
{
  Handle object(thread, lock->obj());  
  assert(Universe::heap()->is_in_reserved_or_null(object()), "should be");
  if (lock == NULL || object()->is_unlocked()) {
    THROW(vmSymbols::java_lang_IllegalMonitorStateException());
  }
  ObjectSynchronizer::slow_exit(object(), lock->lock(), thread);
}
JRT_END
  
JRT_ENTRY(void, SharkRuntime::new_instance(JavaThread* thread, int index))
{
  klassOop k_oop = method(thread)->constants()->klass_at(index, CHECK);
  instanceKlassHandle klass(THREAD, k_oop);
  
  // Make sure we are not instantiating an abstract klass
  klass->check_valid_for_instantiation(true, CHECK);

  // Make sure klass is initialized
  klass->initialize(CHECK);    

  // At this point the class may not be fully initialized
  // because of recursive initialization. If it is fully
  // initialized & has_finalized is not set, we rewrite
  // it into its fast version (Note: no locking is needed
  // here since this is an atomic byte write and can be
  // done more than once).
  //
  // Note: In case of classes with has_finalized we don't
  //       rewrite since that saves us an extra check in
  //       the fast version which then would call the
  //       slow version anyway (and do a call back into
  //       Java).
  //       If we have a breakpoint, then we don't rewrite
  //       because the _breakpoint bytecode would be lost.
  oop obj = klass->allocate_instance(CHECK);
  thread->set_vm_result(obj);  
}
JRT_END

JRT_ENTRY(void, SharkRuntime::newarray(JavaThread* thread,
                                       BasicType   type,
                                       int         size))
{
  oop obj = oopFactory::new_typeArray(type, size, CHECK);
  thread->set_vm_result(obj);
}
JRT_END

JRT_ENTRY(void, SharkRuntime::anewarray(JavaThread* thread,
                                        int         index,
                                        int         size))
{
  klassOop klass = method(thread)->constants()->klass_at(index, CHECK);
  objArrayOop obj = oopFactory::new_objArray(klass, size, CHECK);
  thread->set_vm_result(obj);
}
JRT_END

JRT_ENTRY(void, SharkRuntime::multianewarray(JavaThread* thread,
                                             int         index,
                                             int         ndims,
                                             int*        dims))
{
  klassOop klass = method(thread)->constants()->klass_at(index, CHECK);
  oop obj = arrayKlass::cast(klass)->multi_allocate(ndims, dims, CHECK);
  thread->set_vm_result(obj);
}
JRT_END

JRT_ENTRY(void, SharkRuntime::register_finalizer(JavaThread* thread,
                                                 oop         object))
{
  assert(object->is_oop(), "should be");
  assert(object->klass()->klass_part()->has_finalizer(), "should have");
  instanceKlass::register_finalizer(instanceOop(object), CHECK);
}
JRT_END

JRT_ENTRY(void, SharkRuntime::throw_ArrayIndexOutOfBoundsException(
                                                     JavaThread* thread,
                                                     const char* file,
                                                     int         line,
                                                     int         index))
{
  char msg[jintAsStringSize];
  snprintf(msg, sizeof(msg), "%d", index);
  Exceptions::_throw_msg(
    thread, file, line, 
    vmSymbols::java_lang_ArrayIndexOutOfBoundsException(),
    msg);
}
JRT_END

JRT_ENTRY(void, SharkRuntime::throw_NullPointerException(JavaThread* thread,
                                                         const char* file,
                                                         int         line))
{
  Exceptions::_throw_msg(
    thread, file, line, 
    vmSymbols::java_lang_NullPointerException(),
    "");
}
JRT_END

// Non-VM calls
// Nothing in these must ever GC!

void SharkRuntime::dump(const char *name, intptr_t value)
{
  oop valueOop = (oop) value;
  tty->print("%s = ", name);
  if (valueOop->is_oop(true))
    valueOop->print_on(tty);
  else if (value >= ' ' && value <= '~')
    tty->print("'%c' (%d)", value, value);
  else
    tty->print("%p", value);
  tty->print_cr("");
}

bool SharkRuntime::is_subtype_of(klassOop check_klass, klassOop object_klass)
{
  return object_klass->klass_part()->is_subtype_of(check_klass);
}

void SharkRuntime::uncommon_trap(JavaThread* thread, int trap_request)
{
  // In C2, uncommon_trap_blob creates a frame, so all the various
  // deoptimization functions expect to find the frame of the method
  // being deopted one frame down on the stack.  Create a dummy frame
  // to mirror this.
  ZeroStack *stack = thread->zero_stack();
  thread->push_zero_frame(DeoptimizerFrame::build(stack));

  // Initiate the trap
  thread->set_last_Java_frame();
  Deoptimization::UnrollBlock *urb =
    Deoptimization::uncommon_trap(thread, trap_request);
  thread->reset_last_Java_frame();

  // Pop our dummy frame and the frame being deoptimized
  thread->pop_zero_frame();
  thread->pop_zero_frame();

  // Push skeleton frames
  int number_of_frames = urb->number_of_frames();
  for (int i = 0; i < number_of_frames; i++) {
    intptr_t size = urb->frame_sizes()[i];
    thread->push_zero_frame(InterpreterFrame::build(stack, size));
  }

  // Push another dummy frame
  thread->push_zero_frame(DeoptimizerFrame::build(stack));
  
  // Fill in the skeleton frames
  thread->set_last_Java_frame();
  Deoptimization::unpack_frames(thread, Deoptimization::Unpack_uncommon_trap);
  thread->reset_last_Java_frame();

  // Pop our dummy frame
  thread->pop_zero_frame();

  // Jump into the interpreter  
#ifdef CC_INTERP
  CppInterpreter::main_loop(number_of_frames - 1, thread);
#else
  Unimplemented();
#endif // CC_INTERP
}

DeoptimizerFrame* DeoptimizerFrame::build(ZeroStack* stack)
{
  if (header_words > stack->available_words()) {
    Unimplemented();
  }

  stack->push(0); // next_frame, filled in later
  intptr_t *fp = stack->sp();
  assert(fp - stack->sp() == next_frame_off, "should be");

  stack->push(DEOPTIMIZER_FRAME);
  assert(fp - stack->sp() == frame_type_off, "should be");

  return (DeoptimizerFrame *) fp;
}
