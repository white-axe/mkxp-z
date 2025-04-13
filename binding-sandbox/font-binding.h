/*
** font-binding.h
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

#ifndef MKXPZ_SANDBOX_FONT_BINDING_H
#define MKXPZ_SANDBOX_FONT_BINDING_H

#include "sandbox.h"
#include "binding-util.h"
#include "font.h"
#include "etc.h"
#include "sharedstate.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type font_type;

    SANDBOX_COROUTINE(font_binding_init,
        SANDBOX_DEF_ALLOC(font_type)
        SANDBOX_DEF_DFREE(Font)

        VALUE klass;
        ID default_color_id;
        VALUE default_color_klass;
        VALUE default_color_obj;
        ID default_out_color_id;
        VALUE default_out_color_klass;
        VALUE default_out_color_obj;
        VALUE default_names;
        VALUE default_name;
        wasm_size_t default_name_index;

        SANDBOX_COROUTINE(collect_strings,
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
        )

        static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Font *font;
                std::vector<std::string> *names;
                VALUE names_obj;
                int32_t size;
                ID id;
                VALUE klass;
                VALUE obj;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        names = new std::vector<std::string>;

                        if (argc == 0) {
                            SANDBOX_AWAIT_AND_SET(klass, rb_obj_class, self);
                            SANDBOX_AWAIT_AND_SET(names_obj, rb_iv_get, klass, "default_name");
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

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Color");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                        set_private_data(obj, &font->getColor());
                        SANDBOX_AWAIT(rb_iv_set, self, "color", obj);

                        if (rgssVer >= 3) {
                            SANDBOX_AWAIT_AND_SET(id, rb_intern, "Color");
                            SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                            SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                            set_private_data(obj, &font->getOutColor());
                            SANDBOX_AWAIT(rb_iv_set, self, "out_color", obj);
                        }
                    }

                    return SANDBOX_NIL;
                }

                ~coro() {
                    delete names;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE initialize_copy(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                Font *font;
                ID id;
                VALUE klass;
                VALUE obj;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        if (self != value) {
                            return self;
                        }

                        SANDBOX_AWAIT(rb_obj_init_copy, self, value);
                        font = new Font(*get_private_data<Font>(value));
                        set_private_data(self, font);

                        font->initDynAttribs();

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Color");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                        set_private_data(obj, &font->getColor());
                        SANDBOX_AWAIT(rb_iv_set, self, "color", obj);

                        if (rgssVer >= 3) {
                            SANDBOX_AWAIT_AND_SET(id, rb_intern, "Color");
                            SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                            SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                            set_private_data(obj, &font->getOutColor());
                            SANDBOX_AWAIT(rb_iv_set, self, "out_color", obj);
                        }
                    }

                    return self;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_name(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "name");
        }

        static VALUE set_name(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
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
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_size(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Font>(self)->getSize());
        }

        static VALUE set_size(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int32_t size;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(size, rb_num2int, value);
                        get_private_data<Font>(self)->setSize(size);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_bold(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(get_private_data<Font>(self)->getBold());
        }

        static VALUE set_bold(VALUE self, VALUE value) {
            get_private_data<Font>(self)->setBold(SANDBOX_VALUE_TO_BOOL(value));
            return value;
        }

        static VALUE get_italic(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(get_private_data<Font>(self)->getItalic());
        }

        static VALUE set_italic(VALUE self, VALUE value) {
            get_private_data<Font>(self)->setItalic(SANDBOX_VALUE_TO_BOOL(value));
            return value;
        }

        static VALUE get_color(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "color");
        }

        static VALUE set_color(VALUE self, VALUE value) {
            get_private_data<Font>(self)->setColor(*get_private_data<Color>(value));
            return value;
        }

        static VALUE get_shadow(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(get_private_data<Font>(self)->getShadow());
        }

        static VALUE set_shadow(VALUE self, VALUE value) {
            get_private_data<Font>(self)->setShadow(SANDBOX_VALUE_TO_BOOL(value));
            return value;
        }

        static VALUE get_outline(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(get_private_data<Font>(self)->getOutline());
        }

        static VALUE set_outline(VALUE self, VALUE value) {
            get_private_data<Font>(self)->setOutline(SANDBOX_VALUE_TO_BOOL(value));
            return value;
        }

        static VALUE get_out_color(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "out_color");
        }

        static VALUE set_out_color(VALUE self, VALUE value) {
            get_private_data<Font>(self)->setOutColor(*get_private_data<Color>(value));
            return value;
        }

        static VALUE get_default_name(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "default_name");
        }

        static VALUE set_default_name(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
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
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_default_size(VALUE self) {
            return Font::getDefaultSize();
        }

        static VALUE set_default_size(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int32_t size;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(size, rb_num2int, value);
                        Font::setDefaultSize(size);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_default_bold(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(Font::getDefaultBold());
        }

        static VALUE set_default_bold(VALUE self, VALUE value) {
            Font::setDefaultBold(SANDBOX_VALUE_TO_BOOL(value));
            return value;
        }

        static VALUE get_default_italic(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(Font::getDefaultItalic());
        }

        static VALUE set_default_italic(VALUE self, VALUE value) {
            Font::setDefaultItalic(SANDBOX_VALUE_TO_BOOL(value));
            return value;
        }

        static VALUE get_default_color(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "default_color");
        }

        static VALUE set_default_color(VALUE self, VALUE value) {
            get_private_data<Font>(self)->setDefaultColor(*get_private_data<Color>(value));
            return value;
        }

        static VALUE get_default_shadow(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(Font::getDefaultShadow());
        }

        static VALUE set_default_shadow(VALUE self, VALUE value) {
            Font::setDefaultShadow(SANDBOX_VALUE_TO_BOOL(value));
            return value;
        }

        static VALUE get_default_outline(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(Font::getDefaultOutline());
        }

        static VALUE set_default_outline(VALUE self, VALUE value) {
            Font::setDefaultOutline(SANDBOX_VALUE_TO_BOOL(value));
            return value;
        }

        static VALUE get_default_out_color(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "default_out_color");
        }

        static VALUE set_default_out_color(VALUE self, VALUE value) {
            get_private_data<Font>(self)->setDefaultColor(*get_private_data<Color>(value));
            return value;
        }

        static VALUE exist(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
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
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                font_type = sb()->rb_data_type("Font", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Font", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);

                Font::initDefaultDynAttribs();

                SANDBOX_AWAIT_AND_SET(default_color_id, rb_intern, "Color");
                SANDBOX_AWAIT_AND_SET(default_color_klass, rb_const_get, sb()->rb_cObject(), default_color_id);
                SANDBOX_AWAIT_AND_SET(default_color_obj, rb_obj_alloc, default_color_klass);
                set_private_data(default_color_obj, &Font::getDefaultColor());
                SANDBOX_AWAIT(rb_iv_set, klass, "default_color", default_color_obj);

                if (rgssVer >= 3) {
                    SANDBOX_AWAIT_AND_SET(default_out_color_id, rb_intern, "Color");
                    SANDBOX_AWAIT_AND_SET(default_out_color_klass, rb_const_get, sb()->rb_cObject(), default_out_color_id);
                    SANDBOX_AWAIT_AND_SET(default_out_color_obj, rb_obj_alloc, default_out_color_klass);
                    set_private_data(default_out_color_obj, &Font::getDefaultOutColor());
                    SANDBOX_AWAIT(rb_iv_set, klass, "default_out_color", default_out_color_obj);
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
                SANDBOX_AWAIT(rb_iv_set, klass, "default_name", default_names);

                SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "initialize_copy", (VALUE (*)(ANYARGS))initialize_copy, 1);

                SANDBOX_AWAIT(rb_define_method, klass, "name", (VALUE (*)(ANYARGS))get_name, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "name=", (VALUE (*)(ANYARGS))set_name, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "size", (VALUE (*)(ANYARGS))get_size, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "size=", (VALUE (*)(ANYARGS))set_size, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "bold", (VALUE (*)(ANYARGS))get_bold, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "bold=", (VALUE (*)(ANYARGS))set_bold, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "italic", (VALUE (*)(ANYARGS))get_italic, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "italic=", (VALUE (*)(ANYARGS))set_italic, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "color", (VALUE (*)(ANYARGS))get_color, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "color=", (VALUE (*)(ANYARGS))set_color, 1);

                if (rgssVer >= 2) {
                    SANDBOX_AWAIT(rb_define_method, klass, "shadow", (VALUE (*)(ANYARGS))get_shadow, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "shadow=", (VALUE (*)(ANYARGS))set_shadow, 1);
                }

                if (rgssVer >= 3) {
                    SANDBOX_AWAIT(rb_define_method, klass, "outline", (VALUE (*)(ANYARGS))get_outline, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "outline=", (VALUE (*)(ANYARGS))set_outline, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "out_color", (VALUE (*)(ANYARGS))get_out_color, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "out_color=", (VALUE (*)(ANYARGS))set_out_color, 1);
                }

                SANDBOX_AWAIT(rb_define_singleton_method, klass, "default_name", (VALUE (*)(ANYARGS))get_default_name, 0);
                SANDBOX_AWAIT(rb_define_singleton_method, klass, "default_name=", (VALUE (*)(ANYARGS))set_default_name, 1);
                SANDBOX_AWAIT(rb_define_singleton_method, klass, "default_size", (VALUE (*)(ANYARGS))get_default_size, 0);
                SANDBOX_AWAIT(rb_define_singleton_method, klass, "default_size=", (VALUE (*)(ANYARGS))set_default_size, 1);
                SANDBOX_AWAIT(rb_define_singleton_method, klass, "default_bold", (VALUE (*)(ANYARGS))get_default_bold, 0);
                SANDBOX_AWAIT(rb_define_singleton_method, klass, "default_bold=", (VALUE (*)(ANYARGS))set_default_bold, 1);
                SANDBOX_AWAIT(rb_define_singleton_method, klass, "default_italic", (VALUE (*)(ANYARGS))get_default_italic, 0);
                SANDBOX_AWAIT(rb_define_singleton_method, klass, "default_italic=", (VALUE (*)(ANYARGS))set_default_italic, 1);
                SANDBOX_AWAIT(rb_define_singleton_method, klass, "default_color", (VALUE (*)(ANYARGS))get_default_color, 0);
                SANDBOX_AWAIT(rb_define_singleton_method, klass, "default_color=", (VALUE (*)(ANYARGS))set_default_color, 1);

                if (rgssVer >= 2) {
                    SANDBOX_AWAIT(rb_define_method, klass, "default_shadow", (VALUE (*)(ANYARGS))get_default_shadow, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "default_shadow=", (VALUE (*)(ANYARGS))set_default_shadow, 1);
                }

                if (rgssVer >= 3) {
                    SANDBOX_AWAIT(rb_define_method, klass, "default_outline", (VALUE (*)(ANYARGS))get_default_outline, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "default_outline=", (VALUE (*)(ANYARGS))set_default_outline, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "default_out_color", (VALUE (*)(ANYARGS))get_default_out_color, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "default_out_color=", (VALUE (*)(ANYARGS))set_default_out_color, 1);
                }

                SANDBOX_AWAIT(rb_define_singleton_method, klass, "exist?", (VALUE (*)(ANYARGS))exist, 1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_FONT_BINDING_H
