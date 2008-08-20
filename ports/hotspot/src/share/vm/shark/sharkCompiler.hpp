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

class SharkCompiler : public AbstractCompiler {
  friend class SharkCompilation;

 public:
  // Creation
  SharkCompiler();

  // Name of this compiler
  const char *name()     { return "shark"; }

  // Missing feature tests
  bool supports_native() { return false; }
  bool supports_osr()    { return false; }

  // Customization
  bool needs_adapters()  { return false; }
  bool needs_stubs()     { return false; }

  // Initialization
  void initialize();

  // Compilation entry point for methods
  void compile_method(ciEnv* env, ciMethod* target, int entry_bci);

  // The builder we'll use to compile our functions
 private:
  SharkBuilder _builder;

 protected:
  SharkBuilder* builder()
  {
    return &_builder;
  }

  // Helpers
 private:
  void install_method(ciEnv*          env,
                      ciMethod*       target,
                      int             entry_bci,
                      CodeBuffer*     cb,
                      OopMapSet*      oopmaps);

  static const char *methodname(const ciMethod* target);
};
