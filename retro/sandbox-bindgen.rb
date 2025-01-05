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

# The name of the function defined in extra-ruby-bindings.h that yields to Ruby's asynchronous runtime
YIELD_FUNC = 'mkxp_sandbox_yield'

################################################################################

IGNORED_FUNCTIONS = Set[
  'rb_class_descendants',
  'rb_close_before_exec',
]

ARG_HANDLERS = {
  'VALUE' => { keep: true, primitive: :ptr },
  'ID' => { keep: true, primitive: :ptr },
  'int' => { primitive: :s32 },
  'unsigned int' => { primitive: :u32 },
  'const char *' => {
    keep: true,
    buf_size: 'std::strlen(ARG) + 1',
    serialize: "std::strcpy((char *)(module_instance->w2c_memory.data + BUF), ARG);\n",
  },
  'const VALUE *' => {
    keep: true,
    condition: lambda { |func_name, args, arg_index| arg_index > 0 && args[arg_index - 1] == 'int' }, # Only handle arguments of type `const VALUE *` if the previous argument is of type `int`
    buf_size: 'PREV_ARG * sizeof(wasm_ptr_t)',
    serialize: <<~HEREDOC
      for (int i = 0; i < PREV_ARG; ++i) {
          ((wasm_ptr_t *)module_instance->w2c_memory.data + BUF)[i] = SERIALIZE_PTR(ARG[i]);
      }
    HEREDOC
  },
  'VALUE (*)(ANYARGS)' => {
    keep: true,
    anyargs: true,
    formatter: lambda { |name| "VALUE (*#{name})(void *, ANYARGS)" },
    declaration: 'VALUE (*)(void *, ANYARGS)',
  },
  'VALUE (*)(VALUE)' => {
    keep: true,
    func_ptr_args: [:ptr],
    func_ptr_rets: [:ptr],
    formatter: lambda { |name| "VALUE (*#{name})(void *, VALUE)" },
    declaration: 'VALUE (*)(void *, VALUE)',
  },
  'VALUE (*)(VALUE,VALUE)' => {
    keep: true,
    func_ptr_args: [:ptr, :ptr],
    func_ptr_rets: [:ptr],
    formatter: lambda { |name| "VALUE (*#{name})(void *, VALUE, VALUE)" },
    declaration: 'VALUE (*)(void *, VALUE, VALUE)',
  },
}

RET_HANDLERS = {
  'void' => { keep: true, primitive: :void },
  'VALUE' => { keep: true, primitive: :ptr },
  'ID' => { keep: true, primitive: :ptr },
  'int' => { primitive: :s32 },
  'unsigned int' => { primitive: :u32 },
}

VAR_TYPE_TABLE = {
  ptr: 'wasm_ptr_t',
  s32: 'int32_t',
  u32: 'uint32_t',
  s64: 'int64_t',
  u64: 'uint64_t',
  f32: 'float',
  f64: 'double',
}

FUNC_TYPE_TABLE = {
  ptr: 'WASM_RT_I32',
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
  #{MODULE_INCLUDE}
  #include "src/sandbox/types.h"

  // Autogenerated by sandbox-bindgen.rb. Don't manually modify this file - modify sandbox-bindgen.rb instead!

  #define ANYARGS ...
  typedef uint#{MEMORY64 ? '64' : '32'}_t wasm_ptr_t;
  typedef wasm_ptr_t VALUE;
  typedef wasm_ptr_t ID;

  struct SandboxBind {
      private:
      wasm_ptr_t next_func_ptr;
      std::shared_ptr<struct w2c_#{MODULE_NAME}> module_instance;
      wasm_ptr_t _sbindgen_malloc(wasm_ptr_t);
      wasm_ptr_t _sbindgen_create_func_ptr();

      public:
      SandboxBind(std::shared_ptr<struct w2c_#{MODULE_NAME}>);

HEREDOC

HEADER_END = <<~HEREDOC
  };

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
  #endif // WABT_BIG_ENDIAN
  #define SERIALIZE_PTR(value) SERIALIZE_#{MEMORY64 ? '64' : '32'}(value)


  SandboxBind::SandboxBind(std::shared_ptr<struct w2c_#{MODULE_NAME}> m) : next_func_ptr(m->w2c_T0.size), module_instance(m) {}


  wasm_ptr_t SandboxBind::_sbindgen_malloc(wasm_ptr_t size) {
      wasm_ptr_t buf = w2c_#{MODULE_NAME}_#{MALLOC_FUNC}(module_instance.get(), size);

      // Verify that the entire allocated buffer is in valid memory
      wasm_ptr_t buf_end;
      if (buf == 0 || __builtin_add_overflow(buf, size, &buf_end) || buf_end >= module_instance->w2c_memory.size) {
          return 0;
      }

      return buf;
  }


  wasm_ptr_t SandboxBind::_sbindgen_create_func_ptr() {
      if (next_func_ptr < module_instance->w2c_T0.max_size) {
          return next_func_ptr++;
      }

      // Make sure that an integer overflow won't occur if we double the max size of the funcref table
      wasm_ptr_t new_max_size;
      if (__builtin_add_overflow(module_instance->w2c_T0.max_size, module_instance->w2c_T0.max_size, &new_max_size)) {
          return -1;
      }

      // Double the max size of the funcref table
      wasm_ptr_t old_max_size = module_instance->w2c_T0.max_size;
      module_instance->w2c_T0.max_size = new_max_size;

      // Double the size of the funcref table buffer
      if (wasm_rt_grow_funcref_table(&module_instance->w2c_T0, old_max_size, wasm_rt_funcref_t {
          .func_type = wasm2c_ruby_get_func_type(0, 0),
          .func = NULL,
          .func_tailcallee = {.fn = NULL},
          .module_instance = module_instance.get(),
      }) != old_max_size) {
          module_instance->w2c_T0.max_size = old_max_size;
          return -1;
      }

      return next_func_ptr++;
  }


  //////////////////////////////////////////////////////////////////////////////
HEREDOC

################################################################################

declarations = []
bindings = []

File.readlines('tags', chomp: true).each do |line|
  line = line.split("\t")

  func_name = line[0]
  next unless func_name.start_with?('rb_')
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

  binding = ''
  transformed_args = Set[]
  buffers = []
  i = 0
  args.each_with_index do |arg, i|
    next if arg == '...'
    handler = ARG_HANDLERS[arg]

    # Generate bindings for converting the arguments
    if !handler[:func_ptr_args].nil? || handler[:anyargs]
      binding += "wasm_ptr_t v#{i} = _sbindgen_create_func_ptr();\n"
      binding += "if (v#{i} == (wasm_ptr_t)-1) {\n"
      buffers.reverse_each { |buf| binding += "    w2c_#{MODULE_NAME}_#{FREE_FUNC}(module_instance.get(), #{buf});\n" }
      binding += "    throw SandboxOutOfMemoryException();\n"
      binding += "}\n"
      if handler[:anyargs]
        binding += <<~HEREDOC
          module_instance->w2c_T0.data[v#{i}] = wasm_rt_funcref_t {
              .func_type = wasm2c_#{MODULE_NAME}_get_func_type(a#{args.length - 1} == -1 ? 3 : a#{args.length - 1} == -2 ? 2 : a#{args.length - 1} + 1, 1, #{([:ptr] * 16).map { |type| FUNC_TYPE_TABLE[type] }.join(', ')}),
              .func = (wasm_rt_function_ptr_t)a#{i},
              .func_tailcallee = {.fn = NULL},
              .module_instance = module_instance.get(),
          };
        HEREDOC
      else
        binding += <<~HEREDOC
          module_instance->w2c_T0.data[v#{i}] = wasm_rt_funcref_t {
              .func_type = wasm2c_#{MODULE_NAME}_get_func_type(#{handler[:func_ptr_args].length}, #{handler[:func_ptr_rets].length}#{handler[:func_ptr_args].empty? && handler[:func_ptr_rets].empty? ? '' : ', ' + (handler[:func_ptr_args] + handler[:func_ptr_rets]).map { |type| FUNC_TYPE_TABLE[type] }.join(', ')}),
              .func = (wasm_rt_function_ptr_t)a#{i},
              .func_tailcallee = {.fn = NULL},
              .module_instance = module_instance.get(),
          };
        HEREDOC
      end
      binding += "\n"
      transformed_args.add(i)
    elsif !handler[:buf_size].nil?
      binding += <<~HEREDOC
        wasm_ptr_t v#{i} = _sbindgen_malloc(#{handler[:buf_size].gsub('PREV_ARG', "a#{i - 1}").gsub('ARG', "a#{i}")});
        if (v#{i} == 0) {
      HEREDOC
      buffers.reverse_each { |buf| binding += "    w2c_#{MODULE_NAME}_#{FREE_FUNC}(module_instance.get(), #{buf});\n" }
      binding += <<~HEREDOC
            throw SandboxOutOfMemoryException();
        }
      HEREDOC
      binding += handler[:serialize].gsub('PREV_ARG', "a#{i - 1}").gsub('ARG', "a#{i}").gsub('BUF', "v#{i}")
      binding += "\n"
      transformed_args.add(i)
      buffers.append("v#{i}")
    end

    i += 1
  end

  # If this is a varargs function, manually generate bindings for getting the varargs based on the function name
  if !args.empty? && args[-1] == '...'
    case func_name
    when 'rb_funcall'
      binding += <<~HEREDOC
        wasm_ptr_t v = _sbindgen_malloc(a#{args.length - 2} * sizeof(wasm_ptr_t));
        if (v == 0) {
      HEREDOC
      buffers.reverse_each { |buf| binding += "    w2c_#{MODULE_NAME}_#{FREE_FUNC}(module_instance.get(), #{buf});\n" }
      binding += <<~HEREDOC
            throw SandboxOutOfMemoryException();
        }
        std::va_list a;
        va_start(a, a#{args.length - 2});
        for (long i = 0; i < a#{args.length - 2}; ++i) {
            ((wasm_ptr_t *)module_instance->w2c_memory.data + v)[i] = SERIALIZE_PTR(va_arg(a, wasm_ptr_t));
        }
        va_end(a);
      HEREDOC
      binding += "\n"
      buffers.append('v')
    when 'rb_rescue2'
      binding += <<~HEREDOC
        std::va_list a, b;
        va_start(a, a#{args.length - 2});
        va_copy(b, a);
        wasm_ptr_t n = 0;
        do ++n; while (va_arg(b, wasm_ptr_t));
        va_end(b);
        wasm_ptr_t v = _sbindgen_malloc(n * sizeof(wasm_ptr_t));
        if (v == 0) {
            va_end(a);
      HEREDOC
      buffers.reverse_each { |buf| binding += "    w2c_#{MODULE_NAME}_#{FREE_FUNC}(module_instance.get(), #{buf});\n" }
      binding += <<~HEREDOC
            throw SandboxOutOfMemoryException();
        }
        for (wasm_ptr_t i = 0; i < n; ++i) {
            ((wasm_ptr_t *)module_instance->w2c_memory.data + v)[i] = SERIALIZE_PTR(va_arg(a, wasm_ptr_t));
        }
      HEREDOC
      binding += "\n"
      buffers.append('v')
    else
      next
    end
  end

  # Generate bindings for running the actual function
  handler = RET_HANDLERS[ret]
  if handler[:primitive] != :void
    binding += <<~HEREDOC
      #{!RET_HANDLERS[ret][:keep] ? VAR_TYPE_TABLE[RET_HANDLERS[ret][:primitive]] : ret} r;
      do r = w2c_#{MODULE_NAME}_#{func_name}(#{(['module_instance.get()'] + (0...args.length).map { |i| args[i] == '...' ? 'v' : transformed_args.include?(i) ? "v#{i}" : "a#{i}" }).join(', ')}); while (w2c_#{MODULE_NAME}_#{YIELD_FUNC}(module_instance.get()));
    HEREDOC
  else
    binding += "do w2c_#{MODULE_NAME}_#{func_name}(#{(['module_instance.get()'] + (0...args.length).map { |i| args[i] == '...' ? 'v' : transformed_args.include?(i) ? "v#{i}" : "a#{i}" }).join(', ')}); while (w2c_#{MODULE_NAME}_#{YIELD_FUNC}(module_instance.get()));\n"
  end
  buffers.reverse_each { |buf| binding += "w2c_#{MODULE_NAME}_#{FREE_FUNC}(module_instance.get(), #{buf});\n" }
  if handler[:primitive] != :void
    binding += "return r;\n"
  end

  declarations.append("#{!RET_HANDLERS[ret][:keep] ? VAR_TYPE_TABLE[RET_HANDLERS[ret][:primitive]] : ret} #{func_name}(#{(0...args.length).map { |i| args[i] == '...' ? '...' : !ARG_HANDLERS[args[i]][:declaration].nil? ? ARG_HANDLERS[args[i]][:declaration] : !ARG_HANDLERS[args[i]][:keep] ? VAR_TYPE_TABLE[ARG_HANDLERS[args[i]][:primitive]] : args[i] }.join(', ')});\n")
  bindings.append("#{!RET_HANDLERS[ret][:keep] ? VAR_TYPE_TABLE[RET_HANDLERS[ret][:primitive]] : ret} SandboxBind::#{func_name}(#{(0...args.length).map { |i| args[i] == '...' ? '...' : !ARG_HANDLERS[args[i]][:formatter].nil? ? ARG_HANDLERS[args[i]][:formatter].call("a#{i}") : !ARG_HANDLERS[args[i]][:keep] ? "#{VAR_TYPE_TABLE[ARG_HANDLERS[args[i]][:primitive]]} a#{i}" : "#{args[i]} a#{i}" }.join(', ')}) {\n#{binding.split("\n").map { |line| "    #{line}".rstrip() }.join("\n")}\n}\n")
end

File.open('mkxp-sandbox-bindgen.h', 'w') do |file|
  file.write(HEADER_START)
  for declaration in declarations
    file.write('    ' + declaration)
  end
  file.write(HEADER_END)
end
File.open('mkxp-sandbox-bindgen.cpp', 'w') do |file|
  file.write(PRELUDE)
  for binding in bindings
    file.write("\n\n")
    file.write(binding)
  end
end
