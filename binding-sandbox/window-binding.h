/*
** window-binding.h
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

#ifndef MKXPZ_SANDBOX_WINDOW_BINDING_H
#define MKXPZ_SANDBOX_WINDOW_BINDING_H

#include "sandbox.h"
#include "binding-util.h"
#include "window.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type window_type;

    SANDBOX_COROUTINE(window_binding_init,
        SANDBOX_DEF_ALLOC(window_type)
        SANDBOX_DEF_DFREE(Window)

        static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Window *window;
                VALUE viewport_obj;
                Viewport *viewport;
                VALUE klass;
                VALUE cursor_obj;
                ID id;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        viewport_obj = SANDBOX_NIL;
                        viewport = NULL;
                        if (argc > 0) {
                            viewport_obj = *(VALUE *)(**sb() + argv);
                            if (viewport_obj != SANDBOX_NIL) {
                                viewport = get_private_data<Viewport>(viewport_obj);
                            }
                        }

                        GFX_LOCK
                        window = new Window(viewport);
                        SANDBOX_AWAIT(rb_iv_set, self, "viewport", viewport_obj);

                        set_private_data(self, window);
                        window->initDynAttribs();
                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Rect");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(cursor_obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(cursor_obj, &window->getCursorRect());
                        SANDBOX_AWAIT(rb_iv_set, self, "cursor_rect", cursor_obj);
                        GFX_UNLOCK
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE dispose(VALUE self) {
            Window *window = get_private_data<Window>(self);
            if (window != NULL) {
                window->dispose();
            }
            return SANDBOX_NIL;
        }

        static VALUE disposed(VALUE self) {
            Window *window = get_private_data<Window>(self);
            return window == NULL || window->isDisposed() ? SANDBOX_TRUE : SANDBOX_FALSE;
        }

        static VALUE update(VALUE self) {
            GFX_GUARD_EXC(get_private_data<Window>(self)->update();)
            return SANDBOX_NIL;
        }

        static VALUE get_contents(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "contents");
        }

        static VALUE set_contents(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<Window>(self)->setContents(value == SANDBOX_NIL ? NULL : get_private_data<Bitmap>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "contents", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_stretch(VALUE self) {
            return get_private_data<Window>(self)->getStretch() ? SANDBOX_TRUE : SANDBOX_FALSE;
        }

        static VALUE set_stretch(VALUE self, VALUE value) {
            GFX_GUARD_EXC(get_private_data<Window>(self)->setStretch(value != SANDBOX_FALSE && value != SANDBOX_NIL);)
            return value;
        }

        static VALUE get_cursor_rect(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "cursor_rect");
        }

        static VALUE set_cursor_rect(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<Window>(self)->setCursorRect(*get_private_data<Rect>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "cursor_rect", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_active(VALUE self) {
            return get_private_data<Window>(self)->getActive() ? SANDBOX_TRUE : SANDBOX_FALSE;
        }

        static VALUE set_active(VALUE self, VALUE value) {
            GFX_GUARD_EXC(get_private_data<Window>(self)->setActive(value != SANDBOX_FALSE && value != SANDBOX_NIL);)
            return value;
        }

        static VALUE get_visible(VALUE self) {
            return get_private_data<Window>(self)->getVisible() ? SANDBOX_TRUE : SANDBOX_FALSE;
        }

        static VALUE set_visible(VALUE self, VALUE value) {
            GFX_GUARD_EXC(get_private_data<Window>(self)->setVisible(value != SANDBOX_FALSE && value != SANDBOX_NIL));
            return value;
        }

        static VALUE get_pause(VALUE self) {
            return get_private_data<Window>(self)->getPause() ? SANDBOX_TRUE : SANDBOX_FALSE;
        }

        static VALUE set_pause(VALUE self, VALUE value) {
            GFX_GUARD_EXC(get_private_data<Window>(self)->setPause(value != SANDBOX_FALSE && value != SANDBOX_NIL));
            return value;
        }

        static VALUE get_x(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Window>(self)->getX());
        }

        static VALUE set_x(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int x;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(x, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Window>(self)->setX(x));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_y(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Window>(self)->getY());
        }

        static VALUE set_y(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int y;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(y, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Window>(self)->setY(y));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_width(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Window>(self)->getWidth());
        }

        static VALUE set_width(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int width;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(width, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Window>(self)->setWidth(width));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_height(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Window>(self)->getHeight());
        }

        static VALUE set_height(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int height;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(height, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Window>(self)->setHeight(height));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_ox(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Window>(self)->getOX());
        }

        static VALUE set_ox(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int ox;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(ox, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Window>(self)->setOX(ox));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_oy(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Window>(self)->getOY());
        }

        static VALUE set_oy(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int oy;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(oy, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Window>(self)->setOY(oy));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_z(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Window>(self)->getZ());
        }

        static VALUE set_z(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int z;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(z, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Window>(self)->setZ(z));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_opacity(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Window>(self)->getOpacity());
        }

        static VALUE set_opacity(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int opacity;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(opacity, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Window>(self)->setOpacity(opacity));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_back_opacity(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Window>(self)->getBackOpacity());
        }

        static VALUE set_back_opacity(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int back_opacity;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(back_opacity, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Window>(self)->setBackOpacity(back_opacity));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_contents_opacity(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Window>(self)->getContentsOpacity());
        }

        static VALUE set_contents_opacity(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int contents_opacity;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(contents_opacity, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Window>(self)->setContentsOpacity(contents_opacity));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE todo(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return SANDBOX_NIL;
        }

        VALUE klass;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                window_type = sb()->rb_data_type("Window", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Window", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "update", (VALUE (*)(ANYARGS))update, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "dispose", (VALUE (*)(ANYARGS))dispose, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "disposed?", (VALUE (*)(ANYARGS))disposed, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "windowskin=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "contents", (VALUE (*)(ANYARGS))get_contents, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "contents=", (VALUE (*)(ANYARGS))set_contents, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "stretch", (VALUE (*)(ANYARGS))get_stretch, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "stretch=", (VALUE (*)(ANYARGS))set_stretch, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "cursor_rect", (VALUE (*)(ANYARGS))get_cursor_rect, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "cursor_rect=", (VALUE (*)(ANYARGS))set_cursor_rect, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "active", (VALUE (*)(ANYARGS))get_active, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "active=", (VALUE (*)(ANYARGS))set_active, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "visible", (VALUE (*)(ANYARGS))get_visible, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "visible=", (VALUE (*)(ANYARGS))set_visible, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "pause", (VALUE (*)(ANYARGS))get_pause, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "pause=", (VALUE (*)(ANYARGS))set_pause, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "x", (VALUE (*)(ANYARGS))get_x, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "x=", (VALUE (*)(ANYARGS))set_x, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "y", (VALUE (*)(ANYARGS))get_y, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "y=", (VALUE (*)(ANYARGS))set_y, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "width", (VALUE (*)(ANYARGS))get_width, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "width=", (VALUE (*)(ANYARGS))set_width, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "height", (VALUE (*)(ANYARGS))get_height, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "height=", (VALUE (*)(ANYARGS))set_height, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "ox", (VALUE (*)(ANYARGS))get_ox, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "ox=", (VALUE (*)(ANYARGS))set_ox, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "oy", (VALUE (*)(ANYARGS))get_oy, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "oy=", (VALUE (*)(ANYARGS))set_oy, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "opacity", (VALUE (*)(ANYARGS))get_opacity, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "opacity=", (VALUE (*)(ANYARGS))set_opacity, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "back_opacity", (VALUE (*)(ANYARGS))get_back_opacity, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "back_opacity=", (VALUE (*)(ANYARGS))set_back_opacity, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "contents_opacity", (VALUE (*)(ANYARGS))get_contents_opacity, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "contents_opacity=", (VALUE (*)(ANYARGS))set_contents_opacity, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "z", (VALUE (*)(ANYARGS))get_z, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "z=", (VALUE (*)(ANYARGS))set_z, 1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_WINDOW_BINDING_H
