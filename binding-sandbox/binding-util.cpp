/*
** binding-util.cpp
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

#include "binding-util.h"
#include "sharedstate.h"
#include <utility>
#include <boost/type_traits/is_detected.hpp>

using namespace mkxp_sandbox;

template <typename T> static typename std::enable_if<std::is_constructible<T>::value, void *>::type construct() {
    return new T;
}

template <typename T> static typename std::enable_if<!std::is_constructible<T>::value && std::is_constructible<T, Exception &>::value, void *>::type construct() {
    Exception e;
    T *obj = new T(e);
    if (e.is_ok()) {
        return obj;
    } else {
        delete obj;
        return nullptr;
    }
}

template <typename T> static void destroy(void *self) {
    if (self != nullptr) {
        delete (T *)self;
    }
}

template <typename T> static typename std::enable_if<std::is_base_of<Disposable, T>::value>::type dispose(void *self) {
    if (self != nullptr) {
        ((T *)self)->dispose();
    }
}

template <typename T> static typename std::enable_if<!std::is_base_of<Disposable, T>::value>::type dispose(void *self) {}

template <typename T> static typename std::enable_if<std::is_base_of<Disposable, T>::value, bool>::type disposed(void *self) {
    return self == nullptr || ((T *)self)->isDisposed();
}

template <typename T> static typename std::enable_if<!std::is_base_of<Disposable, T>::value, bool>::type disposed(void *self) {
    return self == nullptr;
}

template <typename T> static bool serialize(const void *self, void *&data, wasm_size_t &max_size) {
    return ((const T *)self)->sandbox_serialize(data, max_size);
}

template <typename T> static bool deserialize(void *self, const void *&data, wasm_size_t &max_size) {
    return ((T *)self)->sandbox_deserialize(data, max_size);
}

template <typename T> using deserialize_begin_declaration_with_is_new = decltype(std::declval<T *>()->sandbox_deserialize_begin(std::declval<bool>()));
template <typename T> using deserialize_begin_declaration_without_is_new = decltype(std::declval<T *>()->sandbox_deserialize_begin());

template <typename T> static typename std::enable_if<boost::is_detected<deserialize_begin_declaration_with_is_new, T>::value>::type deserialize_begin(void *self, bool is_new) {
    ((T *)self)->sandbox_deserialize_begin(is_new);
}

template <typename T> static typename std::enable_if<!boost::is_detected<deserialize_begin_declaration_with_is_new, T>::value && boost::is_detected<deserialize_begin_declaration_without_is_new, T>::value>::type deserialize_begin(void *self, bool is_new) {
    ((T *)self)->sandbox_deserialize_begin();
}

template <typename T> static typename std::enable_if<!boost::is_detected<deserialize_begin_declaration_with_is_new, T>::value && !boost::is_detected<deserialize_begin_declaration_without_is_new, T>::value>::type deserialize_begin(void *self, bool is_new) {}

template <typename T> using deserialize_end_declaration = decltype(std::declval<T *>()->sandbox_deserialize_end());

template <typename T> static typename std::enable_if<boost::is_detected<deserialize_end_declaration, T>::value>::type deserialize_end(void *self) {
    ((T *)self)->sandbox_deserialize_end();
}

template <typename T> static typename std::enable_if<!boost::is_detected<deserialize_end_declaration, T>::value>::type deserialize_end(void *self) {}

#define _SANDBOX_DEF_TYPENUM_TABLE_ENTRY(_r, _data, T) {construct<T>, destroy<T>, dispose<T>, disposed<T>, serialize<T>, deserialize<T>, deserialize_begin<T>, deserialize_end<T>},
extern const struct typenum_table_entry mkxp_sandbox::typenum_table[SANDBOX_NUM_TYPENUMS] = {BOOST_PP_SEQ_FOR_EACH(_SANDBOX_DEF_TYPENUM_TABLE_ENTRY, _, SANDBOX_TYPENUM_TYPES)};
extern const wasm_size_t mkxp_sandbox::typenum_table_size = SANDBOX_NUM_TYPENUMS;

void mkxp_sandbox::dfree(wasm_objkey_t key) {
    sb()->destroy_object(key);
}

wasm_size_t get_length::operator()(VALUE obj) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_S(0, rb_intern, "length");
        SANDBOX_AWAIT_S(1, rb_funcall, obj, SANDBOX_SLOT(0), 0);
        SANDBOX_AWAIT_S(2, rb_num2ulong, SANDBOX_SLOT(1));
    }

    return SANDBOX_SLOT(2);
}

wasm_size_t get_bytesize::operator()(VALUE obj) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_S(0, rb_intern, "bytesize");
        SANDBOX_AWAIT_S(1, rb_funcall, obj, SANDBOX_SLOT(0), 0);
        SANDBOX_AWAIT_S(2, rb_num2ulong, SANDBOX_SLOT(1));
    }

    return SANDBOX_SLOT(2);
}

void log_backtrace::operator()(VALUE exception) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT(rb_p, exception);
        SANDBOX_AWAIT_S(0, rb_intern, "backtrace");
        SANDBOX_AWAIT_S(1, rb_funcall, exception, SANDBOX_SLOT(0), 0);
        SANDBOX_AWAIT_S(0, rb_intern, "join");
        SANDBOX_AWAIT_S(2, rb_str_new_cstr, "\n\t");
        SANDBOX_AWAIT_S(1, rb_funcall, SANDBOX_SLOT(1), SANDBOX_SLOT(0), 1, SANDBOX_SLOT(2));
        SANDBOX_AWAIT_S(3, rb_string_value_cstr, &SANDBOX_SLOT(1));
        mkxp_retro::log_printf(RETRO_LOG_ERROR, "%s\n", sb()->str(SANDBOX_SLOT(3)));
    }
}

VALUE mkxp_sandbox::mkxp_error_class;
VALUE mkxp_sandbox::physfs_error_class;
VALUE mkxp_sandbox::sdl_error_class;
VALUE mkxp_sandbox::rgss_error_class;
VALUE mkxp_sandbox::reset_class;
VALUE mkxp_sandbox::enoent_class;

void exception_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_R(mkxp_error_class, rb_define_class, "MKXPError", sb()->rb_eException());
        SANDBOX_AWAIT_R(physfs_error_class, rb_define_class, "PHYSFSError", sb()->rb_eException());
        SANDBOX_AWAIT_R(sdl_error_class, rb_define_class, "SDLError", sb()->rb_eException());
        SANDBOX_AWAIT_R(rgss_error_class, rb_define_class, "RGSSError", sb()->rb_eStandardError());
        SANDBOX_AWAIT_R(reset_class, rb_define_class, rgssVer >= 3 ? "RGSSReset" : "Reset", sb()->rb_eStandardError());
        SANDBOX_AWAIT_S(0, rb_intern, "Errno");
        SANDBOX_AWAIT_R(enoent_class, rb_const_get, sb()->rb_cObject(), SANDBOX_SLOT(0));
        SANDBOX_AWAIT_S(0, rb_intern, "ENOENT");
        SANDBOX_AWAIT_R(enoent_class, rb_const_get, enoent_class, SANDBOX_SLOT(0));
    }
}

void exception_raise::operator()(Exception &exception) {
    BOOST_ASIO_CORO_REENTER (this) {
        if (exception.type == Exception::Ok) {
            return;
        }

        SANDBOX_AWAIT_S(0, rb_str_new_cstr, exception.msg.c_str());

        if (exception.type == Exception::RGSSError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), rgss_error_class);
        } else if (exception.type == Exception::Reset) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), reset_class);
        } else if (exception.type == Exception::NoFileError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), enoent_class);
        } else if (exception.type == Exception::IOError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sb()->rb_eIOError());
        } else if (exception.type == Exception::TypeError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sb()->rb_eTypeError());
        } else if (exception.type == Exception::ArgumentError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sb()->rb_eArgError());
        } else if (exception.type == Exception::SystemExit) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sb()->rb_eSystemExit());
        } else if (exception.type == Exception::RuntimeError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sb()->rb_eRuntimeError());
        } else if (exception.type == Exception::PHYSFSError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), physfs_error_class);
        } else if (exception.type == Exception::SDLError) {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), sdl_error_class);
        } else {
            SANDBOX_AWAIT_S(0, rb_class_new_instance, 1, &SANDBOX_SLOT(0), mkxp_error_class);
        }

        SANDBOX_AWAIT(rb_exc_raise, SANDBOX_SLOT(0));
    }
}
