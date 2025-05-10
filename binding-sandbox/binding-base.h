/*
** binding-base.h
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

#ifndef MKXPZ_SANDBOX_BINDING_BASE_H
#define MKXPZ_SANDBOX_BINDING_BASE_H

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <boost/core/enable_if.hpp>
#include <boost/type_traits/is_detected.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/asio/coroutine.hpp>
#include <mkxp-sandbox-ruby.h>
#include "types.h"

#ifdef MKXPZ_BIG_ENDIAN
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

// LLVM uses a stack alignment of 16 on WebAssembly targets
#define WASMSTACKALIGN 16

// Rounds a number up to the nearest multiple of the WebAssembly stack alignment
#define CEIL_WASMSTACKALIGN(x) (((wasm_size_t)(x) + (wasm_size_t)(WASMSTACKALIGN - 1)) & ~(wasm_size_t)(WASMSTACKALIGN - 1))

namespace mkxp_sandbox {
    template <typename...> struct decl_slots {};

    template <typename> struct get_num_slots;
    template <> struct get_num_slots<struct decl_slots<>> {
        static constexpr wasm_size_t value = 0;
    };
    template <typename Head, typename... Tail> struct get_num_slots<struct decl_slots<Head, Tail...>> {
        static constexpr wasm_size_t value = 1 + get_num_slots<struct decl_slots<Tail...>>::value;
    };

    // typename concat_slots<decl_slots<x1, x2, ... xn>, decl_slots<y1, y2, ..., ym>>::type -> decl_slots<x1, x2, ..., xn, y1, y2, ..., ym>
    template <typename, typename> struct concat_slots;
    template <typename... Head, typename... Tail> struct concat_slots<struct decl_slots<Head...>, struct decl_slots<Tail...>> {
        using type = decl_slots<Head..., Tail...>;
    };

    // typename get_last_slot<decl_slots<x1, x2, ..., xn>>::type -> xn
    template <typename> struct get_last_slot;
    template <typename Tail> struct get_last_slot<struct decl_slots<Tail>> {
        using type = Tail;
    };
    template <typename Head, typename... Tail> struct get_last_slot<struct decl_slots<Head, Tail...>> {
        using type = typename get_last_slot<decl_slots<Tail...>>::type;
    };

    // typename pop_last_slot<decl_slots<x1, x2, ..., xn-1, xn>>::type -> decl_slots<x1, x2, ..., xn-1>
    template <typename> struct pop_last_slot;
    template <typename Tail> struct pop_last_slot<struct decl_slots<Tail>> {
        using type = decl_slots<>;
    };
    template <typename Head, typename... Tail> struct pop_last_slot<struct decl_slots<Head, Tail...>> {
        using type = typename concat_slots<struct decl_slots<Head>, typename pop_last_slot<struct decl_slots<Tail...>>::type>::type;
    };

    // `slot_type<i, slots>::type` is the type of the `i`th slot.
    // For example:
    //     typedef decl_slots<uint64_t, uint32_t, uint16_t, uint8_t> slots;
    //     slot_type<0, slots>::type var0; // this variable should be of type `uint64_t`
    //     slot_type<1, slots>::type var1; // this variable should be of type `uint32_t`
    //     slot_type<2, slots>::type var2; // this variable should be of type `uint16_t`
    //     slot_type<3, slots>::type var3; // this variable should be of type `uint8_t`
    template <wasm_size_t Index, typename Slots> struct slot_type;
    template <typename Head, typename... Tail> struct slot_type<0, struct decl_slots<Head, Tail...>> {
	static_assert(std::is_integral<Head>::value || std::is_floating_point<Head>::value, "slots must have numeric types");
        typedef Head type;
    };
    template <wasm_size_t Index, typename Head, typename... Tail> struct slot_type<Index, struct decl_slots<Head, Tail...>> : slot_type<Index - 1, struct decl_slots<Tail...>> {};

    // `slots_size<slots>::value` is the total number of bytes required to store all the slots, including padding bytes between the slots but not including padding bytes after the last slot.
    // For example:
    //     typedef decl_slots<uint64_t, uint32_t, uint16_t, uint8_t> slots;
    //     constexpr wasm_size_t size = slots_size<slots>::value; // should be 15
    template <typename Slots> struct slots_size;
    template <> struct slots_size<struct decl_slots<>> {
        static constexpr wasm_size_t value = 0;
    };
    template <typename Head, typename... Tail> struct slots_size<struct decl_slots<Head, Tail...>> {
	static_assert(std::is_integral<typename get_last_slot<struct decl_slots<Head, Tail...>>::type>::value || std::is_floating_point<typename get_last_slot<struct decl_slots<Head, Tail...>>::type>::value, "slots must have numeric types");
    private:
        static constexpr wasm_size_t last_size = sizeof(typename get_last_slot<struct decl_slots<Head, Tail...>>::type);
        static constexpr wasm_size_t rest_size = slots_size<typename pop_last_slot<struct decl_slots<Head, Tail...>>::type>::value;
        static constexpr wasm_size_t rest_size_aligned_to_last_size = (rest_size - 1 + last_size) / last_size * last_size;
    public:
        static constexpr wasm_size_t value = rest_size_aligned_to_last_size + last_size;
    };

    template <wasm_size_t Index, typename> struct slot_offset_nothrow;
    template <wasm_size_t Index> struct slot_offset_nothrow<Index, struct decl_slots<>> {
        static constexpr wasm_size_t value = 0;
    };
    template <wasm_size_t Index, typename Head, typename... Tail> struct slot_offset_nothrow<Index, struct decl_slots<Head, Tail...>> {
        static constexpr wasm_size_t value = get_num_slots<struct decl_slots<Head, Tail...>>::value <= Index
            ? slots_size<struct decl_slots<Head, Tail...>>::value
            : slot_offset_nothrow<Index, typename pop_last_slot<struct decl_slots<Head, Tail...>>::type>::value;
    };

    // `slot_offset<i, slots>::value` is the byte offset of the `i`th slot.
    // For example:
    //     typedef decl_slots<uint64_t, uint32_t, uint16_t, uint8_t> slots;
    //     constexpr wasm_size_t slot0_offset = slot_offset<0, slots>::value; // should be 0
    //     constexpr wasm_size_t slot1_offset = slot_offset<1, slots>::value; // should be 8
    //     constexpr wasm_size_t slot2_offset = slot_offset<2, slots>::value; // should be 12
    //     constexpr wasm_size_t slot3_offset = slot_offset<3, slots>::value; // should be 14
    template <wasm_size_t Index, typename Slots> struct slot_offset;
    template <wasm_size_t Index, typename Head, typename... Tail> struct slot_offset<Index, struct decl_slots<Head, Tail...>> {
	static_assert(Index < get_num_slots<struct decl_slots<Head, Tail...>>::value, "index out of range");
	static constexpr wasm_size_t value = slot_offset_nothrow<Index, struct decl_slots<Head, Tail...>>::value;
    };

    // If the type `T::slots` exists,
    // then `declared_slots_size<T>::value` is equal to `slots_size<typename T::slots>::value` (i.e. the total size of the slots used by `T`).
    // Otherwise, it's equal to 0.
    template <typename T, typename Dummy = void> struct declared_slots_size;
    template <typename T> using slots_declaration = typename T::slots;
    template <typename T> struct declared_slots_size<T, typename boost::enable_if<boost::is_detected<slots_declaration, T>>::type> {
        static constexpr wasm_size_t value = slots_size<typename T::slots>::value;
    };
    template <typename T> struct declared_slots_size<T, typename boost::disable_if<boost::is_detected<slots_declaration, T>>::type> {
        static constexpr wasm_size_t value = 0;
    };

    struct binding_base {
    private:
        typedef std::tuple<wasm_ptr_t, wasm_ptr_t, wasm_ptr_t> key_t;

        struct stack_frame {
            void *coroutine;
            void (*destructor)(void *coroutine);
            wasm_ptr_t stack_ptr;
            stack_frame(void *coroutine, void (*destructor)(void *coroutine), wasm_ptr_t stack_ptr);
            stack_frame(const struct stack_frame &frame) = delete;
            stack_frame(struct stack_frame &&frame) noexcept;
            struct stack_frame &operator=(const struct stack_frame &frame) = delete;
            struct stack_frame &operator=(struct stack_frame &&frame) noexcept;
            ~stack_frame();
        };

        struct fiber {
            key_t key;
            std::vector<struct stack_frame> stack;
            size_t stack_index;
        };

        std::shared_ptr<struct w2c_ruby> _instance;
        std::unordered_map<key_t, struct fiber, boost::hash<key_t>> fibers;
        wasm_ptr_t next_func_ptr;
        wasm_ptr_t stack_ptr;

    public:
        binding_base(std::shared_ptr<struct w2c_ruby> m);
        ~binding_base();
        struct w2c_ruby &instance() const noexcept;
        uint8_t *get() const noexcept;
        uint8_t *operator*() const noexcept;
        wasm_ptr_t sandbox_malloc(wasm_size_t);
        void sandbox_free(wasm_ptr_t ptr);
        wasm_ptr_t rtypeddata_data(VALUE obj) const noexcept;
        void rtypeddata_dmark(wasm_ptr_t data, wasm_ptr_t ptr);
        void rtypeddata_dfree(wasm_ptr_t data, wasm_ptr_t ptr);
        wasm_size_t rtypeddata_dsize(wasm_ptr_t data, wasm_ptr_t ptr);
        void rtypeddata_dcompact(wasm_ptr_t data, wasm_ptr_t ptr);

        template <typename T> struct stack_frame_guard {
            static_assert(std::is_base_of<boost::asio::coroutine, T>::value, "`T` must be a subclass of `boost::asio::coroutine`");
            friend struct binding_base;

        private:
            T *coroutine;
            struct binding_base *bind;
            struct fiber *fiber;

            static void coroutine_destructor(void *coroutine) {
                ((T *)coroutine)->~T();
            }

            static struct fiber &init_fiber(struct binding_base &bind) {
                key_t key = {
                    *(wasm_ptr_t *)(*bind + bind.instance().w2c_mkxp_sandbox_fiber_entry_point),
                    *(wasm_ptr_t *)(*bind + bind.instance().w2c_mkxp_sandbox_fiber_arg0),
                    *(wasm_ptr_t *)(*bind + bind.instance().w2c_mkxp_sandbox_fiber_arg1),
                };
                if (bind.fibers.count(key) == 0) {
                    bind.fibers[key] = (struct fiber){.key = key};
                }
                return bind.fibers[key];
            }

            template <typename U> static typename boost::enable_if<std::is_constructible<U, struct binding_base &>, U *>::type construct_frame(struct binding_base &bind) {
                return new U(bind);
            }

            template <typename U> static typename boost::disable_if<std::is_constructible<U, struct binding_base &>, U *>::type construct_frame(struct binding_base &bind) {
                return new U;
            }

            stack_frame_guard(struct binding_base &b) : bind(&b), fiber(&init_fiber(b)) {
                uint32_t state = w2c_ruby_asyncify_get_state(&b.instance());

                if (fiber->stack_index > fiber->stack.size()) {
                    std::abort();
                }

                // If Asyncify is rewinding, restore the stack frame from before Asyncify started unwinding
                if (state == 2) {
                    if (fiber->stack_index == fiber->stack.size()) {
                        std::abort();
                    }
                    struct stack_frame &frame = fiber->stack[fiber->stack_index++];
                    b.stack_ptr = frame.stack_ptr;
                    coroutine = (T *)frame.coroutine;
                    return;
                }

                // Otherwise, create a new stack frame
                assert(state == 0);
                while (fiber->stack.size() > fiber->stack_index) {
                    bind->stack_ptr = fiber->stack.back().stack_ptr;
                    fiber->stack.pop_back();
                }
                ++fiber->stack_index;
                b.stack_ptr = w2c_ruby_rb_wasm_get_stack_pointer(&b.instance()) - CEIL_WASMSTACKALIGN(declared_slots_size<T>::value);
                assert(b.stack_ptr % sizeof(VALUE) == 0);
                assert(b.stack_ptr % WASMSTACKALIGN == 0);
                if (declared_slots_size<T>::value != 0) {
                    w2c_ruby_rb_wasm_set_stack_pointer(&b.instance(), b.stack_ptr);
                }
                coroutine = construct_frame<T>(b);
                fiber->stack.emplace_back(
                    coroutine,
                    coroutine_destructor,
                    b.stack_ptr
                );
            }

        public:
            stack_frame_guard(const stack_frame_guard &frame) = delete;

            stack_frame_guard(stack_frame_guard &&frame) noexcept : coroutine(std::exchange(frame.coroutine, nullptr)), bind(std::exchange(frame.bind, nullptr)), fiber(std::exchange(frame.fiber, nullptr)) {}

            struct stack_frame_guard &operator=(const stack_frame_guard &frame) = delete;

            struct stack_frame_guard &operator=(stack_frame_guard &&frame) noexcept {
                coroutine = std::exchange(frame.coroutine, nullptr);
                bind = std::exchange(frame.bind, nullptr);
                fiber = std::exchange(frame.fiber, nullptr);
                return *this;
            }

            ~stack_frame_guard() {
                if (fiber == nullptr) {
                    return;
                }

                assert(fiber->stack_index > 0);
                assert(fiber->stack_index - 1 < fiber->stack.size());

                if (get()->is_complete()) {
                    while (fiber->stack.size() > fiber->stack_index) {
                        bind->stack_ptr = fiber->stack.back().stack_ptr;
                        fiber->stack.pop_back();
                    }

                    assert(fiber->stack.size() == fiber->stack_index);

                    w2c_ruby_rb_wasm_set_stack_pointer(&bind->instance(), fiber->stack.back().stack_ptr + CEIL_WASMSTACKALIGN(declared_slots_size<T>::value));
                    bind->stack_ptr = fiber->stack.back().stack_ptr;
                    fiber->stack.pop_back();
                }

                if (--fiber->stack_index > 0) {
                    bind->stack_ptr = fiber->stack[fiber->stack_index - 1].stack_ptr;
                }

                if (fiber->stack.empty()) {
                    bind->fibers.erase(fiber->key);
                }
            }

            inline T *get() const noexcept {
                return coroutine;
            }

            inline T &operator()() const noexcept {
                return *get();
            }
        };

        template <typename T> struct stack_frame_guard<T> bind() {
            return *this;
        }

        wasm_ptr_t stack_pointer() const noexcept {
            return stack_ptr;
        }
    };
}

#undef SERIALIZE_32
#undef SERIALIZE_64
#undef SERIALIZE_VALUE

#endif // MKXPZ_SANDBOX_BINDING_BASE
