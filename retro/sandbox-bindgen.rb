# sandbox-bindgen.rb
#
# This file is part of mkxp.
#
# Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
#
# mkxp is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# mkxp is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with mkxp.  If not, see <http://www.gnu.org/licenses/>.

################################################################################

# True if generating bindings for 64-bit WebAssembly, false if generating bindings for 32-bit WebAssembly
MEMORY64 = false

# The name passed as the `-n`/`--module-name` flag to `wasm2c`
MODULE_NAME = 'ruby'

# Include directive for including the header file generated by `wasm2c`
MODULE_INCLUDE = '#include <mkxp-retro-ruby.h>'

# The name of the `malloc()` binding defined in ruby-bindings.h
MALLOC_FUNC = 'mkxp_sandbox_malloc'

# The name of the `free()` binding defined in ruby-bindings.h
FREE_FUNC = 'mkxp_sandbox_free'

RTYPEDDATA_DATA_OFFSET = 'mkxp_sandbox_rtypeddata_data_offset'
RTYPEDDATA_DMARK_FUNC = 'mkxp_sandbox_rtypeddata_dmark'
RTYPEDDATA_DFREE_FUNC = 'mkxp_sandbox_rtypeddata_dfree'
RTYPEDDATA_DSIZE_FUNC = 'mkxp_sandbox_rtypeddata_dsize'
RTYPEDDATA_DCOMPACT_FUNC = 'mkxp_sandbox_rtypeddata_dcompact'

FIBER_ENTRY_POINT = 'mkxp_sandbox_fiber_entry_point'
FIBER_ARG0 = 'mkxp_sandbox_fiber_arg0'
FIBER_ARG1 = 'mkxp_sandbox_fiber_arg1'

################################################################################

IGNORED_FUNCTIONS = Set[
  'rb_class_descendants',
  'rb_close_before_exec',
]

ARG_HANDLERS = {
  'VALUE' => { keep: true, primitive: :size },
  'ID' => { keep: true, primitive: :size },
  'int' => { primitive: :s32 },
  'unsigned int' => { primitive: :u32 },
  'long' => { primitive: :size },
  'unsigned long' => { primitive: :size },
  'long long' => { primitive: :s64 },
  'unsigned long long' => { primitive: :u64 },
  'float' => { primitive: :f32 },
  'double' => { primitive: :f64 },
  'const char *' => {
    keep: true,
    buf_size: 'std::strlen(ARG) + 1',
    serialize: "std::strcpy((char *)(bind.instance->w2c_memory.data + BUF), ARG);\n",
  },
  'const VALUE *' => {
    keep: true,
    condition: lambda { |func_name, args, arg_index| arg_index > 0 && args[arg_index - 1] == 'int' }, # Only handle arguments of type `const VALUE *` if the previous argument is of type `int`
    buf_size: 'PREV_ARG * sizeof(VALUE)',
    serialize: <<~HEREDOC
      std::memcpy(bind.instance->w2c_memory.data + BUF, ARG, PREV_ARG * sizeof(VALUE));
    HEREDOC
  },
  'volatile VALUE *' => {
    keep: true,
    buf_size: 'sizeof(VALUE)',
    serialize: <<~HEREDOC
      *(VALUE *)(bind.instance->w2c_memory.data + BUF) = *ARG;
    HEREDOC
  },
  'void *' => {
    condition: lambda { |func_name, args, arg_index| args[arg_index + 1] == 'const rb_data_type_t *' }, # Only handle arguments of type `void *` if the next argument is of type `const rb_data_type_t *`
    primitive: :ptr
  },
  'const rb_data_type_t *' => {
    keep: true,
    formatter: lambda { |name| "const struct bindings::rb_data_type &#{name}" },
  },
  'VALUE (*)()' => {
    keep: true,
    anyargs: true,
    formatter: lambda { |name| "VALUE (*#{name})(ANYARGS)" },
    declaration: 'VALUE (*)(ANYARGS)',
  },
  'rb_alloc_func_t' => {
    keep: true,
    func_ptr_args: [:value],
    func_ptr_rets: [:value],
    formatter: lambda { |name| "VALUE (*#{name})(VALUE)" },
    declaration: 'VALUE (*)(VALUE)',
  },
  'VALUE (*)(VALUE)' => {
    keep: true,
    func_ptr_args: [:value],
    func_ptr_rets: [:value],
    formatter: lambda { |name| "VALUE (*#{name})(VALUE)" },
    declaration: 'VALUE (*)(VALUE)',
  },
  'VALUE (*)(VALUE,VALUE)' => {
    keep: true,
    func_ptr_args: [:value, :value],
    func_ptr_rets: [:value],
    formatter: lambda { |name| "VALUE (*#{name})(VALUE, VALUE)" },
    declaration: 'VALUE (*)(VALUE, VALUE)',
  },
}

RET_HANDLERS = {
  'void' => { keep: true, primitive: :void },
  'VALUE' => { keep: true, primitive: :size },
  'ID' => { keep: true, primitive: :size },
  'int' => { primitive: :s32 },
  'unsigned int' => { primitive: :u32 },
  'long' => { primitive: :ssize },
  'unsigned long' => { primitive: :size },
  'long long' => { primitive: :s64 },
  'unsigned long long' => { primitive: :u64 },
  'float' => { primitive: :f32 },
  'double' => { primitive: :f64 },
  'char *' => { primitive: :ptr },
  'const char *' => { primitive: :ptr },
}

VAR_TYPE_TABLE = {
  ssize: 'wasm_ssize_t',
  size: 'wasm_size_t',
  ptr: 'wasm_ptr_t',
  s32: 'int32_t',
  u32: 'uint32_t',
  s64: 'int64_t',
  u64: 'uint64_t',
  f32: 'float',
  f64: 'double',
}

FUNC_TYPE_TABLE = {
  ssize: MEMORY64 ? 'WASM_RT_I64' : 'WASM_RT_I32',
  size: MEMORY64 ? 'WASM_RT_I64' : 'WASM_RT_I32',
  value: MEMORY64 ? 'WASM_RT_I64' : 'WASM_RT_I32',
  ptr: MEMORY64 ? 'WASM_RT_I64' : 'WASM_RT_I32',
  s32: 'WASM_RT_I32',
  u32: 'WASM_RT_I32',
  s64: 'WASM_RT_I64',
  u64: 'WASM_RT_I64',
  f32: 'WASM_RT_F32',
  f64: 'WASI_RT_F64',
}

################################################################################

CALL_TYPES = [
  [:void, [:value]], # dmark, dfree, dcompact
  [:size, [:value]], # dsize
  [:value, [:s32, :ptr, :value]], # rb_define_method with argc = -1
]
for i in 0..16
  CALL_TYPES.append([:value, [:value] * (i + 1)]) # rb_define_method with argc = i
end

$call_type_hash_salt = 0

def call_type_hash(call_type)
  h = ([$call_type_hash_salt] + call_type).hash.to_s(36)
  if h.start_with?('-')
    h = h[1..]
  end
  return h
end

while CALL_TYPES.map { |call_type| call_type_hash(call_type) }.uniq.length < CALL_TYPES.length
  $call_type_hash_salt += 1
end

HEADER_START = <<~HEREDOC
  /*
  ** mkxp-sandbox-bindgen.h
  **
  ** This file is part of mkxp.
  **
  ** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
  **
  ** mkxp is free software: you can redistribute it and/or modify
  ** it under the terms of the GNU General Public License as published by
  ** the Free Software Foundation, either version 2 of the License, or
  ** (at your option) any later version.
  **
  ** mkxp is distributed in the hope that it will be useful,
  ** but WITHOUT ANY WARRANTY; without even the implied warranty of
  ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  ** GNU General Public License for more details.
  **
  ** You should have received a copy of the GNU General Public License
  ** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
  */

  #ifndef MKXP_SANDBOX_BINDGEN_H
  #define MKXP_SANDBOX_BINDGEN_H

  #include <cassert>
  #include <cstdint>
  #include <cstring>
  #include <memory>
  #include <unordered_map>
  #include <vector>
  #include <boost/container_hash/hash.hpp>
  #include <boost/type_index.hpp>
  #include <boost/asio/coroutine.hpp>
  #{MODULE_INCLUDE}
  #include "binding-sandbox/types.h"

  // Autogenerated by sandbox-bindgen.rb. Don't manually modify this file - modify sandbox-bindgen.rb instead!

  #if WABT_BIG_ENDIAN
  #define SERIALIZE_32(value) __builtin_bswap32(value)
  #define SERIALIZE_64(value) __builtin_bswap64(value)
  #else
  #define SERIALIZE_32(value) (value)
  #define SERIALIZE_64(value) (value)
  #endif
  #define SERIALIZE_VALUE(value) SERIALIZE_#{MEMORY64 ? '64' : '32'}(value)

  #define ANYARGS ...
  typedef int#{MEMORY64 ? '64' : '32'}_t wasm_ssize_t;
  typedef uint#{MEMORY64 ? '64' : '32'}_t wasm_size_t;
  typedef wasm_size_t wasm_ptr_t;
  typedef wasm_size_t VALUE;
  typedef wasm_size_t ID;

  namespace mkxp_sandbox {
      struct bindings {
          private:

          typedef std::tuple<wasm_ptr_t, wasm_ptr_t, wasm_ptr_t> key_t;

          struct stack_frame {
              struct bindings &bind;
              void (*destructor)(void *ptr);
              boost::typeindex::type_index type;
              wasm_ptr_t ptr;
              inline stack_frame(struct bindings &bind, void (*destructor)(void *ptr), boost::typeindex::type_index type, wasm_ptr_t ptr) : bind(bind), destructor(destructor), type(type), ptr(ptr) {}
              inline ~stack_frame() {
                  destructor(bind.instance->w2c_memory.data + ptr);
              }
          };

          struct fiber {
              key_t key;
              std::vector<struct stack_frame> stack;
              size_t stack_ptr;
          };

          wasm_ptr_t next_func_ptr;
          std::shared_ptr<struct w2c_#{MODULE_NAME}> instance;
          std::unordered_map<key_t, struct fiber, boost::hash<key_t>> fibers;
          wasm_ptr_t sandbox_create_func_ptr();

          public:

          inline bindings(std::shared_ptr<struct w2c_#{MODULE_NAME}> m) : next_func_ptr(-1), instance(m) {}

          inline ~bindings() {
              // Destroy all stack frames in order from top to bottom to enforce a portable, compiler-independent ordering of stack frame destruction
              // If we let the compiler use its default destructor, the stack frames may not be deallocated in a particular order, which can lead to hard-to-detect bugs if somehow a bug depends on the order in which the stack frames are deallocated
              for (auto &it : fibers) {
                  while (!it.second.stack.empty()) {
                      it.second.stack.pop_back();
                  }
              }
          }

          wasm_ptr_t sandbox_malloc(wasm_size_t);

          inline void sandbox_free(wasm_ptr_t ptr) { w2c_#{MODULE_NAME}_#{FREE_FUNC}(instance.get(), ptr); }

          inline uint8_t *get() const noexcept { return instance->w2c_memory.data; }

          inline uint8_t *operator*() const noexcept { return get(); }

          inline wasm_ptr_t rtypeddata_data(VALUE obj) const noexcept { return SERIALIZE_VALUE(obj) + *(wasm_size_t *)(instance->w2c_memory.data + instance->w2c_#{RTYPEDDATA_DATA_OFFSET}); }

          inline void rtypeddata_dmark(wasm_ptr_t data, wasm_ptr_t ptr) { w2c_#{MODULE_NAME}_#{RTYPEDDATA_DMARK_FUNC}(instance.get(), data, ptr); }

          inline void rtypeddata_dfree(wasm_ptr_t data, wasm_ptr_t ptr) { w2c_#{MODULE_NAME}_#{RTYPEDDATA_DFREE_FUNC}(instance.get(), data, ptr); }

          inline wasm_size_t rtypeddata_dsize(wasm_ptr_t data, wasm_ptr_t ptr) { return w2c_#{MODULE_NAME}_#{RTYPEDDATA_DSIZE_FUNC}(instance.get(), data, ptr); }

          inline void rtypeddata_dcompact(wasm_ptr_t data, wasm_ptr_t ptr) { w2c_#{MODULE_NAME}_#{RTYPEDDATA_DCOMPACT_FUNC}(instance.get(), data, ptr); }

          struct rb_data_type {
              friend struct bindings;
              inline rb_data_type() : ptr(0) {}
              inline wasm_ptr_t get() const {
                  if (ptr == 0) throw SandboxTrapException();
                  return ptr;
              }
              private:
              wasm_ptr_t ptr;
              inline rb_data_type(wasm_ptr_t ptr) : ptr(ptr) {}
          };

          struct rb_data_type rb_data_type(const char *wrap_struct_name, void (*dmark)(wasm_ptr_t), void (*dfree)(wasm_ptr_t), wasm_size_t (*dsize)(wasm_ptr_t), void (*dcompact)(wasm_ptr_t), wasm_ptr_t parent, wasm_ptr_t data, wasm_size_t flags);

          template <typename T> struct stack_frame_guard {
              friend struct bindings;

              private:

              struct bindings &bind;
              struct fiber &fiber;
              wasm_ptr_t ptr;

              static void stack_frame_destructor(void *ptr) {
                  ((T *)ptr)->~T();
              }

              static inline struct fiber &init_fiber(struct bindings &bind) {
                  key_t key = {
                      *(wasm_ptr_t *)(bind.instance->w2c_memory.data + bind.instance->w2c_#{FIBER_ENTRY_POINT}),
                      *(wasm_ptr_t *)(bind.instance->w2c_memory.data + bind.instance->w2c_#{FIBER_ARG0}),
                      *(wasm_ptr_t *)(bind.instance->w2c_memory.data + bind.instance->w2c_#{FIBER_ARG1}),
                  };
                  if (bind.fibers.count(key) == 0) {
                      bind.fibers[key] = (struct fiber){.key = key};
                  }
                  return bind.fibers[key];
              }

              static wasm_ptr_t init_inner(struct bindings &bind, struct fiber &fiber) {
                  wasm_ptr_t sp = w2c_#{MODULE_NAME}_rb_wasm_get_stack_pointer(bind.instance.get());

                  if (fiber.stack_ptr == fiber.stack.size()) {
                      fiber.stack.emplace_back(
                          bind,
                          stack_frame_destructor,
                          boost::typeindex::type_id<T>(),
                          (sp -= sizeof(T))
                      );
                      assert(sp % sizeof(VALUE) == 0);
                      new(bind.instance->w2c_memory.data + sp) T(bind);
                  } else if (fiber.stack_ptr > fiber.stack.size()) {
                      throw SandboxTrapException();
                  }

                  if (fiber.stack[fiber.stack_ptr].type == boost::typeindex::type_id<T>()) {
                      w2c_#{MODULE_NAME}_rb_wasm_set_stack_pointer(bind.instance.get(), sp);
                      return fiber.stack[fiber.stack_ptr++].ptr;
                  } else {
                      while (fiber.stack.size() > fiber.stack_ptr) {
                          fiber.stack.pop_back();
                      }
                      ++fiber.stack_ptr;
                      fiber.stack.emplace_back(
                          bind,
                          stack_frame_destructor,
                          boost::typeindex::type_id<T>(),
                          (sp -= sizeof(T))
                      );
                      assert(sp % sizeof(VALUE) == 0);
                      new(bind.instance->w2c_memory.data + sp) T(bind);
                      w2c_#{MODULE_NAME}_rb_wasm_set_stack_pointer(bind.instance.get(), sp);
                      return sp;
                  }
              }

              stack_frame_guard(struct bindings &b) : bind(b), fiber(init_fiber(b)), ptr(init_inner(b, fiber)) {}

              public:

              ~stack_frame_guard() {
                  if (get()->is_complete()) {
                      while (fiber.stack.size() > fiber.stack_ptr) {
                          fiber.stack.pop_back();
                      }

                      // Check for stack corruptions
                      assert(fiber.stack.size() == fiber.stack_ptr);
                      assert(fiber.stack.back().type == boost::typeindex::type_id<T>());

                      w2c_#{MODULE_NAME}_rb_wasm_set_stack_pointer(bind.instance.get(), fiber.stack.back().ptr + sizeof(T));
                      fiber.stack.pop_back();
                  }

                  --fiber.stack_ptr;

                  if (fiber.stack.empty()) {
                      bind.fibers.erase(fiber.key);
                  }
              }

              inline T *get() const noexcept {
                  return (T *)(bind.instance->w2c_memory.data + ptr);
              }

              inline T &operator()() const noexcept {
                  return *get();
              }
          };

          template <typename T> struct stack_frame_guard<T> bind() {
              return *this;
          }

HEREDOC

HEADER_END = <<~HEREDOC
  }

  #undef SERIALIZE_32
  #undef SERIALIZE_64
  #undef SERIALIZE_VALUE

  #endif // MKXP_SANDBOX_BINDGEN_H
HEREDOC

PRELUDE = <<~HEREDOC
  /*
  ** mkxp-sandbox-bindgen.cpp
  **
  ** This file is part of mkxp.
  **
  ** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
  **
  ** mkxp is free software: you can redistribute it and/or modify
  ** it under the terms of the GNU General Public License as published by
  ** the Free Software Foundation, either version 2 of the License, or
  ** (at your option) any later version.
  **
  ** mkxp is distributed in the hope that it will be useful,
  ** but WITHOUT ANY WARRANTY; without even the implied warranty of
  ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  ** GNU General Public License for more details.
  **
  ** You should have received a copy of the GNU General Public License
  ** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
  */

  // Autogenerated by sandbox-bindgen.rb. Don't manually modify this file - modify sandbox-bindgen.rb instead!

  #include <cstdarg>
  #include "mkxp-sandbox-bindgen.h"

  static_assert(alignof(VALUE) % sizeof(VALUE) == 0, "Alignment of `VALUE` must be divisible by size of `VALUE` for Ruby garbage collection to work. If you compiled Ruby for wasm64, try compiling it for wasm32 instead.");

  #if WABT_BIG_ENDIAN
  #define SERIALIZE_32(value) __builtin_bswap32(value)
  #define SERIALIZE_64(value) __builtin_bswap64(value)
  #else
  #define SERIALIZE_32(value) (value)
  #define SERIALIZE_64(value) (value)
  #endif
  #define SERIALIZE_VALUE(value) SERIALIZE_#{MEMORY64 ? '64' : '32'}(value)


  using namespace mkxp_sandbox;


  wasm_ptr_t bindings::sandbox_malloc(wasm_size_t size) {
      wasm_ptr_t buf = w2c_#{MODULE_NAME}_#{MALLOC_FUNC}(instance.get(), size);

      // Verify that the entire allocated buffer is in valid memory
      wasm_ptr_t buf_end;
      if (buf == 0 || __builtin_add_overflow(buf, size, &buf_end) || buf_end >= instance->w2c_memory.size) {
          return 0;
      }

      return buf;
  }


  wasm_ptr_t bindings::sandbox_create_func_ptr() {
      if (next_func_ptr == (wasm_ptr_t)-1) {
          next_func_ptr = instance->w2c_T0.size;
      }

      if (next_func_ptr < instance->w2c_T0.max_size) {
          return next_func_ptr++;
      }

      // Make sure that an integer overflow won't occur if we double the max size of the funcref table
      wasm_size_t new_max_size;
      if (__builtin_add_overflow(instance->w2c_T0.max_size, instance->w2c_T0.max_size, &new_max_size)) {
          return -1;
      }

      // Double the max size of the funcref table
      wasm_size_t old_max_size = instance->w2c_T0.max_size;
      instance->w2c_T0.max_size = new_max_size;

      // Double the size of the funcref table buffer
      if (wasm_rt_grow_funcref_table(&instance->w2c_T0, old_max_size, wasm_rt_funcref_t {
          .func_type = wasm2c_ruby_get_func_type(0, 0),
          .func = NULL,
          .func_tailcallee = {.fn = NULL},
          .module_instance = instance.get(),
      }) != old_max_size) {
          instance->w2c_T0.max_size = old_max_size;
          return -1;
      }

      return next_func_ptr++;
  }


  //////////////////////////////////////////////////////////////////////////////
HEREDOC

POSTSCRIPT = <<~HEREDOC


  //////////////////////////////////////////////////////////////////////////////


  struct bindings::rb_data_type bindings::rb_data_type(const char *wrap_struct_name, void (*dmark)(wasm_ptr_t), void (*dfree)(wasm_ptr_t), wasm_size_t (*dsize)(wasm_ptr_t), void (*dcompact)(wasm_ptr_t), wasm_ptr_t parent, wasm_ptr_t data, wasm_size_t flags) {
      wasm_ptr_t ptrs[6] = {0};
      bool oom = false;

      ptrs[0] = sandbox_malloc(9 * sizeof(wasm_ptr_t));
      ptrs[1] = sandbox_malloc(std::strlen(wrap_struct_name) + 1);
      for (size_t i = 0; i < 2; ++i) {
          if (ptrs[i] == 0) oom = true;
      }

      for (size_t i = 2; i < 6; ++i) {
          if ((i == 2 && dmark == NULL) || (i == 3 && dfree == NULL) || (i == 4 && dsize == NULL) || (i == 5 && dcompact == NULL)) continue;
          ptrs[i] = sandbox_create_func_ptr();
          if (ptrs[i] == (wasm_ptr_t)-1) oom = true;
      }

      if (oom) {
          for (size_t i = 0; i < 2; ++i) {
              if (ptrs[i] != 0) sandbox_free(ptrs[i]);
          }
          throw SandboxOutOfMemoryException();
      }

      std::strcpy((char *)(instance->w2c_memory.data + ptrs[1]), wrap_struct_name);

      if (dmark != NULL) {
          instance->w2c_T0.data[ptrs[2]] = wasm_rt_funcref_t {
              .func_type = wasm2c_#{MODULE_NAME}_get_func_type(1, 0, #{FUNC_TYPE_TABLE[:ptr]}),
              .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:void, [:value]])},
              .func_tailcallee = {.fn = NULL},
              .module_instance = (void *)dmark,
          };
      }

      if (dfree != NULL) {
          instance->w2c_T0.data[ptrs[3]] = wasm_rt_funcref_t {
              .func_type = wasm2c_#{MODULE_NAME}_get_func_type(1, 0, #{FUNC_TYPE_TABLE[:ptr]}),
              .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:void, [:value]])},
              .func_tailcallee = {.fn = NULL},
              .module_instance = (void *)dfree,
          };
      }

      if (dsize != NULL) {
          instance->w2c_T0.data[ptrs[4]] = wasm_rt_funcref_t {
              .func_type = wasm2c_#{MODULE_NAME}_get_func_type(1, 1, #{FUNC_TYPE_TABLE[:ptr]}, #{FUNC_TYPE_TABLE[:size]}),
              .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:size, [:value]])},
              .func_tailcallee = {.fn = NULL},
              .module_instance = (void *)dsize,
          };
      }

      if (dcompact != NULL) {
          instance->w2c_T0.data[ptrs[5]] = wasm_rt_funcref_t {
              .func_type = wasm2c_#{MODULE_NAME}_get_func_type(1, 0, #{FUNC_TYPE_TABLE[:ptr]}),
              .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:void, [:value]])},
              .func_tailcallee = {.fn = NULL},
              .module_instance = (void *)dcompact,
          };
      }

      for (size_t i = 1; i < 6; ++i) {
          ((wasm_ptr_t *)(instance->w2c_memory.data + ptrs[0]))[i - 1] = SERIALIZE_VALUE(ptrs[i]);
      }
      ((wasm_ptr_t *)(instance->w2c_memory.data + ptrs[0]))[5] = 0;
      ((wasm_ptr_t *)(instance->w2c_memory.data + ptrs[0]))[6] = SERIALIZE_VALUE(parent);
      ((wasm_ptr_t *)(instance->w2c_memory.data + ptrs[0]))[7] = SERIALIZE_VALUE(data);
      ((wasm_ptr_t *)(instance->w2c_memory.data + ptrs[0]))[8] = SERIALIZE_VALUE(flags);

      return ptrs[0];
  }
HEREDOC

################################################################################

declarations = []
coroutines = []
call_bindings = []
func_names = []
globals = []
consts = []

for call_type in CALL_TYPES
  call_bindings.append(
    <<~HEREDOC
      static #{call_type[0] == :void ? 'void' : call_type[0] == :value ? 'VALUE' : VAR_TYPE_TABLE[call_type[0]]} _sbindgen_call_#{call_type_hash(call_type)}(#{(["#{call_type[0] == :void ? 'void' : call_type[0] == :value ? 'VALUE' : VAR_TYPE_TABLE[call_type[0]]} (*func)(#{(0...call_type[1].length).map { |i| call_type[1][i] == :value ? 'VALUE' : VAR_TYPE_TABLE[call_type[1][i]] }.join(', ')})"] + (0...call_type[1].length).map { |i| "#{call_type[1][i] == :value ? 'VALUE' : VAR_TYPE_TABLE[call_type[1][i]]} a#{i}" }).join(', ')}) {
          #{call_type[0] == :void ? '' : 'return '}#{call_type[0] != :value ? '' : 'SERIALIZE_VALUE('}func(#{(0...call_type[1].length).map { |i| call_type[1][i] == :value ? "SERIALIZE_VALUE(a#{i})" : "a#{i}" }.join(', ')})#{call_type[0] != :value ? '' : ')'};
      }
    HEREDOC
  )
end

File.readlines('tags', chomp: true).each do |line|
  line = line.split("\t")
  next unless line[3] == 'e'

  const_name = line[0]
  next unless const_name.match?(/^RUBY_Q[a-z]/)
  const_name = const_name[6..]

  signature = line[2].match(/(?<==) *(?:(?:[1-9][0-9]*)|(?:0x[0-9a-f]+))(?=[,;]?\$\/)/)
  next if signature.nil?

  consts.append([const_name.upcase, signature[0].strip.to_i(signature[0].strip.start_with?('0x') ? 16 : 10)])
end

File.readlines('tags', chomp: true).each do |line|
  line = line.split("\t")
  next unless line[3] == 'x'

  global_name = line[0]
  next unless global_name.match?(/^rb_[a-z][A-Z]/)

  signature = line[2]
  next unless signature.start_with?('/^extern VALUE ')

  globals.append(global_name)
end

File.readlines('tags', chomp: true).each do |line|
  line = line.split("\t")
  next unless line[3] == 'p'

  func_name = line[0]
  next unless func_name.start_with?('rb_')
  next if func_name.end_with?('_static')
  next if IGNORED_FUNCTIONS.include?(func_name)

  # Only bind functions whose return type matches one of the return types we have a handler for
  ret = line[2]
  next unless ret.start_with?('/^') && ret.include?('(')
  ret = ret[2..].partition('(')[0].strip
  next unless ret.include?(' ') && ret.rpartition(' ')[2].end_with?(func_name)
  ret = ret[...-func_name.length].strip
  next unless RET_HANDLERS.include?(ret)

  # Only bind functions whose arguments all match a return type we have a handler for
  args = line[4]
  next unless args.start_with?('signature:(') && args.end_with?(')')
  args = args[11...-1]
  args = args.gsub('VALUE,VALUE', '$').split(',').map { |arg| arg.gsub('$', 'VALUE,VALUE') }.map { |arg| arg == '...' ? '...' : arg.match?(/\(\* \w+\)/) ? arg.gsub(/\(\* \w+\)/, '(*)') : arg.rpartition(' ')[0].strip }
  next unless (0...args.length).all? { |i| args[i] == '...' || (ARG_HANDLERS.include?(args[i]) && (ARG_HANDLERS[args[i]][:condition].nil? || ARG_HANDLERS[args[i]][:condition].call(func_name, args, i))) }

  coroutine_initializer = ''
  destructor = []
  transformed_args = Set[]
  buffers = []
  i = 0
  args.each_with_index do |arg, i|
    next if arg == '...'
    handler = ARG_HANDLERS[arg]

    # Generate bindings for converting the arguments
    if !handler[:func_ptr_args].nil? || handler[:anyargs]
      coroutine_initializer += <<~HEREDOC
        f#{i} = bind.sandbox_create_func_ptr();
        if (f#{i} == (wasm_ptr_t)-1) throw SandboxOutOfMemoryException();
      HEREDOC
      if handler[:anyargs]
        coroutine_initializer += <<~HEREDOC
          switch (a#{args.length - 1}) {
              case -1:
                  bind.instance->w2c_T0.data[f#{i}] = wasm_rt_funcref_t {
                      .func_type = wasm2c_#{MODULE_NAME}_get_func_type(3, 1, #{FUNC_TYPE_TABLE[:s32]}, #{FUNC_TYPE_TABLE[:ptr]}, #{FUNC_TYPE_TABLE[:value]}, #{FUNC_TYPE_TABLE[:value]}),
                      .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:value, [:s32, :ptr, :value]])},
                      .func_tailcallee = {.fn = NULL},
                      .module_instance = (void *)a#{i},
                  };
                  break;
              case -2:
                  bind.instance->w2c_T0.data[f#{i}] = wasm_rt_funcref_t {
                      .func_type = wasm2c_#{MODULE_NAME}_get_func_type(2, 1, #{FUNC_TYPE_TABLE[:value]}, #{FUNC_TYPE_TABLE[:value]}, #{FUNC_TYPE_TABLE[:value]}),
                      .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:value, [:value, :value]])},
                      .func_tailcallee = {.fn = NULL},
                      .module_instance = (void *)a#{i},
                  };
                  break;
        HEREDOC
        for j in 0..16
          case_str = <<~HEREDOC
            case #{j}:
                bind.instance->w2c_T0.data[f#{i}] = wasm_rt_funcref_t {
                    .func_type = wasm2c_#{MODULE_NAME}_get_func_type(#{j + 1}, 1, #{([FUNC_TYPE_TABLE[:value]] * (j + 2)).join(', ')}),
                    .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:value, [:value] * (j + 1)])},
                    .func_tailcallee = {.fn = NULL},
                    .module_instance = (void *)a#{i},
                };
                break;
          HEREDOC
          coroutine_initializer += case_str.split("\n").map { |line| "    #{line}".rstrip }.join("\n") + "\n"
        end
        coroutine_initializer += <<~HEREDOC
              default:
                  throw SandboxTrapException();
          }
        HEREDOC
      else
        coroutine_initializer += <<~HEREDOC
          bind.instance->w2c_T0.data[f#{i}] = wasm_rt_funcref_t {
              .func_type = wasm2c_#{MODULE_NAME}_get_func_type(#{handler[:func_ptr_args].length}, #{handler[:func_ptr_rets].length}#{handler[:func_ptr_args].empty? && handler[:func_ptr_rets].empty? ? '' : ', ' + (handler[:func_ptr_args] + handler[:func_ptr_rets]).map { |type| FUNC_TYPE_TABLE[type] }.join(', ')}),
              .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([handler[:func_ptr_rets].empty? ? :void : handler[:func_ptr_rets][0], handler[:func_ptr_args]])},
              .func_tailcallee = {.fn = NULL},
              .module_instance = (void *)a#{i},
          };
        HEREDOC
      end
      coroutine_initializer += "\n"
      transformed_args.add(i)
    elsif !handler[:buf_size].nil?
      coroutine_initializer += <<~HEREDOC
        f#{i} = bind.sandbox_malloc(#{handler[:buf_size].gsub('PREV_ARG', "a#{i - 1}").gsub('ARG', "a#{i}")});
        if (f#{i} == 0) throw SandboxOutOfMemoryException();
      HEREDOC
      coroutine_initializer += handler[:serialize].gsub('PREV_ARG', "a#{i - 1}").gsub('ARG', "a#{i}").gsub('BUF', "f#{i}")
      coroutine_initializer += "\n"
      transformed_args.add(i)
      buffers.append("f#{i}")
    end

    i += 1
  end

  coroutine_vars = []

  # If this is a varargs function, manually generate bindings for getting the varargs based on the function name
  if !args.empty? && args[-1] == '...'
    case func_name
    when 'rb_funcall'
      coroutine_initializer += <<~HEREDOC
        f#{args.length - 1} = bind.sandbox_malloc(a#{args.length - 2} * sizeof(VALUE));
        if (f#{args.length - 1} == 0) throw SandboxOutOfMemoryException();
        std::va_list a;
        va_start(a, a#{args.length - 2});
        for (long i = 0; i < a#{args.length - 2}; ++i) {
            ((VALUE *)(bind.instance->w2c_memory.data + f#{args.length - 1}))[i] = va_arg(a, VALUE);
        }
        va_end(a);
      HEREDOC
      coroutine_initializer += "\n"
      buffers.append("f#{args.length - 1}")
    when 'rb_rescue2'
      coroutine_vars.append('wasm_size_t n')
      coroutine_initializer += <<~HEREDOC
        std::va_list a, b;
        va_start(a, a#{args.length - 2});
        va_copy(b, a);
        n = 0;
        do ++n; while (va_arg(b, VALUE));
        va_end(b);
        f#{args.length - 1} = bind.sandbox_malloc(n * sizeof(VALUE));
        if (f#{args.length - 1} == 0) {
            va_end(a);
            throw SandboxOutOfMemoryException();
        }
        for (wasm_size_t i = 0; i < n; ++i) {
            ((VALUE *)(bind.instance->w2c_memory.data + f#{args.length - 1}))[i] = va_arg(a, VALUE);
        }
      HEREDOC
      coroutine_initializer += "\n"
      buffers.append("f#{args.length - 1}")
    else
      next
    end
  end

  handler = RET_HANDLERS[ret]

  fields = (0...args.length).filter_map do |i|
    (args[i] == '...' || transformed_args.include?(i)) && "wasm_ptr_t f#{i}"
  end

  coroutine_ret = !RET_HANDLERS[ret][:keep] ? VAR_TYPE_TABLE[RET_HANDLERS[ret][:primitive]] : ret;

  coroutine_vars.append("#{coroutine_ret} r") if handler[:primitive] != :void

  coroutine_args = (0...args.length).map do |i|
    args[i] == '...' ? '...'
      : !ARG_HANDLERS[args[i]][:formatter].nil? ? ARG_HANDLERS[args[i]][:formatter].call("a#{i}")
      : !ARG_HANDLERS[args[i]][:keep] ? "#{VAR_TYPE_TABLE[ARG_HANDLERS[args[i]][:primitive]]} a#{i}"
      : "#{args[i]} a#{i}"
  end

  declaration_args = (0...args.length).map do |i|
    args[i] == '...' ? '...'
      : !ARG_HANDLERS[args[i]][:formatter].nil? ? ARG_HANDLERS[args[i]][:formatter].call('')
      : !ARG_HANDLERS[args[i]][:keep] ? "#{VAR_TYPE_TABLE[ARG_HANDLERS[args[i]][:primitive]]}"
      : "#{args[i]}"
  end

  coroutine_inner = <<~HEREDOC
    #{handler[:primitive] == :void ? '' : 'r = '}w2c_#{MODULE_NAME}_#{func_name}(#{(['bind.instance.get()'] + (0...args.length).map { |i| args[i] == '...' || transformed_args.include?(i) ? "f#{i}" : args[i] == 'VALUE' ? "SERIALIZE_VALUE(a#{i})" : args[i] == 'const rb_data_type_t *' ? "a#{i}.get()" : "a#{i}" }).join(', ')});
    if (w2c_#{MODULE_NAME}_asyncify_get_state(bind.instance.get()) != 1) break;
    BOOST_ASIO_CORO_YIELD;
  HEREDOC

  coroutine_destructor = buffers.empty? ? '' : <<~HEREDOC
    #{func_name}::~#{func_name}() {
    #{(0...buffers.length).map { |i| "    if (#{buffers[buffers.length - 1 - i]} != 0) bind.sandbox_free(#{buffers[buffers.length - 1 - i]});" }.join("\n")}
    }
  HEREDOC

  coroutine_definition = <<~HEREDOC
    #{coroutine_ret} #{func_name}::operator()(#{coroutine_args.join(', ')}) {#{coroutine_vars.empty? ? '' : (coroutine_vars.map { |var| "\n    #{var} = 0;" }.join + "\n")}
        BOOST_ASIO_CORO_REENTER (this) {
    #{coroutine_initializer.empty? ? '' : (coroutine_initializer.split("\n").map { |line| "        #{line}".rstrip }.join("\n") + "\n\n")}        for (;;) {
    #{coroutine_inner.split("\n").map { |line| "            #{line}" }.join("\n")}
            }
        }#{handler[:primitive] == :void ? '' : ret == 'VALUE' ? "\n\n    return SERIALIZE_VALUE(r);" : "\n\n    return r;"}
    }#{coroutine_destructor.empty? ? '' : ("\n" + coroutine_destructor)}
  HEREDOC

  coroutine_declaration = <<~HEREDOC
    struct #{func_name} : boost::asio::coroutine {
        friend struct bindings;
        friend struct bindings::stack_frame_guard<struct #{func_name}>;
        BOOST_TYPE_INDEX_REGISTER_CLASS
        #{coroutine_ret} operator()(#{declaration_args.join(', ')});
        #{coroutine_destructor.empty? ? '' : "~#{func_name}();\n    "}private:
        struct bindings &bind;
        inline #{func_name}(struct bindings &b) : #{(['bind(b)'] + buffers.map { |buffer| "#{buffer}(0)" }).join(', ')} {}
    #{fields.empty? ? '' : fields.map { |field| "    #{field};\n" }.join}};
  HEREDOC

  func_names.append(func_name)
  coroutines.append(coroutine_definition)
  declarations.append(coroutine_declaration)
end

File.open('mkxp-sandbox-bindgen.h', 'w') do |file|
  file.write(HEADER_START)
  for global_name in globals
    file.write("        inline VALUE #{global_name}() const noexcept { return *(VALUE *)(instance->w2c_memory.data + instance->w2c_#{global_name}); }\n")
  end
  for func_name in func_names
    file.write("        friend struct #{func_name};\n")
  end
  file.write("    };")
  for declaration in declarations
    file.write("\n\n" + declaration.split("\n").map { |line| "    #{line}" }.join("\n").rstrip)
  end
  file.write("\n\n")
  file.write("#if WABT_BIG_ENDIAN\n")
  for const in consts
    file.write("#define SANDBOX_#{const[0]} 0x#{[const[1]].pack(MEMORY64 ? 'Q<' : 'L<').unpack('H*')[0]}u\n")
  end
  file.write("#else\n")
  for const in consts
    file.write("#define SANDBOX_#{const[0]} 0x#{[const[1]].pack(MEMORY64 ? 'Q>' : 'L>').unpack('H*')[0]}u\n")
  end
  file.write("#endif\n")
  file.write(HEADER_END)
end
File.open('mkxp-sandbox-bindgen.cpp', 'w') do |file|
  file.write(PRELUDE)
  for call_binding in call_bindings
    file.write("\n\n")
    file.write(call_binding.rstrip + "\n")
  end
  for coroutine in coroutines
    file.write("\n\n")
    file.write(coroutine.rstrip + "\n")
  end
  file.write(POSTSCRIPT)
end
