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

# The name of the `malloc()` binding defined in extra-ruby-bindings.h
MALLOC_FUNC = 'mkxp_sandbox_malloc'

# The name of the `free()` binding defined in extra-ruby-bindings.h
FREE_FUNC = 'mkxp_sandbox_free'

COMPLETE_FUNC = 'mkxp_sandbox_complete'

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
      for (int i = 0; i < PREV_ARG; ++i) {
          ((VALUE *)(bind.instance->w2c_memory.data + BUF))[i] = SERIALIZE_PTR(ARG[i]);
      }
    HEREDOC
  },
  'VALUE (*)()' => {
    keep: true,
    anyargs: true,
    formatter: lambda { |name| "VALUE (*#{name})(void *, ANYARGS)" },
    declaration: 'VALUE (*)(void *, ANYARGS)',
  },
  'VALUE (*)(VALUE)' => {
    keep: true,
    func_ptr_args: [:size],
    func_ptr_rets: [:size],
    formatter: lambda { |name| "VALUE (*#{name})(void *, VALUE)" },
    declaration: 'VALUE (*)(void *, VALUE)',
  },
  'VALUE (*)(VALUE,VALUE)' => {
    keep: true,
    func_ptr_args: [:size, :size],
    func_ptr_rets: [:size],
    formatter: lambda { |name| "VALUE (*#{name})(void *, VALUE, VALUE)" },
    declaration: 'VALUE (*)(void *, VALUE, VALUE)',
  },
}

RET_HANDLERS = {
  'void' => { keep: true, primitive: :void },
  'VALUE' => { keep: true, primitive: :size },
  'ID' => { keep: true, primitive: :size },
  'int' => { primitive: :s32 },
  'unsigned int' => { primitive: :u32 },
  'long' => { primitive: :size },
  'unsigned long' => { primitive: :size },
  'long long' => { primitive: :s64 },
  'unsigned long long' => { primitive: :u64 },
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
  ptr: MEMORY64 ? 'WASM_RT_I64' : 'WASM_RT_I32',
  s32: 'WASM_RT_I32',
  u32: 'WASM_RT_I32',
  s64: 'WASM_RT_I64',
  u64: 'WASM_RT_I64',
  f32: 'WASM_RT_F32',
  f64: 'WASI_RT_F64',
}

################################################################################

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

  #include <cstdint>
  #include <cstring>
  #include <memory>
  #include <vector>
  #include <boost/any.hpp>
  #include <boost/asio/coroutine.hpp>
  #include <boost/asio/yield.hpp>
  #{MODULE_INCLUDE}
  #include "src/sandbox/types.h"

  // Autogenerated by sandbox-bindgen.rb. Don't manually modify this file - modify sandbox-bindgen.rb instead!

  #define ANYARGS ...
  typedef int#{MEMORY64 ? '64' : '32'}_t wasm_ssize_t;
  typedef uint#{MEMORY64 ? '64' : '32'}_t wasm_size_t;
  typedef wasm_size_t wasm_ptr_t;
  typedef wasm_size_t VALUE;
  typedef wasm_size_t ID;

  namespace mkxp_sandbox {
      struct bindings {
          private:
          wasm_ptr_t next_func_ptr;
          std::shared_ptr<struct w2c_#{MODULE_NAME}> instance;
          size_t depth;
          std::vector<boost::any> stack;
          wasm_ptr_t sbindgen_malloc(wasm_ptr_t);
          wasm_ptr_t sbindgen_create_func_ptr();

          public:
          bindings(std::shared_ptr<struct w2c_#{MODULE_NAME}>);

          template <typename T> struct stack_frame {
              friend struct bindings;

              private:
              struct bindings &bindings;
              T &inner;
              static inline T &init(struct bindings &bindings) {
                  if (bindings.depth == bindings.stack.size()) {
                      bindings.stack.push_back(T(bindings));
                  } else if (bindings.depth > bindings.stack.size()) {
                      throw SandboxTrapException();
                  }
                  try {
                      return boost::any_cast<T &>(bindings.stack[bindings.depth++]);
                  } catch (boost::bad_any_cast &) {
                      throw SandboxTrapException();
                  }
              }
              stack_frame(struct bindings &b) : bindings(b), inner(init(b)) {}

              public:
              ~stack_frame() {
                  if (inner.is_complete()) {
                      bindings.stack.pop_back();
                  }
                  --bindings.depth;
              }
              inline T &operator()() {
                  return inner;
              }
          };

          template <typename T> struct stack_frame<T> bind() {
              return (struct stack_frame<T>)(*this);
          }

HEREDOC

HEADER_END = <<~HEREDOC

  }

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

  #if WABT_BIG_ENDIAN
  #define SERIALIZE_32(value) __builtin_bswap32(value)
  #define SERIALIZE_64(value) __builtin_bswap64(value)
  #else
  #define SERIALIZE_32(value) (value)
  #define SERIALIZE_64(value) (value)
  #endif
  #define SERIALIZE_PTR(value) SERIALIZE_#{MEMORY64 ? '64' : '32'}(value)


  using namespace mkxp_sandbox;


  bindings::bindings(std::shared_ptr<struct w2c_#{MODULE_NAME}> m) : next_func_ptr(-1), instance(m), depth(0) {}


  wasm_ptr_t bindings::sbindgen_malloc(wasm_size_t size) {
      wasm_ptr_t buf = w2c_#{MODULE_NAME}_#{MALLOC_FUNC}(instance.get(), size);

      // Verify that the entire allocated buffer is in valid memory
      wasm_ptr_t buf_end;
      if (buf == 0 || __builtin_add_overflow(buf, size, &buf_end) || buf_end >= instance->w2c_memory.size) {
          return 0;
      }

      return buf;
  }


  wasm_ptr_t bindings::sbindgen_create_func_ptr() {
      if (next_func_ptr == (wasm_ptr_t)-1) {
          next_func_ptr = instance->w2c_T0.size;
      }

      if (next_func_ptr < instance->w2c_T0.max_size) {
          return next_func_ptr++;
      }

      // Make sure that an integer overflow won't occur if we double the max size of the funcref table
      wasm_size_t new_max_size;
      if (__builtin_add_overflow(instance->w2c_T0.max_size, instance->w2c_T0.max_size, &new_max_size)) {
          return 0;
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
          return 0;
      }

      return next_func_ptr++;
  }


  //////////////////////////////////////////////////////////////////////////////
HEREDOC

################################################################################

declarations = []
coroutines = []
func_names = []

File.readlines('tags', chomp: true).each do |line|
  line = line.split("\t")

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
  args = line[3]
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
        f#{i} = bind.sbindgen_create_func_ptr();
        if (f#{i} == 0) {
      HEREDOC
      buffers.reverse_each { |buf| coroutine_initializer += "    w2c_#{MODULE_NAME}_#{FREE_FUNC}(bind.instance.get(), #{buf});\n" }
      coroutine_initializer += <<~HEREDOC
            throw SandboxOutOfMemoryException();
        }
      HEREDOC
      if handler[:anyargs]
        coroutine_initializer += <<~HEREDOC
          bind.instance->w2c_T0.data[f#{i}] = wasm_rt_funcref_t {
              .func_type = wasm2c_#{MODULE_NAME}_get_func_type(a#{args.length - 1} == -1 ? 3 : a#{args.length - 1} == -2 ? 2 : a#{args.length - 1} + 1, 1, #{([:size] * 16).map { |type| FUNC_TYPE_TABLE[type] }.join(', ')}),
              .func = (wasm_rt_function_ptr_t)a#{i},
              .func_tailcallee = {.fn = NULL},
              .module_instance = bind.instance.get(),
          };
        HEREDOC
      else
        coroutine_initializer += <<~HEREDOC
          bind.instance->w2c_T0.data[f#{i}] = wasm_rt_funcref_t {
              .func_type = wasm2c_#{MODULE_NAME}_get_func_type(#{handler[:func_ptr_args].length}, #{handler[:func_ptr_rets].length}#{handler[:func_ptr_args].empty? && handler[:func_ptr_rets].empty? ? '' : ', ' + (handler[:func_ptr_args] + handler[:func_ptr_rets]).map { |type| FUNC_TYPE_TABLE[type] }.join(', ')}),
              .func = (wasm_rt_function_ptr_t)a#{i},
              .func_tailcallee = {.fn = NULL},
              .module_instance = bind.instance.get(),
          };
        HEREDOC
      end
      coroutine_initializer += "\n"
      transformed_args.add(i)
    elsif !handler[:buf_size].nil?
      coroutine_initializer += <<~HEREDOC
        f#{i} = bind.sbindgen_malloc(#{handler[:buf_size].gsub('PREV_ARG', "a#{i - 1}").gsub('ARG', "a#{i}")});
        if (f#{i} == 0) {
      HEREDOC
      buffers.reverse_each { |buf| coroutine_initializer += "    w2c_#{MODULE_NAME}_#{FREE_FUNC}(bind.instance.get(), #{buf});\n" }
      coroutine_initializer += <<~HEREDOC
            throw SandboxOutOfMemoryException();
        }
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
        f#{args.length - 1} = bind.sbindgen_malloc(a#{args.length - 2} * sizeof(VALUE));
        if (f#{args.length - 1} == 0) {
      HEREDOC
      buffers.reverse_each { |buf| coroutine_initializer += "    w2c_#{MODULE_NAME}_#{FREE_FUNC}(bind.instance.get(), #{buf});\n" }
      coroutine_initializer += <<~HEREDOC
            throw SandboxOutOfMemoryException();
        }
        std::va_list a;
        va_start(a, a#{args.length - 2});
        for (long i = 0; i < a#{args.length - 2}; ++i) {
            ((VALUE *)(bind.instance->w2c_memory.data + f#{args.length - 1}))[i] = SERIALIZE_PTR(va_arg(a, VALUE));
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
        f#{args.length - 1} = bind.sbindgen_malloc(n * sizeof(VALUE));
        if (f#{args.length - 1} == 0) {
            va_end(a);
      HEREDOC
      buffers.reverse_each { |buf| coroutine_initializer += "    w2c_#{MODULE_NAME}_#{FREE_FUNC}(bind.instance.get(), #{buf});\n" }
      coroutine_initializer += <<~HEREDOC
            throw SandboxOutOfMemoryException();
        }
        for (wasm_size_t i = 0; i < n; ++i) {
            ((VALUE *)(bind.instance->w2c_memory.data + f#{args.length - 1}))[i] = SERIALIZE_PTR(va_arg(a, VALUE));
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
    #{handler[:primitive] == :void ? '' : 'r = '}w2c_#{MODULE_NAME}_#{func_name}(#{(['bind.instance.get()'] + (0...args.length).map { |i| args[i] == '...' || transformed_args.include?(i) ? "f#{i}" : "a#{i}" }).join(', ')});
    if (w2c_#{MODULE_NAME}_#{COMPLETE_FUNC}(bind.instance.get())) break;
    yield;
  HEREDOC

  coroutine_finalizer = (0...buffers.length).map { |i| "w2c_#{MODULE_NAME}_#{FREE_FUNC}(bind.instance.get(), #{buffers[buffers.length - 1 - i]});" }

  coroutine_definition = <<~HEREDOC
    #{func_name}::#{func_name}(bindings &bind) : bind(bind) {}
    #{coroutine_ret} #{func_name}::operator()(#{coroutine_args.join(', ')}) {#{coroutine_vars.empty? ? '' : (coroutine_vars.map { |var| "\n    #{var} = 0;" }.join + "\n")}
        reenter (this) {
    #{coroutine_initializer.empty? ? '' : (coroutine_initializer.split("\n").map { |line| "        #{line}" }.join("\n") + "\n\n")}        for (;;) {
    #{coroutine_inner.split("\n").map { |line| "            #{line}" }.join("\n")}
            }#{coroutine_finalizer.empty? ? '' : ("\n\n" + coroutine_finalizer.map { |line| "        #{line}" }.join("\n"))}
        }#{handler[:primitive] == :void ? '' : "\n\n    return r;"}
    }
  HEREDOC

  coroutine_declaration = <<~HEREDOC
    struct #{func_name} : boost::asio::coroutine {
        friend struct bindings;
        friend struct bindings::stack_frame<struct #{func_name}>;
        #{coroutine_ret} operator()(#{declaration_args.join(', ')});
        private:
        #{func_name}(bindings &bind);
        bindings &bind;
    #{fields.empty? ? '' : fields.map { |field| "    #{field};\n" }.join}};
  HEREDOC

  func_names.append(func_name)
  coroutines.append(coroutine_definition)
  declarations.append(coroutine_declaration)
end

File.open('mkxp-sandbox-bindgen.h', 'w') do |file|
  file.write(HEADER_START)
  for func_name in func_names
    file.write("        friend struct #{func_name};\n")
  end
  file.write("    };\n")
  for declaration in declarations
    file.write("\n" + declaration.split("\n").map { |line| "    #{line}" }.join("\n").rstrip)
  end
  file.write(HEADER_END)
end
File.open('mkxp-sandbox-bindgen.cpp', 'w') do |file|
  file.write(PRELUDE)
  for coroutine in coroutines
    file.write("\n\n")
    file.write(coroutine.rstrip)
  end
end
