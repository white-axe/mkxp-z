/*
** binding-util.h
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

#ifndef MKXPZ_SANDBOX_BINDING_UTIL_H
#define MKXPZ_SANDBOX_BINDING_UTIL_H

#include <type_traits>
#include <boost/optional.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/size.hpp>
#include "core.h"
#include "exception.h"
#include "sandbox.h"

#include "bitmap.h"
#include "etc.h"
#include "font.h"
#include "plane.h"
#include "sprite.h"
#include "table.h"
#include "tilemap.h"
#include "tilemapvx.h"
#include "viewport.h"
#include "window.h"
#include "windowvx.h"

#define SANDBOX_SLOT(slot_index) (::mkxp_sandbox::sb()->ref<typename ::mkxp_sandbox::slot_type<(slot_index), slots>::type>(::mkxp_sandbox::sb()->stack_pointer() + ::mkxp_sandbox::slot_offset<(slot_index), slots>::value))

#define SANDBOX_AWAIT(coroutine, ...) \
    do { \
        { \
            using namespace ::mkxp_sandbox; \
            if (_sandbox_await<struct coroutine>(__VA_ARGS__)) { \
                break; \
            } \
        } \
        BOOST_ASIO_CORO_YIELD; \
    } while (1)

#define SANDBOX_AWAIT_R(reference, coroutine, ...) \
    do { \
        { \
            using namespace ::mkxp_sandbox; \
            typedef std::remove_reference<decltype(reference)>::type _sandbox_await_output_t; \
            boost::optional<_sandbox_await_output_t> _sandbox_await_output = _sandbox_await_r<struct coroutine, _sandbox_await_output_t>(__VA_ARGS__); \
            if (_sandbox_await_output.has_value()) { \
                (reference) = *_sandbox_await_output; \
                break; \
            } \
        } \
        BOOST_ASIO_CORO_YIELD; \
    } while (1)

#define SANDBOX_AWAIT_S(slot_index, coroutine, ...) SANDBOX_AWAIT_R(SANDBOX_SLOT(slot_index), coroutine, __VA_ARGS__)

#define SANDBOX_YIELD \
    do { \
        using namespace ::mkxp_sandbox; \
        sb()._begin_yield(); \
        BOOST_ASIO_CORO_YIELD; \
        sb()._end_yield(); \
    } while (0)

#define SANDBOX_VALUE_TO_BOOL(value) ((value) != SANDBOX_FALSE && (value) != SANDBOX_NIL)

#define SANDBOX_BOOL_TO_VALUE(boolean) ((boolean) ? SANDBOX_TRUE : SANDBOX_FALSE)

#define SANDBOX_DEF_ALLOC(rbtype) \
    static VALUE alloc(VALUE _klass) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            typedef decl_slots<VALUE> slots; \
            VALUE operator()(VALUE _klass) { \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_S(0, rb_data_typed_object_wrap, _klass, 0, rbtype); \
                } \
                return SANDBOX_SLOT(0); \
            } \
        }; \
        return sb()->bind<struct coro>()()(_klass); \
    }

#define SANDBOX_DEF_ALLOC_WITH_INIT(rbtype, initializer) \
    static VALUE alloc(VALUE _klass) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            typedef decl_slots<VALUE> slots; \
            VALUE operator()(VALUE _klass) { \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_S(0, rb_data_typed_object_wrap, _klass, 0, rbtype); \
                    set_private_data(SANDBOX_SLOT(0), initializer); \
                } \
                return SANDBOX_SLOT(0); \
            } \
        }; \
        return sb()->bind<struct coro>()()(_klass); \
    }

#define SANDBOX_DEF_CLASS_PROP_B(S, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return SANDBOX_BOOL_TO_VALUE(S::get##prop()); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        bool v = SANDBOX_VALUE_TO_BOOL(value); \
        S::set##prop(v); \
        return value; \
    }

#define SANDBOX_DEF_CLASS_PROP(V, num2val, val2num, S, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return sb()->bind<struct num2val>()()(S::get##prop()); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE operator()(VALUE self, VALUE value) { \
                typedef decl_slots<V> slots; \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_S(0, val2num, value); \
                    S::set##prop(SANDBOX_SLOT(0)); \
                } \
                return value; \
            } \
        }; \
        return sb()->bind<struct coro>()()(self, value); \
    }

#define SANDBOX_DEF_CLASS_PROP_I(S, prop, name) SANDBOX_DEF_CLASS_PROP(int32_t, rb_ll2inum, rb_num2int, S, prop, name)
#define SANDBOX_DEF_CLASS_PROP_F(S, prop, name) SANDBOX_DEF_CLASS_PROP(float, rb_float_new, rb_num2dbl, S, prop, name)
#define SANDBOX_DEF_CLASS_PROP_D(S, prop, name) SANDBOX_DEF_CLASS_PROP(double, rb_float_new, rb_num2dbl, S, prop, name)

#define SANDBOX_DEF_CLASS_PROP_OBJ_REF(S, V, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return sb()->bind<struct rb_iv_get>()()(self, #name); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE operator()(VALUE self, VALUE value) { \
                BOOST_ASIO_CORO_REENTER (this) { \
                    { \
                        V *v = value == SANDBOX_NIL ? nullptr : get_private_data<V>(value); \
                        S::set##prop(v); \
                    } \
                    SANDBOX_AWAIT(rb_iv_set, self, #name, value); \
                } \
                return value; \
            } \
        }; \
        return sb()->bind<struct coro>()()(self, value); \
    }

#define SANDBOX_DEF_CLASS_PROP_OBJ_VAL(S, V, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return sb()->bind<struct rb_iv_get>()()(self, #name); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        V *v = get_private_data<V>(value); \
        S::set##prop(*v); \
        return value; \
    }

#define SANDBOX_DEF_PROP_B(S, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        S *s = get_private_data<S>(self); \
        return SANDBOX_BOOL_TO_VALUE(s->get##prop()); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        S *s = get_private_data<S>(self); \
        bool v = SANDBOX_VALUE_TO_BOOL(value); \
        s->set##prop(v); \
        return value; \
    }

#define SANDBOX_DEF_PROP(V, num2val, val2num, S, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return sb()->bind<struct num2val>()()(get_private_data<S>(self)->get##prop()); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE operator()(VALUE self, VALUE value) { \
                typedef decl_slots<V> slots; \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_S(0, val2num, value); \
                    S *s = get_private_data<S>(self); \
                    s->set##prop(SANDBOX_SLOT(0)); \
                } \
                return value; \
            } \
        }; \
        return sb()->bind<struct coro>()()(self, value); \
    }

#define SANDBOX_DEF_PROP_I(S, prop, name) SANDBOX_DEF_PROP(int32_t, rb_ll2inum, rb_num2int, S, prop, name)
#define SANDBOX_DEF_PROP_F(S, prop, name) SANDBOX_DEF_PROP(float, rb_float_new, rb_num2dbl, S, prop, name)
#define SANDBOX_DEF_PROP_D(S, prop, name) SANDBOX_DEF_PROP(double, rb_float_new, rb_num2dbl, S, prop, name)

#define SANDBOX_DEF_PROP_OBJ_REF(S, V, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return sb()->bind<struct rb_iv_get>()()(self, #name); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE operator()(VALUE self, VALUE value) { \
                BOOST_ASIO_CORO_REENTER (this) { \
                    { \
                        S *s = get_private_data<S>(self); \
                        V *v = value == SANDBOX_NIL ? nullptr : get_private_data<V>(value); \
                        s->set##prop(v); \
                    } \
                    SANDBOX_AWAIT(rb_iv_set, self, #name, value); \
                } \
                return value; \
            } \
        }; \
        return sb()->bind<struct coro>()()(self, value); \
    }

#define SANDBOX_DEF_PROP_OBJ_VAL(S, V, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return sb()->bind<struct rb_iv_get>()()(self, #name); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        S *s = get_private_data<S>(self); \
        V *v = get_private_data<V>(value); \
        s->set##prop(*v); \
        return value; \
    }

#define SANDBOX_DEF_GFX_PROP_B(S, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE operator()(VALUE self) { \
                typedef decl_slots<uint8_t> slots; \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_GUARD(SANDBOX_SLOT(0) = get_private_data<S>(self)->get##prop(sb().e)); \
                } \
                return SANDBOX_BOOL_TO_VALUE(SANDBOX_SLOT(0)); \
            } \
        }; \
        return sb()->bind<struct coro>()()(self); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE operator()(VALUE self, VALUE value) { \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_GUARD_L(get_private_data<S>(self)->set##prop(sb().e, SANDBOX_VALUE_TO_BOOL(value))); \
                } \
                return value; \
            } \
        }; \
        return sb()->bind<struct coro>()()(self, value); \
    }

#define SANDBOX_DEF_GFX_PROP(V, num2val, val2num, S, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE operator()(VALUE self) { \
                typedef decl_slots<VALUE> slots; \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_GUARD(SANDBOX_AWAIT_S(0, num2val, get_private_data<S>(self)->get##prop(sb().e))); \
                } \
                return SANDBOX_SLOT(0); \
            } \
        }; \
        return sb()->bind<struct coro>()()(self); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE operator()(VALUE self, VALUE value) { \
                typedef decl_slots<V> slots; \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_S(0, val2num, value); \
                    SANDBOX_GUARD_L(get_private_data<S>(self)->set##prop(sb().e, SANDBOX_SLOT(0))); \
                } \
                return value; \
            } \
        }; \
        return sb()->bind<struct coro>()()(self, value); \
    }

#define SANDBOX_DEF_GFX_PROP_I(S, prop, name) SANDBOX_DEF_GFX_PROP(int32_t, rb_ll2inum, rb_num2int, S, prop, name)
#define SANDBOX_DEF_GFX_PROP_F(S, prop, name) SANDBOX_DEF_GFX_PROP(float, rb_float_new, rb_num2dbl, S, prop, name)
#define SANDBOX_DEF_GFX_PROP_D(S, prop, name) SANDBOX_DEF_GFX_PROP(double, rb_float_new, rb_num2dbl, S, prop, name)

#define SANDBOX_DEF_GFX_PROP_OBJ_REF(S, V, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return sb()->bind<struct rb_iv_get>()()(self, #name); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE operator()(VALUE self, VALUE value) { \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_GUARD_L(get_private_data<S>(self)->set##prop(sb().e, value == SANDBOX_NIL ? nullptr : get_private_data<V>(value))); \
                    SANDBOX_AWAIT(rb_iv_set, self, #name, value); \
                } \
                return value; \
            } \
        }; \
        return sb()->bind<struct coro>()()(self, value); \
    }

#define SANDBOX_DEF_GFX_PROP_OBJ_VAL(S, V, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return sb()->bind<struct rb_iv_get>()()(self, #name); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE operator()(VALUE self, VALUE value) { \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_GUARD_L(get_private_data<S>(self)->set##prop(sb().e, *get_private_data<V>(value))); \
                } \
                return value; \
            } \
        }; \
        return sb()->bind<struct coro>()()(self, value); \
    }

#define SANDBOX_DEF_GRA_PROP_B(prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return SANDBOX_BOOL_TO_VALUE(shState->graphics().get##prop()); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        bool v = SANDBOX_VALUE_TO_BOOL(value); \
        GFX_LOCK; \
        shState->graphics().set##prop(v); \
        GFX_UNLOCK; \
        return value; \
    }

#define SANDBOX_DEF_GRA_PROP(V, num2val, val2num, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return sb()->bind<struct num2val>()()(shState->graphics().get##prop()); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE operator()(VALUE self, VALUE value) { \
                typedef decl_slots<V> slots; \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_S(0, val2num, value); \
                    GFX_LOCK; \
                    shState->graphics().set##prop(SANDBOX_SLOT(0)); \
                    GFX_UNLOCK; \
                } \
                return value; \
            } \
        }; \
        return sb()->bind<struct coro>()()(self, value); \
    }

#define SANDBOX_DEF_GRA_PROP_I(prop, name) SANDBOX_DEF_GRA_PROP(int32_t, rb_ll2inum, rb_num2int, prop, name)
#define SANDBOX_DEF_GRA_PROP_F(prop, name) SANDBOX_DEF_GRA_PROP(float, rb_float_new, rb_num2dbl, prop, name)
#define SANDBOX_DEF_GRA_PROP_D(prop, name) SANDBOX_DEF_GRA_PROP(double, rb_float_new, rb_num2dbl, prop, name)

#define SANDBOX_DEF_GRA_PROP_OBJ_REF(V, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return sb()->bind<struct rb_iv_get>()()(self, #name); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE operator()(VALUE self, VALUE value) { \
                BOOST_ASIO_CORO_REENTER (this) { \
                    { \
                        V *v = value == SANDBOX_NIL ? nullptr : get_private_data<V>(value); \
                        GFX_LOCK; \
                        shState->graphics().set##prop(v); \
                        GFX_UNLOCK; \
                    } \
                    SANDBOX_AWAIT(rb_iv_set, self, #name, value); \
                } \
                return value; \
            } \
        }; \
        return sb()->bind<struct coro>()()(self, value); \
    }

#define SANDBOX_DEF_GRA_PROP_OBJ_VAL(V, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return sb()->bind<struct rb_iv_get>()()(self, #name); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        V *v = get_private_data<V>(value); \
        GFX_LOCK; \
        shState->graphics().set##prop(*v); \
        GFX_UNLOCK; \
        return value; \
    }

#define SANDBOX_INIT_FUNC_PROP_BIND(func, target, name) do { \
    SANDBOX_AWAIT(func, target, #name, (VALUE (*)(ANYARGS))get_##name, 0); \
    SANDBOX_AWAIT(func, target, #name "=", (VALUE (*)(ANYARGS))set_##name, 1); \
} while (0)

#define SANDBOX_INIT_PROP_BIND(klass, name) SANDBOX_INIT_FUNC_PROP_BIND(rb_define_method, klass, name)
#define SANDBOX_INIT_SINGLETON_PROP_BIND(klass, name) SANDBOX_INIT_FUNC_PROP_BIND(rb_define_singleton_method, klass, name)
#define SANDBOX_INIT_MODULE_PROP_BIND(module, name) SANDBOX_INIT_FUNC_PROP_BIND(rb_define_module_function, module, name)

#define SANDBOX_GUARD_F(finalizer, ...) do { \
    using namespace ::mkxp_sandbox; \
    sb().e = Exception(); \
    __VA_ARGS__; \
    if (sb().e.is_error()) { \
        finalizer; \
        SANDBOX_AWAIT(exception_raise, sb().e); \
    } \
} while (0)

#define SANDBOX_GUARD(...) SANDBOX_GUARD_F(, __VA_ARGS__)
#define SANDBOX_GUARD_LF(finalizer, ...) do { GFX_LOCK; SANDBOX_GUARD_F(finalizer; GFX_UNLOCK, __VA_ARGS__); GFX_UNLOCK; } while (0)
#define SANDBOX_GUARD_L(...) SANDBOX_GUARD_LF(, __VA_ARGS__)

#define SANDBOX_TYPENUM_TYPES \
    (Bitmap) \
    (Color) \
    (Font) \
    (Plane) \
    (Rect) \
    (Sprite) \
    (Table) \
    (Tilemap) \
    (Tilemap::Autotiles) \
    (TilemapVX) \
    (TilemapVX::BitmapArray) \
    (Tone) \
    (Viewport) \
    (Window) \
    (WindowVX) \

#define SANDBOX_NUM_TYPENUMS BOOST_PP_SEQ_SIZE(SANDBOX_TYPENUM_TYPES)

#define _SANDBOX_DEF_GET_TYPENUM_DETAIL(T, num) template <> struct get_typenum<T> { \
    static_assert(num != 0, "typenum should not be 0"); \
    static_assert(num <= SANDBOX_NUM_TYPENUMS, "typenum should not be greater than the number of typenums"); \
    static constexpr wasm_size_t value = num; \
};
#define _SANDBOX_DEF_GET_TYPENUM(_r, _data, T) _SANDBOX_DEF_GET_TYPENUM_DETAIL(T, __COUNTER__ - _get_typenum_counter_start)

namespace mkxp_sandbox {
    template <typename T> struct get_typenum;
    static constexpr size_t _get_typenum_counter_start = __COUNTER__;
    BOOST_PP_SEQ_FOR_EACH(_SANDBOX_DEF_GET_TYPENUM, _, SANDBOX_TYPENUM_TYPES);

    // We need these helper functions so that the arguments to `SANDBOX_AWAIT`/`SANDBOX_AWAIT_R`/`SANDBOX_AWAIT_S` are evaluated before `sb()->bind` is called instead of after.
    // The reverse happening can lead to incorrect behaviour if one or more of the arguments is using `SANDBOX_SLOT` or other macros that need the state of the sandbox.
    template <typename Coroutine, typename... Args> bool _sandbox_await(Args... args) {
        struct bindings::stack_frame_guard<Coroutine> frame_guard = sb()->bind<Coroutine>();
        frame_guard()(args...);
        return frame_guard().is_complete();
    }

    template <typename Coroutine, typename Output, typename... Args> boost::optional<Output> _sandbox_await_r(Args... args) {
        struct bindings::stack_frame_guard<Coroutine> frame_guard = sb()->bind<Coroutine>();
        Output output = frame_guard()(args...);
        if (frame_guard().is_complete()) {
            return output;
        } else {
            return boost::none;
        }
    }

    // Given a Ruby object `val`, stores the C++ object `ptr` into the private data field of `val`.
    // You can set `ptr` to `nullptr` if you just want to destroy the current object in the private data field,
    // but note that calling `get_private_data` while the private data field is set to `nullptr` will trigger an abort.
    template <typename T> void set_private_data(VALUE val, T *ptr) {
        /* RGSS's behavior is to just leak memory if a disposable is reinitialized,
         * with the original disposable being left permanently instantiated,
         * but that's (1) bad, and (2) would currently cause memory access issues
         * when things like a sprite's src_rect inevitably get GC'd, so we're not
         * copying that. */

        wasm_objkey_t &key = sb()->ref<wasm_objkey_t>(sb()->rtypeddata_data(val));

        // Free the old value if it already exists (initialize called twice?)
        if (key != 0 && sb()->get_object(key) != ptr) {
            sb()->destroy_object(key);
        }

        key = ptr == nullptr ? 0 : sb()->create_object(get_typenum<T>::value, ptr);
    }

    // Given a Ruby object `val`, retrieves the C++ object in its private data field.
    // Aborts if the private data field hasn't ever been set, has been set to `nullptr` or has been set to an object of a different type,
    // so make sure it's been set properly using `set_private_data` beforehand.
    template <typename T> inline T *get_private_data(VALUE val) {
        wasm_objkey_t key = sb()->ref<wasm_ptr_t>(sb()->rtypeddata_data(val));
        if (!sb()->check_object_type(key, get_typenum<T>::value)) {
            std::abort();
        }
        return (T *)sb()->get_object(key);
    }

    void dfree(wasm_objkey_t key);

    // Gets the length of a Ruby object.
    struct get_length : boost::asio::coroutine {
        typedef decl_slots<ID, VALUE, wasm_size_t> slots;
        wasm_size_t operator()(VALUE obj);
    };

    // Gets the bytesize of a Ruby object.
    struct get_bytesize : boost::asio::coroutine {
        typedef decl_slots<ID, VALUE, wasm_size_t> slots;
        wasm_size_t operator()(VALUE obj);
    };

    struct wrap_property : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        template <typename T> VALUE operator()(VALUE self, T *ptr, const char *iv, VALUE klass) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_obj_alloc, klass);
                set_private_data(SANDBOX_SLOT(0), ptr);
                SANDBOX_AWAIT(rb_iv_set, self, iv, SANDBOX_SLOT(0));
            }

            return SANDBOX_SLOT(0);
        }
    };

    // Prints the backtrace of a Ruby exception to the log.
    struct log_backtrace : boost::asio::coroutine {
        typedef decl_slots<ID, VALUE, VALUE, wasm_ptr_t> slots;
        void operator()(VALUE exception);
    };

    extern VALUE mkxp_error_class;
    extern VALUE physfs_error_class;
    extern VALUE sdl_error_class;
    extern VALUE rgss_error_class;
    extern VALUE reset_class;
    extern VALUE enoent_class;

    struct exception_binding_init : boost::asio::coroutine {
        typedef decl_slots<ID> slots;
        void operator()();
    };

    struct exception_raise : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;
        void operator()(Exception &exception);
    };
}

#endif // MKXPZ_SANDBOX_BINDING_UTIL_H
