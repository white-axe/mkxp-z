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

using namespace mkxp_sandbox;

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

binding_base::binding_base(std::shared_ptr<struct w2c_ruby> m) : next_free_objkey(0), _instance(m) {}

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

const char *mkxp_sandbox::sandbox_str(struct w2c_ruby &instance, wasm_ptr_t address) noexcept {
#ifdef MKXPZ_BIG_ENDIAN
    static std::string buf;
    buf.clear();
    buf.reserve(sandbox_strlen(instance, address));
    const char *ptr = (const char *)sandbox_ptr(instance, address);
    while ((uint8_t *)ptr != instance.w2c_memory.data && *--ptr) {
        buf.push_back(*ptr);
    }
    return buf.c_str();
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

const char *binding_base::str(wasm_ptr_t address) const noexcept {
    return sandbox_str(instance(), address);
}

void binding_base::strcpy(wasm_ptr_t dst_address, const char *src) const noexcept {
    sandbox_strcpy(instance(), dst_address, src);
}

void binding_base::strncpy(wasm_ptr_t dst_address, const char *src, wasm_size_t max_size) const noexcept {
    sandbox_strncpy(instance(), dst_address, src, max_size);
}

binding_base::object::object(wasm_size_t typenum, void *ptr) : typenum(typenum), inner {.ptr = ptr} {}

binding_base::object::object(struct object &&object) noexcept : typenum(std::exchange(object.typenum, 0)), inner(std::exchange(object.inner, (union binding_base::object::inner){.next = 0})) {}

struct binding_base::object &binding_base::object::operator=(struct object &&object) noexcept {
    typenum = std::exchange(object.typenum, 0);
    inner = std::exchange(object.inner, (union binding_base::object::inner){.next = 0});
    return *this;
}

binding_base::object::~object() {
    if (typenum != 0) {
        if (typenum > typenum_table_size) {
            std::abort();
        }
        typenum_table[typenum - 1].destructor(inner.ptr);
    }
}

wasm_objkey_t binding_base::create_object(wasm_size_t typenum, void *ptr) {
    if (ptr == nullptr || typenum == 0 || typenum > typenum_table_size) {
        std::abort();
    }
    if (next_free_objkey == 0) {
        objects.emplace_back(typenum, ptr);
        if ((size_t)(wasm_objkey_t)objects.size() < objects.size()) {
            MKXPZ_THROW(std::bad_alloc());
        }
        return objects.size();
    } else {
        wasm_objkey_t key = next_free_objkey;
        struct object &object = objects[next_free_objkey - 1];
        assert(object.typenum == 0);
        next_free_objkey = object.inner.next;
        object.typenum = typenum;
        object.inner.ptr = ptr;
        return key;
    }
}

void *binding_base::get_object(wasm_objkey_t key) {
    if (key == 0 || key > objects.size()) {
        std::abort();
    }
    struct object &object = objects[key - 1];
    if (object.typenum == 0 || object.typenum > typenum_table_size) {
        std::abort();
    }
    return object.inner.ptr;
}

bool binding_base::check_object_type(wasm_objkey_t key, wasm_size_t typenum) {
    if (key == 0 || key > objects.size()) {
        std::abort();
    }
    struct object &object = objects[key - 1];
    if (object.typenum == 0 || object.typenum > typenum_table_size) {
        std::abort();
    }
    return object.typenum == typenum;
}

void binding_base::destroy_object(wasm_objkey_t key) {
    if (key == 0 || key > objects.size()) {
        std::abort();
    }
    struct object &object = objects[key - 1];
    if (object.typenum == 0 || object.typenum > typenum_table_size) {
        std::abort();
    }
    typenum_table[object.typenum - 1].destructor(object.inner.ptr);
    object.typenum = 0;
    object.inner.next = next_free_objkey;
    next_free_objkey = key;
}
