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

# The name passed as the `-n`/`--module-name` flag to `wasm2c`
MODULE_NAME = 'ruby'

# The name of the `malloc()` binding defined in ruby-bindings.h
MALLOC_FUNC = 'mkxp_sandbox_malloc'

# The name of the `free()` binding defined in ruby-bindings.h
FREE_FUNC = 'mkxp_sandbox_free'

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
    serialize: "std::strcpy((char *)(*bind + BUF), ARG);\n",
  },
  'const VALUE *' => {
    keep: true,
    condition: lambda { |func_name, args, arg_index| arg_index > 0 && args[arg_index - 1] == 'int' }, # Only handle arguments of type `const VALUE *` if the previous argument is of type `int`
    buf_size: 'PREV_ARG * sizeof(VALUE)',
    serialize: <<~HEREDOC
      std::memcpy(*bind + BUF, ARG, PREV_ARG * sizeof(VALUE));
    HEREDOC
  },
  'volatile VALUE *' => {
    keep: true,
    buf_size: 'sizeof(VALUE)',
    serialize: <<~HEREDOC
      *(VALUE *)(*bind + BUF) = *ARG;
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
  ssize: 'WASM_RT_ISIZE',
  size: 'WASM_RT_ISIZE',
  value: 'WASM_RT_ISIZE',
  ptr: 'WASM_RT_ISIZE',
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

  #include "binding-sandbox/binding-base.h"

  // Autogenerated by sandbox-bindgen.rb. Don't manually modify this file - modify sandbox-bindgen.rb instead!

  #if WABT_BIG_ENDIAN
  #  define SERIALIZE_32(value) __builtin_bswap32(value)
  #  define SERIALIZE_64(value) __builtin_bswap64(value)
  #else
  #  define SERIALIZE_32(value) (value)
  #  define SERIALIZE_64(value) (value)
  #endif

  #ifdef MKXPZ_RETRO_MEMORY64
  #  define SERIALIZE_VALUE(value) SERIALIZE_64(value)
  #else
  #  define SERIALIZE_VALUE(value) SERIALIZE_32(value)
  #endif

  namespace mkxp_sandbox {
      struct bindings : binding_base {
          bindings(std::shared_ptr<struct w2c_#{MODULE_NAME}> m);

          struct rb_data_type {
              friend struct bindings;
              rb_data_type();
              wasm_ptr_t get() const;
              private:
              wasm_ptr_t ptr;
              rb_data_type(wasm_ptr_t ptr);
          };

          struct rb_data_type rb_data_type(const char *wrap_struct_name, void (*dmark)(wasm_ptr_t), void (*dfree)(wasm_ptr_t), wasm_size_t (*dsize)(wasm_ptr_t), void (*dcompact)(wasm_ptr_t), wasm_ptr_t parent, wasm_ptr_t data, wasm_size_t flags);

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
  #  define SERIALIZE_32(value) __builtin_bswap32(value)
  #  define SERIALIZE_64(value) __builtin_bswap64(value)
  #else
  #  define SERIALIZE_32(value) (value)
  #  define SERIALIZE_64(value) (value)
  #endif

  #ifdef MKXPZ_RETRO_MEMORY64
  #  define SERIALIZE_VALUE(value) SERIALIZE_64(value)
  #  define WASM_RT_ISIZE WASM_RT_I64
  #else
  #  define SERIALIZE_VALUE(value) SERIALIZE_32(value)
  #  define WASM_RT_ISIZE WASM_RT_I32
  #endif

  using namespace mkxp_sandbox;

  bindings::rb_data_type::rb_data_type() : ptr(0) {}

  bindings::rb_data_type::rb_data_type(wasm_ptr_t ptr) : ptr(ptr) {}

  wasm_ptr_t bindings::rb_data_type::get() const {
      if (ptr == 0) throw SandboxTrapException();
      return ptr;
  }

  bindings::bindings(std::shared_ptr<struct w2c_#{MODULE_NAME}> m) : binding_base(m) {}


  //////////////////////////////////////////////////////////////////////////////
HEREDOC

POSTSCRIPT = <<~HEREDOC

  struct bindings::rb_data_type bindings::rb_data_type(const char *wrap_struct_name, void (*dmark)(wasm_ptr_t), void (*dfree)(wasm_ptr_t), wasm_size_t (*dsize)(wasm_ptr_t), void (*dcompact)(wasm_ptr_t), wasm_ptr_t parent, wasm_ptr_t data, wasm_size_t flags) {
      wasm_ptr_t buf;
      wasm_ptr_t str;

      buf = sandbox_malloc(9 * sizeof(wasm_ptr_t));
      if (buf == 0) {
          throw SandboxOutOfMemoryException();
      }
      str = sandbox_malloc(std::strlen(wrap_struct_name) + 1);
      if (str == 0) {
          sandbox_free(buf);
          throw SandboxOutOfMemoryException();
      }

      std::strcpy((char *)(**this + str), wrap_struct_name);
      ((wasm_ptr_t *)(**this + buf))[0] = SERIALIZE_VALUE(str);

      ((wasm_ptr_t *)(**this + buf))[1] = dmark == NULL ? 0 : SERIALIZE_VALUE(wasm_rt_push_funcref(&instance().w2c_T0, wasm_rt_funcref_t {
          .func_type = wasm2c_#{MODULE_NAME}_get_func_type(1, 0, #{FUNC_TYPE_TABLE[:ptr]}),
          .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:void, [:value]])},
          .func_tailcallee = {.fn = NULL},
          .module_instance = (void *)dmark,
      }));

      ((wasm_ptr_t *)(**this + buf))[2] = dfree == NULL ? 0 : SERIALIZE_VALUE(wasm_rt_push_funcref(&instance().w2c_T0, wasm_rt_funcref_t {
          .func_type = wasm2c_#{MODULE_NAME}_get_func_type(1, 0, #{FUNC_TYPE_TABLE[:ptr]}),
          .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:void, [:value]])},
          .func_tailcallee = {.fn = NULL},
          .module_instance = (void *)dfree,
      }));

      ((wasm_ptr_t *)(**this + buf))[3] = dsize == NULL ? 0 : SERIALIZE_VALUE(wasm_rt_push_funcref(&instance().w2c_T0, wasm_rt_funcref_t {
          .func_type = wasm2c_#{MODULE_NAME}_get_func_type(1, 0, #{FUNC_TYPE_TABLE[:ptr]}, #{FUNC_TYPE_TABLE[:size]}),
          .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:size, [:value]])},
          .func_tailcallee = {.fn = NULL},
          .module_instance = (void *)dsize,
      }));

      ((wasm_ptr_t *)(**this + buf))[4] = dcompact == NULL ? 0 : SERIALIZE_VALUE(wasm_rt_push_funcref(&instance().w2c_T0, wasm_rt_funcref_t {
          .func_type = wasm2c_#{MODULE_NAME}_get_func_type(1, 0, #{FUNC_TYPE_TABLE[:ptr]}),
          .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:void, [:value]])},
          .func_tailcallee = {.fn = NULL},
          .module_instance = (void *)dcompact,
      }));

      ((wasm_ptr_t *)(**this + buf))[5] = 0;
      ((wasm_ptr_t *)(**this + buf))[6] = SERIALIZE_VALUE(parent);
      ((wasm_ptr_t *)(**this + buf))[7] = SERIALIZE_VALUE(data);
      ((wasm_ptr_t *)(**this + buf))[8] = SERIALIZE_VALUE(flags);

      return buf;
  }

  //////////////////////////////////////////////////////////////////////////////


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
      if handler[:anyargs]
        coroutine_initializer += <<~HEREDOC
          switch (a#{args.length - 1}) {
              case -1:
                  f#{i} = wasm_rt_push_funcref(&bind.instance().w2c_T0, wasm_rt_funcref_t {
                      .func_type = wasm2c_#{MODULE_NAME}_get_func_type(3, 1, #{FUNC_TYPE_TABLE[:s32]}, #{FUNC_TYPE_TABLE[:ptr]}, #{FUNC_TYPE_TABLE[:value]}, #{FUNC_TYPE_TABLE[:value]}),
                      .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:value, [:s32, :ptr, :value]])},
                      .func_tailcallee = {.fn = NULL},
                      .module_instance = (void *)a#{i},
                  });
                  break;
              case -2:
                  f#{i} = wasm_rt_push_funcref(&bind.instance().w2c_T0, wasm_rt_funcref_t {
                      .func_type = wasm2c_#{MODULE_NAME}_get_func_type(2, 1, #{FUNC_TYPE_TABLE[:value]}, #{FUNC_TYPE_TABLE[:value]}, #{FUNC_TYPE_TABLE[:value]}),
                      .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:value, [:value, :value]])},
                      .func_tailcallee = {.fn = NULL},
                      .module_instance = (void *)a#{i},
                  });
                  break;
        HEREDOC
        for j in 0..16
          case_str = <<~HEREDOC
            case #{j}:
                f#{i} = wasm_rt_push_funcref(&bind.instance().w2c_T0, wasm_rt_funcref_t {
                    .func_type = wasm2c_#{MODULE_NAME}_get_func_type(#{j + 1}, 1, #{([FUNC_TYPE_TABLE[:value]] * (j + 2)).join(', ')}),
                    .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:value, [:value] * (j + 1)])},
                    .func_tailcallee = {.fn = NULL},
                    .module_instance = (void *)a#{i},
                });
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
          f#{i} = wasm_rt_push_funcref(&bind.instance().w2c_T0, wasm_rt_funcref_t {
              .func_type = wasm2c_#{MODULE_NAME}_get_func_type(#{handler[:func_ptr_args].length}, #{handler[:func_ptr_rets].length}#{handler[:func_ptr_args].empty? && handler[:func_ptr_rets].empty? ? '' : ', ' + (handler[:func_ptr_args] + handler[:func_ptr_rets]).map { |type| FUNC_TYPE_TABLE[type] }.join(', ')}),
              .func = (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([handler[:func_ptr_rets].empty? ? :void : handler[:func_ptr_rets][0], handler[:func_ptr_args]])},
              .func_tailcallee = {.fn = NULL},
              .module_instance = (void *)a#{i},
          });
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
            ((VALUE *)(*bind + f#{args.length - 1}))[i] = va_arg(a, VALUE);
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
            ((VALUE *)(*bind + f#{args.length - 1}))[i] = va_arg(a, VALUE);
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
    #{handler[:primitive] == :void ? '' : 'r = '}w2c_#{MODULE_NAME}_#{func_name}(#{(['&bind.instance()'] + (0...args.length).map { |i| args[i] == '...' || transformed_args.include?(i) ? "f#{i}" : args[i] == 'VALUE' ? "SERIALIZE_VALUE(a#{i})" : args[i] == 'const rb_data_type_t *' ? "a#{i}.get()" : "a#{i}" }).join(', ')});
    if (w2c_#{MODULE_NAME}_asyncify_get_state(&bind.instance()) != 1) break;
    BOOST_ASIO_CORO_YIELD;
  HEREDOC

  coroutine_destructor = buffers.empty? ? '' : <<~HEREDOC
    #{func_name}::~#{func_name}() {
    #{(0...buffers.length).map { |i| "    if (#{buffers[buffers.length - 1 - i]} != 0) bind.sandbox_free(#{buffers[buffers.length - 1 - i]});" }.join("\n")}
    }
  HEREDOC

  coroutine_definition = <<~HEREDOC
    #{func_name}::#{func_name}(struct binding_base &b) : #{(['bind(b)'] + buffers.map { |buffer| "#{buffer}(0)" }).join(', ')} {}
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
        friend struct bindings::stack_frame_guard<struct #{func_name}>;
        BOOST_TYPE_INDEX_REGISTER_CLASS
        #{coroutine_ret} operator()(#{declaration_args.join(', ')});
        #{coroutine_destructor.empty? ? '' : "~#{func_name}();\n    "}private:
        struct binding_base &bind;
        #{func_name}(struct binding_base &b);
    #{fields.empty? ? '' : fields.map { |field| "    #{field};\n" }.join}};
  HEREDOC

  func_names.append(func_name)
  coroutines.append(coroutine_definition)
  declarations.append(coroutine_declaration)
end

File.open('mkxp-sandbox-bindgen.h', 'w') do |file|
  file.write(HEADER_START)
  for global_name in globals
    file.write("        inline VALUE #{global_name}() const noexcept { return *(VALUE *)(**this + instance().w2c_#{global_name}); }\n")
  end
  file.write("    };")
  for declaration in declarations
    file.write("\n\n" + declaration.split("\n").map { |line| "    #{line}" }.join("\n").rstrip)
  end
  file.write("\n\n")
  file.write("#if WABT_BIG_ENDIAN\n")
  file.write("#  ifdef MKXPZ_RETRO_MEMORY64\n")
  for const in consts
    file.write("#    define SANDBOX_#{const[0]} 0x#{[const[1]].pack('Q<').unpack('H*')[0]}u\n")
  end
  file.write("#  else\n")
  for const in consts
    file.write("#    define SANDBOX_#{const[0]} 0x#{[const[1]].pack('L<').unpack('H*')[0]}u\n")
  end
  file.write("#  endif\n")
  file.write("#else\n")
  for const in consts
    file.write("#  define SANDBOX_#{const[0]} 0x#{[const[1]].pack('L>').unpack('H*')[0]}u\n")
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
