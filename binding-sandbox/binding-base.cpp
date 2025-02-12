/*
** binding-base.cpp
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

#include "binding-base.h"

#if MKXPZ_BIG_ENDIAN
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

using namespace mkxp_sandbox;

binding_base::stack_frame::stack_frame(struct binding_base &bind, void (*destructor)(void *ptr), boost::typeindex::type_index type, wasm_ptr_t ptr) : bind(bind), destructor(destructor), type(type), ptr(ptr) {}

binding_base::stack_frame::~stack_frame() {
    destructor(*bind + ptr);
}

binding_base::binding_base(std::shared_ptr<struct w2c_ruby> m) : next_func_ptr(-1), _instance(m) {}

binding_base::~binding_base() {
    // Destroy all stack frames in order from top to bottom to enforce a portable, compiler-independent ordering of stack frame destruction
    // If we let the compiler use its default destructor, the stack frames may not be deallocated in a particular order, which can lead to hard-to-detect bugs if somehow a bug depends on the order in which the stack frames are deallocated
    for (auto &it : fibers) {
        while (!it.second.stack.empty()) {
            it.second.stack.pop_back();
        }
    }
}

struct w2c_ruby &binding_base::instance() const noexcept {
    return *_instance;
}

uint8_t *binding_base::get() const noexcept {
    return instance().w2c_memory.data;
}

uint8_t *binding_base::operator*() const noexcept {
    return get();
}

wasm_ptr_t binding_base::sandbox_malloc(wasm_size_t size) {
    wasm_ptr_t buf = w2c_ruby_mkxp_sandbox_malloc(&instance(), size);

    // Verify that the entire allocated buffer is in valid memory
    wasm_ptr_t buf_end;
    if (buf == 0 || __builtin_add_overflow(buf, size, &buf_end) || buf_end >= instance().w2c_memory.size) {
        return 0;
    }

    return buf;
}

void binding_base::sandbox_free(wasm_ptr_t ptr) {
    w2c_ruby_mkxp_sandbox_free(&instance(), ptr);
}

wasm_ptr_t binding_base::rtypeddata_data(VALUE obj) const noexcept {
    return SERIALIZE_VALUE(obj) + *(wasm_size_t *)(instance().w2c_memory.data + instance().w2c_mkxp_sandbox_rtypeddata_data_offset);
}

void binding_base::rtypeddata_dmark(wasm_ptr_t data, wasm_ptr_t ptr) {
    w2c_ruby_mkxp_sandbox_rtypeddata_dmark(&instance(), data, ptr);
}

void binding_base::rtypeddata_dfree(wasm_ptr_t data, wasm_ptr_t ptr) {
    w2c_ruby_mkxp_sandbox_rtypeddata_dfree(&instance(), data, ptr);
}

wasm_size_t binding_base::rtypeddata_dsize(wasm_ptr_t data, wasm_ptr_t ptr) {
    return w2c_ruby_mkxp_sandbox_rtypeddata_dsize(&instance(), data, ptr);
}

void binding_base::rtypeddata_dcompact(wasm_ptr_t data, wasm_ptr_t ptr) {
    w2c_ruby_mkxp_sandbox_rtypeddata_dcompact(&instance(), data, ptr);
}
