/*
** font-binding.cpp
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

#include "font-binding.h"
#include "etc-binding.h"
#include "font.h"
#include "sharedstate.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::font_class;
static struct bindings::rb_data_type font_type;

SANDBOX_DEF_ALLOC(font_type);
SANDBOX_DEF_DFREE(Font);

struct collect_strings : boost::asio::coroutine {
    VALUE value;
    VALUE entry;
    wasm_ptr_t str;
    wasm_size_t i;
    wasm_size_t length;

    void operator()(VALUE obj, std::vector<std::string> &out) {
        BOOST_ASIO_CORO_REENTER (this) {
            SANDBOX_AWAIT_AND_SET(value, rb_obj_is_kind_of, obj, sb()->rb_cString());
            if (SANDBOX_VALUE_TO_BOOL(value)) {
                SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &obj);
                out.push_back((const char *)(**sb() + str));
            } else {
                SANDBOX_AWAIT_AND_SET(value, rb_obj_is_kind_of, obj, sb()->rb_cArray());
                if (SANDBOX_VALUE_TO_BOOL(value)) {
                    SANDBOX_AWAIT_AND_SET(value, get_length, obj);
                    SANDBOX_AWAIT_AND_SET(length, rb_num2ulong, value);
                    for (i = 0; i < length; ++i) {
                        SANDBOX_AWAIT_AND_SET(entry, rb_ary_entry, obj, i);
                        /* Non-string objects are tolerated (ignored) */
                        SANDBOX_AWAIT_AND_SET(value, rb_obj_is_kind_of, entry, sb()->rb_cString());
                        if (SANDBOX_VALUE_TO_BOOL(value)) {
                            SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &entry);
                            out.push_back((const char *)(**sb() + str));
                        }
                    }
                }
            }
        }
    }
};

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        Font *font;
        std::vector<std::string> *names;
        VALUE names_obj;
        int32_t size;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                names = new std::vector<std::string>;

                if (argc == 0) {
                    SANDBOX_AWAIT_AND_SET(names_obj, rb_iv_get, font_class, "default_name");
                    font = new Font();
                } else if (argc == 1) {
                    names_obj = ((VALUE *)(**sb() + argv))[0];
                    SANDBOX_AWAIT(collect_strings, names_obj, *names);
                    font = new Font(names);
                } else {
                    names_obj = ((VALUE *)(**sb() + argv))[0];
                    SANDBOX_AWAIT(collect_strings, names_obj, *names);
                    font = new Font(names, size);
                }

                set_private_data(self, font);

                font->initDynAttribs();

                /* This is semantically wrong; the new Font object should take
                 * a dup'ed object here in case of an array. Ditto for the setters.
                 * However the same bug/behavior exists in all RM versions. */
                SANDBOX_AWAIT(rb_iv_set, self, "name", names_obj);

                SANDBOX_AWAIT(wrap_property, self, &font->getColor(), "color", color_class);

                if (rgssVer >= 3) {
                    SANDBOX_AWAIT(wrap_property, self, &font->getOutColor(), "out_color", color_class);
                }
            }

            return SANDBOX_NIL;
        }

        ~coro() {
            delete names;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE initialize_copy(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        Font *font;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (self != value) {
                    return self;
                }

                SANDBOX_AWAIT(rb_obj_init_copy, self, value);
                font = new Font(*get_private_data<Font>(value));
                set_private_data(self, font);

                font->initDynAttribs();

                SANDBOX_AWAIT(wrap_property, self, &font->getColor(), "color", color_class);

                if (rgssVer >= 3) {
                    SANDBOX_AWAIT(wrap_property, self, &font->getOutColor(), "out_color", color_class);
                }
            }

            return self;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE get_name(VALUE self) {
    return sb()->bind<struct rb_iv_get>()()(self, "name");
}

static VALUE set_name(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        std::vector<std::string> *names;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                names = new std::vector<std::string>;
                SANDBOX_AWAIT(collect_strings, value, *names);
                get_private_data<Font>(self)->setName(*names);
                SANDBOX_AWAIT(rb_iv_set, self, "name", value);
            }

            return value;
        }

        ~coro() {
            delete names;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

SANDBOX_DEF_PROP_I(Font, Size, size);
SANDBOX_DEF_PROP_B(Font, Bold, bold);
SANDBOX_DEF_PROP_B(Font, Italic, italic);
SANDBOX_DEF_PROP_OBJ_VAL(Font, Color, Color, color);
SANDBOX_DEF_PROP_B(Font, Shadow, shadow);
SANDBOX_DEF_PROP_B(Font, Outline, outline);
SANDBOX_DEF_PROP_OBJ_VAL(Font, Color, OutColor, out_color);

static VALUE get_default_name(VALUE self) {
    return sb()->bind<struct rb_iv_get>()()(self, "default_name");
}

static VALUE set_default_name(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        std::vector<std::string> *names;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                names = new std::vector<std::string>;
                SANDBOX_AWAIT(collect_strings, value, *names);
                get_private_data<Font>(self)->setName(*names);
                SANDBOX_AWAIT(rb_iv_set, self, "default_name", value);
            }

            return value;
        }

        ~coro() {
            delete names;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

SANDBOX_DEF_CLASS_PROP_I(Font, DefaultSize, default_size);
SANDBOX_DEF_CLASS_PROP_B(Font, DefaultBold, default_bold);
SANDBOX_DEF_CLASS_PROP_B(Font, DefaultItalic, default_italic);
SANDBOX_DEF_CLASS_PROP_OBJ_VAL(Font, Color, DefaultColor, default_color);
SANDBOX_DEF_CLASS_PROP_B(Font, DefaultShadow, default_shadow);
SANDBOX_DEF_CLASS_PROP_B(Font, DefaultOutline, default_outline);
SANDBOX_DEF_CLASS_PROP_OBJ_VAL(Font, Color, DefaultOutColor, default_out_color);

static VALUE exist(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        wasm_ptr_t str;
        VALUE is_string;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(is_string, rb_obj_is_kind_of, value, sb()->rb_cString());
                if (SANDBOX_VALUE_TO_BOOL(is_string)) {
                    SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &value);
                    return SANDBOX_BOOL_TO_VALUE(Font::doesExist((const char *)(**sb() + str)));
                } else {
                    return SANDBOX_BOOL_TO_VALUE(Font::doesExist(NULL));
                }
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

void font_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        font_type = sb()->rb_data_type("Font", NULL, dfree, NULL, NULL, 0, 0, 0);
        SANDBOX_AWAIT_AND_SET(font_class, rb_define_class, "Font", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, font_class, alloc);

        Font::initDefaultDynAttribs();

        SANDBOX_AWAIT(wrap_property, font_class, &Font::getDefaultColor(), "default_color", color_class);

        if (rgssVer >= 3) {
            SANDBOX_AWAIT(wrap_property, font_class, &Font::getDefaultOutColor(), "default_out_color", color_class);
        }

        if (Font::getInitialDefaultNames().size() == 1) {
            SANDBOX_AWAIT_AND_SET(default_names, rb_utf8_str_new_cstr, Font::getInitialDefaultNames()[0].c_str());
        } else {
            SANDBOX_AWAIT_AND_SET(default_names, rb_ary_new_capa, Font::getInitialDefaultNames().size());
            for (default_name_index = 0; default_name_index < Font::getInitialDefaultNames().size(); ++default_name_index) {
                SANDBOX_AWAIT_AND_SET(default_name, rb_utf8_str_new_cstr, Font::getInitialDefaultNames()[default_name_index].c_str());
                SANDBOX_AWAIT(rb_ary_push, default_names, default_name);
            }
        }
        SANDBOX_AWAIT(rb_iv_set, font_class, "default_name", default_names);

        SANDBOX_AWAIT(rb_define_method, font_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(rb_define_method, font_class, "initialize_copy", (VALUE (*)(ANYARGS))initialize_copy, 1);

        SANDBOX_INIT_PROP_BIND(font_class, name);
        SANDBOX_INIT_PROP_BIND(font_class, size);
        SANDBOX_INIT_PROP_BIND(font_class, bold);
        SANDBOX_INIT_PROP_BIND(font_class, italic);
        SANDBOX_INIT_PROP_BIND(font_class, color);

        if (rgssVer >= 2) {
            SANDBOX_INIT_PROP_BIND(font_class, shadow);
        }

        if (rgssVer >= 3) {
            SANDBOX_INIT_PROP_BIND(font_class, outline);
            SANDBOX_INIT_PROP_BIND(font_class, out_color);
        }

        SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_name);
        SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_size);
        SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_bold);
        SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_italic);
        SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_color);

        if (rgssVer >= 2) {
            SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_shadow);
        }

        if (rgssVer >= 3) {
            SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_outline);
            SANDBOX_INIT_SINGLETON_PROP_BIND(font_class, default_out_color);
        }

        SANDBOX_AWAIT(rb_define_singleton_method, font_class, "exist?", (VALUE (*)(ANYARGS))exist, 1);
    }
}
