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

#include "core.h"
#include "sandbox.h"

#define GFX_GUARD_EXC(exp) exp

#define SANDBOX_DEF_ALLOC(rbtype) \
    static VALUE alloc(VALUE _klass) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE _obj; \
            VALUE operator()(VALUE _klass) { \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_AND_SET(_obj, rb_data_typed_object_wrap, _klass, 0, rbtype); \
                } \
                return _obj; \
            } \
        }; \
        return sb()->bind<struct coro>()()(_klass); \
    }

#define SANDBOX_DEF_ALLOC_WITH_INIT(rbtype, initializer) \
    static VALUE alloc(VALUE _klass) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE _obj; \
            VALUE operator()(VALUE _klass) { \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_AND_SET(_obj, rb_data_typed_object_wrap, _klass, 0, rbtype); \
                    set_private_data(_obj, initializer); /* TODO: free when sandbox is deallocated */ \
                } \
                return _obj; \
            } \
        }; \
        return sb()->bind<struct coro>()()(_klass); \
    }

#define SANDBOX_DEF_DFREE(T) \
    static void dfree(wasm_ptr_t _buf) { \
        using namespace ::mkxp_sandbox; \
        delete *(T **)(**sb() + _buf); \
        sb()->sandbox_free(_buf); \
    }

#define SANDBOX_DEF_LOAD(T) \
    static VALUE load(VALUE _self, VALUE _serialized) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE _obj; \
            wasm_ptr_t _ptr; \
            wasm_size_t _len; \
            VALUE operator()(VALUE _self, VALUE _serialized) { \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_AND_SET(_obj, rb_obj_alloc, _self); \
                    SANDBOX_AWAIT_AND_SET(_ptr, rb_string_value_ptr, &_serialized); \
                    SANDBOX_AWAIT_AND_SET(_len, get_bytesize, _serialized); \
                    set_private_data(_obj, T::deserialize((const char *)(**sb() + _ptr), _len)); /* TODO: free when sandbox is deallocated */ \
                } \
                return _obj; \
            } \
        }; \
        return sb()->bind<struct coro>()()(_self, _serialized); \
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
                V v; \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_AND_SET(v, val2num, value); \
                    S::set##prop(v); \
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
                V v; \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_AND_SET(v, val2num, value); \
                    S *s = get_private_data<S>(self); \
                    s->set##prop(v); \
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
        S *s = get_private_data<S>(self); \
        return SANDBOX_BOOL_TO_VALUE(s->get##prop()); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        S *s = get_private_data<S>(self); \
        bool v = SANDBOX_VALUE_TO_BOOL(value); \
        GFX_GUARD_EXC(s->set##prop(v);); \
        return value; \
    }

#define SANDBOX_DEF_GFX_PROP(V, num2val, val2num, S, prop, name) \
    static VALUE get_##name(VALUE self) { \
        using namespace ::mkxp_sandbox; \
        return sb()->bind<struct num2val>()()(get_private_data<S>(self)->get##prop()); \
    } \
    static VALUE set_##name(VALUE self, VALUE value) { \
        using namespace ::mkxp_sandbox; \
        struct coro : boost::asio::coroutine { \
            VALUE operator()(VALUE self, VALUE value) { \
                V v; \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_AND_SET(v, val2num, value); \
                    S *s = get_private_data<S>(self); \
                    GFX_GUARD_EXC(s->set##prop(v);); \
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
                    { \
                        S *s = get_private_data<S>(self); \
                        V *v = value == SANDBOX_NIL ? nullptr : get_private_data<V>(value); \
                        GFX_GUARD_EXC(s->set##prop(v);); \
                    } \
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
        S *s = get_private_data<S>(self); \
        V *v = get_private_data<V>(value); \
        GFX_GUARD_EXC(s->set##prop(*v);); \
        return value; \
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
                V v; \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_AND_SET(v, val2num, value); \
                    GFX_LOCK; \
                    shState->graphics().set##prop(v); \
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

namespace mkxp_sandbox {
    // Given Ruby typed data `obj`, stores `ptr` into the private data field of `obj`.
    void set_private_data(VALUE obj, void *ptr);

    // Given Ruby typed data `obj`, retrieves the private data field of `obj`.
    template <typename T> inline T *get_private_data(VALUE obj) {
        return *(T **)(**sb() + *(wasm_ptr_t *)(**sb() + sb()->rtypeddata_data(obj)));
    }

    // Gets the length of a Ruby object.
    struct get_length : boost::asio::coroutine {
        ID id;
        VALUE length_value;
        wasm_size_t result;

        wasm_size_t operator()(VALUE obj) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(id, rb_intern, "length");
                SANDBOX_AWAIT_AND_SET(length_value, rb_funcall, obj, id, 0);
                SANDBOX_AWAIT_AND_SET(result, rb_num2ulong, length_value);
            }

            return result;
        }
    };

    // Gets the bytesize of a Ruby object.
    struct get_bytesize : boost::asio::coroutine {
        ID id;
        VALUE length_value;
        wasm_size_t result;

        wasm_size_t operator()(VALUE obj) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(id, rb_intern, "bytesize");
                SANDBOX_AWAIT_AND_SET(length_value, rb_funcall, obj, id, 0);
                SANDBOX_AWAIT_AND_SET(result, rb_num2ulong, length_value);
            }

            return result;
        }
    };

    struct wrap_property : boost::asio::coroutine {
        VALUE obj;

        VALUE operator()(VALUE self, void *ptr, const char *iv, VALUE klass) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                set_private_data(obj, ptr);
                SANDBOX_AWAIT(rb_iv_set, self, iv, obj);
            }

            return obj;
        }
    };

    // Prints the backtrace of a Ruby exception to the log.
    struct log_backtrace : boost::asio::coroutine {
        ID id;
        VALUE backtrace;
        VALUE separator;
        wasm_ptr_t backtrace_str;

        void operator()(VALUE exception) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(rb_p, exception);
                SANDBOX_AWAIT_AND_SET(id, rb_intern, "backtrace");
                SANDBOX_AWAIT_AND_SET(backtrace, rb_funcall, exception, id, 0);
                SANDBOX_AWAIT_AND_SET(id, rb_intern, "join");
                SANDBOX_AWAIT_AND_SET(separator, rb_str_new_cstr, "\n\t");
                SANDBOX_AWAIT_AND_SET(backtrace, rb_funcall, backtrace, id, 1, separator);
                SANDBOX_AWAIT_AND_SET(backtrace_str, rb_string_value_cstr, &backtrace);
                mkxp_retro::log_printf(RETRO_LOG_ERROR, "%s\n", **sb() + backtrace_str);
            }
        }
    };
}

#endif // MKXPZ_SANDBOX_BINDING_UTIL_H
