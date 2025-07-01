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
#include "wasm-rt.h"
#include "mkxp-sandbox-ruby-indices.h"
#include <algorithm>
#include <cassert>
#include <boost/preprocessor/cat.hpp>

using namespace mkxp_sandbox;

binding_base::deser_stack_frame::deser_stack_frame(wasm_ptr_t stack_ptr, int32_t state) : stack_ptr(stack_ptr), state(state) {}

binding_base::stack_frame::stack_frame(void *coroutine, void (*destructor)(void *coroutine), wasm_ptr_t stack_ptr) : coroutine(coroutine), destructor(destructor), stack_ptr(stack_ptr) {}

binding_base::stack_frame::stack_frame(struct binding_base::stack_frame &&frame) noexcept : coroutine(std::exchange(frame.coroutine, nullptr)), destructor(std::exchange(frame.destructor, nullptr)), stack_ptr(std::exchange(frame.stack_ptr, 0)) {}

struct binding_base::stack_frame &binding_base::stack_frame::operator=(struct binding_base::stack_frame &&frame) noexcept {
    coroutine = std::exchange(frame.coroutine, nullptr);
    destructor = std::exchange(frame.destructor, nullptr);
    stack_ptr = std::exchange(frame.stack_ptr, 0);
    return *this;
}

binding_base::stack_frame::~stack_frame() {
    if (destructor != nullptr) {
        destructor(coroutine);
    }
}

binding_base::binding_base(std::shared_ptr<struct w2c_ruby> m) : _instance(m) {}

binding_base::~binding_base() {
    // Destroy all stack frames in order from top to bottom to enforce a portable, compiler-independent ordering of stack frame destruction
    // If we let the compiler use its default destructor, the stack frames may not be deallocated in a particular order, which can lead to hard-to-detect bugs if somehow a bug depends on the order in which the stack frames are deallocated
    for (auto &it : fibers) {
        while (!it.second.stack.empty()) {
            stack_ptr = it.second.stack.back().stack_ptr;
            it.second.stack.pop_back();
        }
    }
}

struct w2c_ruby &binding_base::instance() const noexcept {
    return *_instance;
}

wasm_ptr_t binding_base::sandbox_malloc(wasm_size_t size) {
    wasm_ptr_t buf = w2c_ruby_mkxp_sandbox_malloc(&instance(), size);

    // Verify that the entire allocated buffer is in valid memory
    wasm_ptr_t buf_end;
    if (buf == 0 || (buf_end = buf + size) < buf || buf_end >= instance().w2c_memory.size) {
        return 0;
    }

    return buf;
}

void binding_base::sandbox_free(wasm_ptr_t ptr) {
    w2c_ruby_mkxp_sandbox_free(&instance(), ptr);
}

wasm_ptr_t binding_base::rtypeddata_data(VALUE obj) const noexcept {
    return obj + ref<wasm_size_t>(instance().w2c_mkxp_sandbox_rtypeddata_data_offset);
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

wasm_size_t binding_base::memory_capacity() const noexcept {
    return instance().w2c_memory.capacity;
}

wasm_size_t binding_base::memory_size() const noexcept {
    return instance().w2c_memory.size;
}

void binding_base::copy_memory_to(void *ptr) const noexcept {
    std::memcpy(ptr, instance().w2c_memory.data, memory_size());
}

void binding_base::copy_memory_from(const void *ptr, wasm_size_t size, wasm_size_t capacity, bool swap_bytes) noexcept {
    capacity = std::max(size, capacity);
    wasm_rt_replace_memory(&instance().w2c_memory, size, capacity);
    if (swap_bytes) {
        std::reverse_copy((const uint8_t *)ptr, (const uint8_t *)ptr + size, (uint8_t *)instance().w2c_memory.data);
    } else {
        std::memcpy(instance().w2c_memory.data, ptr, memory_size());
    }
}

void *mkxp_sandbox::sandbox_ptr(struct w2c_ruby &instance, wasm_ptr_t address) noexcept {
    if (address >= instance.w2c_memory.size) {
        std::abort();
    }
#ifdef MKXPZ_BIG_ENDIAN
    return instance.w2c_memory.data + instance.w2c_memory.size - address;
#else
    return instance.w2c_memory.data + address;
#endif // MKXPZ_BIG_ENDIAN
}

wasm_size_t mkxp_sandbox::sandbox_strlen(struct w2c_ruby &instance, wasm_ptr_t address) noexcept {
    const char *ptr = (char *)sandbox_ptr(instance, address);
#ifdef MKXPZ_BIG_ENDIAN
    wasm_size_t size = 0;
    while ((uint8_t *)ptr != instance.w2c_memory.data && *--ptr) {
        ++size;
    }
    return size;
#else
    const char *end = (const char *)std::memchr(ptr, 0, instance.w2c_memory.size - address);
    return ptr == nullptr ? instance.w2c_memory.size - address : end - ptr;
#endif
}

struct sandbox_str_guard mkxp_sandbox::sandbox_str(struct w2c_ruby &instance, wasm_ptr_t address) noexcept {
#ifdef MKXPZ_BIG_ENDIAN
    std::string str;
    str.reserve(sandbox_strlen(instance, address));
    const char *ptr = (const char *)sandbox_ptr(instance, address);
    while ((uint8_t *)ptr != instance.w2c_memory.data && *--ptr) {
        str.push_back(*ptr);
    }
    return str;
#else
    if (instance.w2c_memory.size - address <= sandbox_strlen(instance, address)) {
        std::abort();
    }
    return (const char *)sandbox_ptr(instance, address);
#endif // MKXPZ_BIG_ENDIAN
}

void mkxp_sandbox::sandbox_strcpy(struct w2c_ruby &instance, wasm_ptr_t dst_address, const char *src) noexcept {
#ifdef MKXPZ_BIG_ENDIAN
    char *dst = (char *)sandbox_ptr(instance, dst_address);
    while (*src) {
        if ((uint8_t *)dst == instance.w2c_memory.data) {
            std::abort();
        }
        *--dst = *src++;
    }
    *dst = 0;
#else
    if (instance.w2c_memory.size - dst_address <= std::strlen(src)) {
        std::abort();
    }
    char *dst = (char *)sandbox_ptr(instance, dst_address);
    std::strcpy(dst, src);
#endif
}

void mkxp_sandbox::sandbox_strncpy(struct w2c_ruby &instance, wasm_ptr_t dst_address, const char *src, wasm_size_t max_size) noexcept {
#ifdef MKXPZ_BIG_ENDIAN
    char *dst = (char *)sandbox_ptr(instance, dst_address);
    while (max_size && *src) {
        if ((uint8_t *)dst == instance.w2c_memory.data) {
            std::abort();
        }
        *--dst = *src++;
        --max_size;
    }
    if (max_size) {
        *dst = 0;
    }
#else
    if (instance.w2c_memory.size - dst_address <= std::strlen(src)) {
        std::abort();
    }
    char *dst = (char *)sandbox_ptr(instance, dst_address);
    std::strncpy(dst, src, max_size);
#endif
}

void *binding_base::ptr(wasm_ptr_t address) const noexcept {
    return sandbox_ptr(instance(), address);
}

wasm_size_t binding_base::strlen(wasm_ptr_t address) const noexcept {
    return sandbox_strlen(instance(), address);
}

struct sandbox_str_guard binding_base::str(wasm_ptr_t address) const noexcept {
    return sandbox_str(instance(), address);
}

void binding_base::strcpy(wasm_ptr_t dst_address, const char *src) const noexcept {
    sandbox_strcpy(instance(), dst_address, src);
}

void binding_base::strncpy(wasm_ptr_t dst_address, const char *src, wasm_size_t max_size) const noexcept {
    sandbox_strncpy(instance(), dst_address, src, max_size);
}

binding_base::object::object() : ptr(nullptr), typenum(0) {}

binding_base::object::object(wasm_size_t typenum, void *ptr) : ptr(ptr), typenum(typenum) {}

binding_base::object::object(struct object &&object) noexcept : ptr(std::exchange(object.ptr, nullptr)), typenum(std::exchange(object.typenum, 0)) {}

struct binding_base::object &binding_base::object::operator=(struct object &&object) noexcept {
    ptr = std::exchange(object.ptr, nullptr);
    typenum = std::exchange(object.typenum, 0);
    return *this;
}

binding_base::object::~object() {
    if (typenum != 0) {
        if (typenum > typenum_table_size) {
            std::abort();
        }
        typenum_table[typenum - 1].destroy(ptr);
    }
}

wasm_objkey_t binding_base::create_object(wasm_size_t typenum, void *ptr) {
    if (ptr == nullptr || typenum == 0 || typenum > typenum_table_size) {
        std::abort();
    }
    if (vacant_object_keys.empty()) {
        objects.emplace_back(typenum, ptr);
        if ((size_t)(wasm_objkey_t)objects.size() < objects.size()) {
            MKXPZ_THROW(std::bad_alloc());
        }
        return objects.size();
    } else {
        wasm_objkey_t key = vacant_object_keys.minimum();
        vacant_object_keys.pop_minimum();
        struct object &object = objects[key - 1];
        assert(object.typenum == 0);
        object.typenum = typenum;
        object.ptr = ptr;
        return key;
    }
}

void *binding_base::get_object(wasm_objkey_t key) const {
    if (key == 0 || key > objects.size()) {
        std::abort();
    }
    const struct object &object = objects[key - 1];
    if (object.typenum == 0 || object.typenum > typenum_table_size) {
        std::abort();
    }
    return object.ptr;
}

bool binding_base::check_object_type(wasm_objkey_t key, wasm_size_t typenum) const {
    return key != 0 && key <= objects.size() && objects[key - 1].typenum == typenum;
}

void binding_base::destroy_object(wasm_objkey_t key) {
    if (key == 0 || key > objects.size()) {
        std::abort();
    }
    struct object &object = objects[key - 1];
    if (object.typenum == 0 || object.typenum > typenum_table_size) {
        std::abort();
    }
    if (key == objects.size()) {
        objects.pop_back();
        while (!objects.empty() && objects.back().typenum == 0) {
            assert(!vacant_object_keys.empty() && vacant_object_keys.maximum() == objects.size());
            vacant_object_keys.pop_maximum();
            objects.pop_back();
        }
    } else {
        typenum_table[object.typenum - 1].destroy(object.ptr);
        object.typenum = 0;
        object.ptr = nullptr;
        vacant_object_keys.push(key);
    }
}

wasm_ptr_t binding_base::get_machine_stack_pointer() const noexcept {
    return w2c_ruby_rb_wasm_get_stack_pointer(&instance());
}

void binding_base::set_machine_stack_pointer(wasm_ptr_t sp) noexcept {
    w2c_ruby_rb_wasm_set_stack_pointer(&instance(), sp);
}

uint8_t binding_base::get_asyncify_state() const noexcept {
    return (uint8_t)BOOST_PP_CAT(instance().w2c_g, MKXPZ_SANDBOX_ASYNCIFY_STATE_INDEX);
}

void binding_base::set_asyncify_state(uint8_t state) noexcept {
    BOOST_PP_CAT(instance().w2c_g, MKXPZ_SANDBOX_ASYNCIFY_STATE_INDEX) = state;
}

wasm_ptr_t binding_base::get_asyncify_data() const noexcept {
    return BOOST_PP_CAT(instance().w2c_g, MKXPZ_SANDBOX_ASYNCIFY_DATA_INDEX);
}

void binding_base::set_asyncify_data(wasm_ptr_t ptr) noexcept {
    BOOST_PP_CAT(instance().w2c_g, MKXPZ_SANDBOX_ASYNCIFY_DATA_INDEX) = ptr;
}
