/*
** windowvx-binding.h
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

#ifndef MKXPZ_SANDBOX_WINDOWVX_BINDING_H
#define MKXPZ_SANDBOX_WINDOWVX_BINDING_H

#include "sandbox.h"
#include "binding-util.h"
#include "bitmap-binding.h"
#include "windowvx.h"
#include "bitmap.h"
#include "etc.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type windowvx_type;

    SANDBOX_COROUTINE(windowvx_binding_init,
        SANDBOX_DEF_ALLOC(windowvx_type)
        SANDBOX_DEF_DFREE(WindowVX)

        static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                WindowVX *window;
                VALUE viewport_obj;
                Viewport *viewport;
                VALUE klass;
                VALUE obj;
                ID id;
                Bitmap *contents;
                int32_t x;
                int32_t y;
                int32_t w;
                int32_t h;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        x = y = w = h = 0;
                        viewport_obj = SANDBOX_NIL;
                        viewport = NULL;

                        GFX_LOCK

                        if (rgssVer >= 3) {
                            if (argc == 4) {
                                SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                                SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                                SANDBOX_AWAIT_AND_SET(w, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                                SANDBOX_AWAIT_AND_SET(h, rb_num2int, ((VALUE *)(**sb() + argv))[3]);
                            }
                            window = new WindowVX(x, y, w, h);
                        } else {
                            if (argc > 0) {
                                viewport_obj = *(VALUE *)(**sb() + argv);
                                if (viewport_obj != SANDBOX_NIL) {
                                    viewport = get_private_data<Viewport>(viewport_obj);
                                }
                            }
                            window = new WindowVX(viewport);
                            SANDBOX_AWAIT(rb_iv_set, self, "viewport", viewport_obj);
                        }

                        set_private_data(self, window);
                        window->initDynAttribs();

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Rect");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                        set_private_data(obj, &window->getCursorRect());
                        SANDBOX_AWAIT(rb_iv_set, self, "cursor_rect", obj);

                        if (rgssVer >= 3) {
                            SANDBOX_AWAIT_AND_SET(id, rb_intern, "Tone");
                            SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                            SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                            set_private_data(obj, &window->getTone());
                            SANDBOX_AWAIT(rb_iv_set, self, "tone", obj);
                        }

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Bitmap");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                        contents = new Bitmap(1, 1);
                        set_private_data(obj, contents);
                        SANDBOX_AWAIT(bitmap_init_props, contents, obj);
                        SANDBOX_AWAIT(rb_iv_set, self, "contents", obj);

                        GFX_UNLOCK
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE dispose(VALUE self) {
            WindowVX *window = get_private_data<WindowVX>(self);
            if (window != NULL) {
                window->dispose();
            }
            return SANDBOX_NIL;
        }

        static VALUE disposed(VALUE self) {
            WindowVX *window = get_private_data<WindowVX>(self);
            return SANDBOX_BOOL_TO_VALUE(window == NULL || window->isDisposed());
        }

        static VALUE update(VALUE self) {
            GFX_GUARD_EXC(get_private_data<WindowVX>(self)->update();)
            return SANDBOX_NIL;
        }

        static VALUE get_viewport(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "viewport");
        }

        static VALUE set_viewport(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setViewport(get_private_data<Viewport>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "viewport", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_windowskin(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "windowskin");
        }

        static VALUE set_windowskin(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setWindowskin(value == SANDBOX_NIL ? NULL : get_private_data<Bitmap>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "windowskin", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_contents(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "contents");
        }

        static VALUE set_contents(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setContents(value == SANDBOX_NIL ? NULL : get_private_data<Bitmap>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "contents", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_cursor_rect(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "cursor_rect");
        }

        static VALUE set_cursor_rect(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setCursorRect(*get_private_data<Rect>(value)));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_active(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(get_private_data<WindowVX>(self)->getActive());
        }

        static VALUE set_active(VALUE self, VALUE value) {
            GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setActive(SANDBOX_VALUE_TO_BOOL(value));)
            return value;
        }

        static VALUE get_visible(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(get_private_data<WindowVX>(self)->getVisible());
        }

        static VALUE set_visible(VALUE self, VALUE value) {
            GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setVisible(SANDBOX_VALUE_TO_BOOL(value)));
            return value;
        }

        static VALUE get_pause(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(get_private_data<WindowVX>(self)->getPause());
        }

        static VALUE set_pause(VALUE self, VALUE value) {
            GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setPause(SANDBOX_VALUE_TO_BOOL(value)));
            return value;
        }

        static VALUE get_x(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<WindowVX>(self)->getX());
        }

        static VALUE set_x(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int x;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(x, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setX(x));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_y(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<WindowVX>(self)->getY());
        }

        static VALUE set_y(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int y;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(y, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setY(y));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_width(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<WindowVX>(self)->getWidth());
        }

        static VALUE set_width(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int width;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(width, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setWidth(width));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_height(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<WindowVX>(self)->getHeight());
        }

        static VALUE set_height(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int height;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(height, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setHeight(height));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_ox(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<WindowVX>(self)->getOX());
        }

        static VALUE set_ox(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int ox;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(ox, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setOX(ox));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_oy(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<WindowVX>(self)->getOY());
        }

        static VALUE set_oy(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int oy;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(oy, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setOY(oy));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_z(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<WindowVX>(self)->getZ());
        }

        static VALUE set_z(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int z;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(z, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setZ(z));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_opacity(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<WindowVX>(self)->getOpacity());
        }

        static VALUE set_opacity(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int opacity;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(opacity, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setOpacity(opacity));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_back_opacity(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<WindowVX>(self)->getBackOpacity());
        }

        static VALUE set_back_opacity(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int back_opacity;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(back_opacity, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setBackOpacity(back_opacity));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_contents_opacity(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<WindowVX>(self)->getContentsOpacity());
        }

        static VALUE set_contents_opacity(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int contents_opacity;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(contents_opacity, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setContentsOpacity(contents_opacity));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_openness(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<WindowVX>(self)->getOpenness());
        }

        static VALUE set_openness(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int openness;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(openness, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setOpenness(openness));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE move(VALUE self, VALUE xv, VALUE yv, VALUE wv, VALUE hv) {
            SANDBOX_COROUTINE(coro,
                WindowVX *window;
                int32_t x;
                int32_t y;
                int32_t w;
                int32_t h;

                VALUE operator()(VALUE self, VALUE xv, VALUE yv, VALUE wv, VALUE hv) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(x, rb_num2int, xv);
                        SANDBOX_AWAIT_AND_SET(y, rb_num2int, yv);
                        SANDBOX_AWAIT_AND_SET(w, rb_num2int, wv);
                        SANDBOX_AWAIT_AND_SET(h, rb_num2int, hv);
                        GFX_LOCK;
                        get_private_data<WindowVX>(self)->move(x, y, w, h);
                        GFX_UNLOCK;
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(self, xv, yv, wv, hv);
        }

        static VALUE is_open(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(get_private_data<WindowVX>(self)->isOpen());
        }

        static VALUE is_closed(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(get_private_data<WindowVX>(self)->isClosed());
        }

        static VALUE get_arrows_visible(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(get_private_data<WindowVX>(self)->getArrowsVisible());
        }

        static VALUE set_arrows_visible(VALUE self, VALUE value) {
            GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setArrowsVisible(SANDBOX_VALUE_TO_BOOL(value)));
            return value;
        }

        static VALUE get_padding(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<WindowVX>(self)->getPadding());
        }

        static VALUE set_padding(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int padding;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(padding, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setPadding(padding));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_padding_bottom(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<WindowVX>(self)->getPaddingBottom());
        }

        static VALUE set_padding_bottom(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int padding_bottom;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(padding_bottom, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setPaddingBottom(padding_bottom));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_tone(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "tone");
        }

        static VALUE set_tone(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<WindowVX>(self)->setTone(*get_private_data<Tone>(value)));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        VALUE klass;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                windowvx_type = sb()->rb_data_type("Window", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Window", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "update", (VALUE (*)(ANYARGS))update, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "dispose", (VALUE (*)(ANYARGS))dispose, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "disposed?", (VALUE (*)(ANYARGS))disposed, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "viewport", (VALUE (*)(ANYARGS))get_viewport, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "viewport=", (VALUE (*)(ANYARGS))set_viewport, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "windowskin", (VALUE (*)(ANYARGS))get_windowskin, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "windowskin=", (VALUE (*)(ANYARGS))set_windowskin, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "contents", (VALUE (*)(ANYARGS))get_contents, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "contents=", (VALUE (*)(ANYARGS))set_contents, 1);
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
                SANDBOX_AWAIT(rb_define_method, klass, "openness", (VALUE (*)(ANYARGS))get_openness, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "openness=", (VALUE (*)(ANYARGS))set_openness, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "z", (VALUE (*)(ANYARGS))get_z, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "z=", (VALUE (*)(ANYARGS))set_z, 1);

                if (rgssVer >= 3) {
                    SANDBOX_AWAIT(rb_define_method, klass, "move", (VALUE (*)(ANYARGS))move, 4);
                    SANDBOX_AWAIT(rb_define_method, klass, "open?", (VALUE (*)(ANYARGS))is_open, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "close?", (VALUE (*)(ANYARGS))is_closed, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "arrows_visible", (VALUE (*)(ANYARGS))get_arrows_visible, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "arrows_visible=", (VALUE (*)(ANYARGS))set_arrows_visible, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "padding", (VALUE (*)(ANYARGS))get_padding, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "padding=", (VALUE (*)(ANYARGS))set_padding, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "padding_bottom", (VALUE (*)(ANYARGS))get_padding_bottom, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "padding_bottom=", (VALUE (*)(ANYARGS))set_padding_bottom, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "tone", (VALUE (*)(ANYARGS))get_tone, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "tone=", (VALUE (*)(ANYARGS))set_tone, 1);
                }
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_WINDOWVX_BINDING_H
