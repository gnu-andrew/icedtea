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

class SharkBlock;
class SharkFunction;
class SharkTopLevelBlock;

class SharkState : public ResourceObj {
 public:
  SharkState(SharkBlock* block, SharkFunction* function = NULL);
  SharkState(SharkBlock* block, const SharkState* state);

 private:
  void initialize(const SharkState* state);

 private:
  SharkBlock*      _block;
  SharkFunction*   _function;
  llvm::Value*     _method;
  SharkFrameCache* _frame_cache;
  SharkValue**     _locals;
  SharkValue**     _stack;
  SharkValue**     _sp;
  int              _num_monitors;
  llvm::Value*     _oop_tmp;

 public:
  SharkBlock *block() const
  {
    return _block;
  }
  SharkFunction *function() const
  {
    return _function;
  }
  SharkFrameCache *frame_cache() const
  {
    return _frame_cache;
  }
  
 public:
  inline SharkBuilder* builder() const;
  inline int max_locals() const;
  inline int max_stack() const;
  inline int max_monitors() const;

  // Method
 public:
  llvm::Value** method_addr()
  {
    return &_method;
  }
  llvm::Value* method() const
  {
    return _method;
  }
 protected:
  void set_method(llvm::Value* method)
  {
    _method = method;
  }

  // Local variables
 public:
  SharkValue** local_addr(int index) const
  {
    assert(index >= 0 && index < max_locals(), "bad local variable index");
    return &_locals[index];
  }
  SharkValue* local(int index) const
  {
    return *local_addr(index);
  }
  void set_local(int index, SharkValue* value)
  {
    *local_addr(index) = value;
  }

  // Expression stack
 public:
  SharkValue** stack_addr(int slot) const
  {
    assert(slot >= 0 && slot < stack_depth(), "bad stack slot");
    return &_sp[-(slot + 1)];
  }
  SharkValue* stack(int slot) const
  {
    return *stack_addr(slot);
  }
 protected:
  void set_stack(int slot, SharkValue* value)
  {
    *stack_addr(slot) = value;
  }
 public:
  int stack_depth() const
  {
    return _sp - _stack;
  }
  void push(SharkValue* value)
  {
    assert(stack_depth() < max_stack(), "stack overrun");
    *(_sp++) = value;
  }
  SharkValue* pop()
  {
    assert(stack_depth() > 0, "stack underrun");
    return *(--_sp);
  }
  void pop(int slots)
  {
    assert(stack_depth() >= slots, "stack underrun");
    _sp -= slots;
  }

  // Monitors
 public:
  int num_monitors() const
  {
    return _num_monitors;
  }
  void set_num_monitors(int num_monitors)
  {
    _num_monitors = num_monitors;
  }

  // Temporary oop slot
 public:
  llvm::Value** oop_tmp_addr()
  {
    return &_oop_tmp;
  }
  llvm::Value* oop_tmp() const
  {
    return _oop_tmp;
  }
  void set_oop_tmp(llvm::Value* oop_tmp)
  {
    _oop_tmp = oop_tmp;
  }

  // Comparison
 public:
  bool equal_to(SharkState* other);

  // Copy and merge
 public:
  SharkState* copy() const
  {
    return new SharkState(block(), this);
  }
  void merge(SharkState*       other,
             llvm::BasicBlock* other_block,
             llvm::BasicBlock* this_block);

  // Cache and decache
 public:
  void decache_for_Java_call(ciMethod* callee);
  void cache_after_Java_call(ciMethod* callee);
  void decache_for_VM_call();
  void cache_after_VM_call();
  void decache_for_trap();
};

// SharkEntryState objects are used to manage the state
// that the method will be entered with.
class SharkEntryState : public SharkState {
 public:
  SharkEntryState(SharkTopLevelBlock* block, llvm::Value* method);
};

// SharkPHIState objects are used to manage the entry state
// for blocks with more than one entry path or for blocks
// entered from blocks that will be compiled later.
class SharkPHIState : public SharkState {
 public:
  SharkPHIState(SharkTopLevelBlock* block);

 public:
  void add_incoming(SharkState* incoming_state); 
};
