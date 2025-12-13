/*
 ** bitmap.cpp
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

#include "bitmap.h"

#ifdef MKXPZ_RETRO
#  include "stb_image_malloc.h"
#  include <stb_image.h>
#  include <pixman-region/pixman-region.h>
#  include FT_STROKER_H
#  include "mkxp-polyfill.h" // std::lround, std::to_string
#  include "sandbox-serial-util.h"
#else
#  include <SDL.h>
#  include <SDL_image.h>
#  include <SDL_ttf.h>
#  include <SDL_rect.h>
#  include <SDL_surface.h>
#  include <pixman.h>
#endif // MKXPZ_RETRO

#include "gl-util.h"
#include "gl-meta.h"
#include "quad.h"
#include "quadarray.h"
#include "transform.h"
#include "exception.h"
#include "forced-assert.h"

#include "sharedstate.h"
#include "glstate.h"
#include "texpool.h"
#include "shader.h"
#include "filesystem.h"
#include "font.h"
#ifndef MKXPZ_RETRO
#include "eventthread.h"
#endif // MKXPZ_RETRO
#include "graphics.h"
#ifndef MKXPZ_RETRO
#include "system.h"
#endif // MKXPZ_RETRO
#include "util/util.h"

#include "debugwriter.h"

#include "sigslot/signal.hpp"

#include <math.h>
#include <algorithm>
#include <cstdio>
#include <unordered_map>

extern "C" {
#include "libnsgif/libnsgif.h"
}

#define GUARD_MEGA(...) \
{ \
if (p->megaSurface) \
{ \
exception = Exception(Exception::MKXPError, \
"Operation not supported for mega surfaces"); \
return __VA_ARGS__; \
} \
}

#define GUARD_ANIMATED(...) \
{ \
if (p->animation.enabled) \
{ \
exception = Exception(Exception::MKXPError, \
"Operation not supported for animated bitmaps"); \
return __VA_ARGS__; \
} \
}

#define GUARD_UNANIMATED(...) \
{ \
if (!p->animation.enabled) \
{ \
exception = Exception(Exception::MKXPError, \
"Operation not supported for static bitmaps"); \
return __VA_ARGS__; \
} \
}

#define GUARD_V(value, expression) do { expression; if (exception.is_error()) return value; } while (0)
#define GUARD(expression) GUARD_V(, expression)

#define OUTLINE_SIZE 1

#ifdef MKXPZ_RETRO
#  define DIFF_TILE_SIZE (size_t)64
#  define FLOOR_DIV_DIFF_TILE_SIZE(x) ((size_t)(x) / DIFF_TILE_SIZE)
#  define CEIL_DIV_DIFF_TILE_SIZE(x) ((((size_t)(x) - 1) / DIFF_TILE_SIZE) + 1)

// This formula is from SDL_ttf (licensed under zlib license); we may want to adjust it for better accuracy with RPG Maker
#  define GET_BOLD_WIDTH(ft_face) ((ft_face)->size->metrics.y_ppem / 10)

// This formula is from SDL_ttf (licensed under zlib license); we may want to adjust it for better accuracy with RPG Maker
static const FT_Matrix ITALIC_TRANSFORM = (FT_Matrix){1 << 16, 0x0366a, 0, 1 << 16};
#  define GET_ITALIC_WIDTH(ft_face) (((uint32_t)ITALIC_TRANSFORM.xy * (uint32_t)(((int32_t)(ft_face)->ascender - (int32_t)(ft_face)->descender)) / 64) >> 16)

static uint64_t next_id = 1;
#endif // MKXPZ_RETRO

/* Normalize (= ensure width and
 * height are positive) */
static IntRect normalizedRect(const IntRect &rect)
{
    IntRect norm = rect;
    
    if (norm.w < 0)
    {
        norm.w = -norm.w;
        norm.x -= norm.w;
    }
    
    if (norm.h < 0)
    {
        norm.h = -norm.h;
        norm.y -= norm.h;
    }
    
    return norm;
}


// libnsgif loading callbacks, taken pretty much straight from their tests

static void *gif_bitmap_create(int width, int height)
{
    /* ensure a stupidly large bitmap is not created */
    return calloc(width * height, 4);
}


static void gif_bitmap_set_opaque(void *bitmap, bool opaque)
{
    (void) opaque;  /* unused */
    (void) bitmap;  /* unused */
    assert(bitmap);
}


static bool gif_bitmap_test_opaque(void *bitmap)
{
    (void) bitmap;  /* unused */
    assert(bitmap);
    return false;
}


static unsigned char *gif_bitmap_get_buffer(void *bitmap)
{
    assert(bitmap);
    return (unsigned char *)bitmap;
}


static void gif_bitmap_destroy(void *bitmap)
{
    assert(bitmap);
    free(bitmap);
}


static void gif_bitmap_modified(void *bitmap)
{
    (void) bitmap;  /* unused */
    assert(bitmap);
    return;
}

// --------------------

struct BitmapPrivate
{
    Bitmap *self;
    
    struct {
        int width;
        int height;
        
        bool enabled;
        bool playing;
        bool needsReset;
        bool loop;
        std::vector<BitmapFrame> frames;
        float fps;
        int lastFrame;
        double startTime, playTime;
        
        inline unsigned int currentFrameIRaw() {
            if (fps <= 0) return lastFrame;
            return floor(lastFrame + (playTime / (1 / fps)));
        }
        
        unsigned int currentFrameI() {
            if (!playing || needsReset) return lastFrame;
            int i = currentFrameIRaw();
            return (loop) ? fmod(i, frames.size()) : (i > (int)frames.size() - 1) ? (int)frames.size() - 1 : i;
        }
        
        inline BitmapFrame &currentFrame() {
            int i = currentFrameI();
            return frames[i];
        }
        
        inline void play() {
            playing = true;
            needsReset = true;
        }
        
        inline void stop() {
            lastFrame = currentFrameI();
            playing = false;
        }
        
        inline void seek(int frame) {
            lastFrame = clamp(frame, 0, (int)frames.size());
        }
        
        void updateTimer() {
            if (needsReset) {
                lastFrame = currentFrameI();
                playTime = 0;
                startTime = shState->runTime();
                needsReset = false;
                return;
            }
            
            playTime = shState->runTime() - startTime;
            return;
        }
    } animation;
    
    sigslot::connection prepareCon;
    
    TEXFBO gl;
    
    Font *font;
    
    /* "Mega surfaces" are a hack to allow Tilesets to be used
     * whose Bitmaps don't fit into a regular texture. They're
     * kept in RAM and will throw an error if they're used in
     * any context other than as Tilesets */
    SDL_Surface *megaSurface;
    
    /* A cached version of the bitmap in client memory, for
     * getPixel calls. Is invalidated any time the bitmap
     * is modified */
    SDL_Surface *surface;
#ifndef MKXPZ_RETRO
    SDL_PixelFormat *format;
#endif // MKXPZ_RETRO
    
    /* The 'tainted' area describes which parts of the
     * bitmap are not cleared, ie. don't have 0 opacity.
     * If we're blitting / drawing text to a cleared part
     * with full opacity, we can disregard any old contents
     * in the texture and blit to it directly, saving
     * ourselves the expensive blending calculation */
    pixman_region16_t tainted;

    // For high-resolution texture replacement.
    Bitmap *selfHires;
    Bitmap *selfLores;
    bool assumingRubyGC;

#ifdef MKXPZ_RETRO
    std::vector<std::vector<uint32_t>> diff;
    std::string path;
    int originalFrameIndex;
#endif // MKXPZ_RETRO
    
    BitmapPrivate(Bitmap *self)
    : self(self),
    megaSurface(0),
    selfHires(0),
    selfLores(0),
    surface(0),
    assumingRubyGC(false)
    {
#ifndef MKXPZ_RETRO
        format = SDL_AllocFormat(SDL_PIXELFORMAT_ABGR8888);
#endif // MKXPZ_RETRO
        
        animation.width = 0;
        animation.height = 0;
        animation.enabled = false;
        animation.playing = false;
        animation.needsReset = false;
        animation.loop = true;
        animation.playTime = 0;
        animation.startTime = 0;
        animation.fps = 0;
        animation.lastFrame = 0;
        
        prepareCon = shState->prepareDraw.connect(&BitmapPrivate::prepare, this);
        
        font = &shState->defaultFont();
        pixman_region_init(&tainted);
    }
    
    ~BitmapPrivate()
    {
        prepareCon.disconnect();
#ifndef MKXPZ_RETRO
        SDL_FreeFormat(format);
#endif // MKXPZ_RETRO
        pixman_region_fini(&tainted);
    }
    
    TEXFBO &getGLTypes() {
        return (animation.enabled) ? animation.currentFrame().gl : gl;
    }
    
    void prepare()
    {
        if (!animation.enabled || !animation.playing) return;
        
        animation.updateTimer();
    }
    
    void allocSurface()
    {
#ifdef MKXPZ_RETRO
        surface = new SDL_Surface {gl.width, gl.height, STBI_MALLOC(4 * gl.width * gl.height)};
        if (surface->pixels == nullptr) {
            MKXPZ_THROW(std::bad_alloc());
        }
#else
        surface = SDL_CreateRGBSurface(0, gl.width, gl.height, format->BitsPerPixel,
                                       format->Rmask, format->Gmask,
                                       format->Bmask, format->Amask);
        if (surface == nullptr) {
            MKXPZ_THROW(std::bad_alloc());
        }
#endif // MKXPZ_RETRO
    }
    
    void clearTaintedArea()
    {
        pixman_region_fini(&tainted);
        pixman_region_init(&tainted);
    }
    
    void addTaintedArea(const IntRect &rect)
    {
        IntRect norm = normalizedRect(rect);
        pixman_region_union_rect
        (&tainted, &tainted, norm.x, norm.y, norm.w, norm.h);
    }
    
    void substractTaintedArea(const IntRect &rect)
    {
        if (!touchesTaintedArea(rect))
            return;
        
        pixman_region16_t m_reg;
        pixman_region_init_rect(&m_reg, rect.x, rect.y, rect.w, rect.h);
        
        pixman_region_subtract(&tainted, &m_reg, &tainted);
        
        pixman_region_fini(&m_reg);
    }
    
    bool touchesTaintedArea(const IntRect &rect)
    {
        pixman_box16_t box;
        box.x1 = rect.x;
        box.y1 = rect.y;
        box.x2 = rect.x + rect.w;
        box.y2 = rect.y + rect.h;
        
        pixman_region_overlap_t result =
        pixman_region_contains_rectangle(&tainted, &box);
        
        return result != PIXMAN_REGION_OUT;
    }
    
    void bindTexture(ShaderBase &shader, bool substituteLoresSize = true)
    {
        if (selfHires) {
            selfHires->bindTex(shader, substituteLoresSize);
            return;
        }

        if (animation.enabled) {
            if (selfLores) {
                Debug() << "BUG: High-res BitmapPrivate bindTexture for animations not implemented";
            }

            TEXFBO cframe = animation.currentFrame().gl;
            TEX::bind(cframe.tex);
            shader.setTexSize(Vec2i(cframe.width, cframe.height));
            return;
        }
        TEX::bind(gl.tex);
        if (selfLores && substituteLoresSize) {
            shader.setTexSize(Vec2i(selfLores->width(), selfLores->height()));
        }
        else {
            shader.setTexSize(Vec2i(gl.width, gl.height));
        }
    }
    
    void bindFBO()
    {
        FBO::bind((animation.enabled) ? animation.currentFrame().gl.fbo : gl.fbo);
    }
    
    void pushSetViewport(ShaderBase &shader) const
    {
        glState.viewport.pushSet(IntRect(0, 0, gl.width, gl.height));
        shader.applyViewportProj();
    }
    
    void popViewport() const
    {
        glState.viewport.pop();
    }
    
    void blitQuad(Quad &quad)
    {
        glState.blend.pushSet(false);
        quad.draw();
        glState.blend.pop();
    }
    
    void fillRect(const IntRect &rect,
                  const Vec4 &color)
    {
        bindFBO();
        
        glState.scissorTest.pushSet(true);
        glState.scissorBox.pushSet(normalizedRect(rect));
        glState.clearColor.pushSet(color);
        
        FBO::clear();
        
        glState.clearColor.pop();
        glState.scissorBox.pop();
        glState.scissorTest.pop();
    }
    
#ifndef MKXPZ_RETRO
    static void ensureFormat(SDL_Surface *&surf, Uint32 format)
    {
        if (surf->format->format == format)
            return;
        
        SDL_Surface *surfConv = SDL_ConvertSurfaceFormat(surf, format, 0);
        SDL_FreeSurface(surf);
        surf = surfConv;
    }
#endif // MKXPZ_RETRO
    
    void onModified(bool freeSurface = true)
    {
        if (surface && freeSurface)
        {
#ifdef MKXPZ_RETRO
            stbi_image_free(surface->pixels);
            delete surface;
#else
            SDL_FreeSurface(surface);
#endif // MKXPZ_RETRO
            surface = 0;
        }
        
        self->modified();
    }

#ifdef MKXPZ_RETRO
    void pushDiff(const void *pixels, IntRect rect)
    {
        int image_width = megaSurface != nullptr ? megaSurface->w : animation.enabled ? animation.width : gl.width;
        int image_height = megaSurface != nullptr ? megaSurface->h : animation.enabled ? animation.height : gl.height;
        rect = normalizedRect(rect);
        rect.x = clamp(rect.x, 0, image_width - 1);
        rect.y = clamp(rect.y, 0, image_height - 1);
        rect.w = clamp(rect.w, 0, image_width - rect.x);
        rect.h = clamp(rect.h, 0, image_height - rect.y);

        if (diff.empty() || rect.w <= 0 || rect.h <= 0)
            return;

        std::vector<std::vector<uint32_t>> &diff = animation.enabled ? animation.currentFrame().diff : this->diff;
        const std::string &path = animation.enabled ? animation.currentFrame().path : this->path;

        for (size_t tile_row = FLOOR_DIV_DIFF_TILE_SIZE(rect.y); tile_row <= FLOOR_DIV_DIFF_TILE_SIZE(rect.y + (rect.h - 1)); ++tile_row)
        {
            for (size_t tile_col = FLOOR_DIV_DIFF_TILE_SIZE(rect.x); tile_col <= FLOOR_DIV_DIFF_TILE_SIZE(rect.x + (rect.w - 1)); ++tile_col)
            {
                size_t tile_width = std::min(DIFF_TILE_SIZE, image_width - DIFF_TILE_SIZE * tile_col);
                size_t tile_height = std::min(DIFF_TILE_SIZE, image_height - DIFF_TILE_SIZE * tile_row);
                size_t x_start = (size_t)rect.x > DIFF_TILE_SIZE * tile_col ? rect.x - DIFF_TILE_SIZE * tile_col : 0;
                size_t y_start = (size_t)rect.y > DIFF_TILE_SIZE * tile_row ? rect.y - DIFF_TILE_SIZE * tile_row : 0;
                size_t x_end = std::min(DIFF_TILE_SIZE, rect.x + rect.w - DIFF_TILE_SIZE * tile_col);
                size_t y_end = std::min(DIFF_TILE_SIZE, rect.y + rect.h - DIFF_TILE_SIZE * tile_row);

                std::vector<uint32_t> &tile = diff[CEIL_DIV_DIFF_TILE_SIZE(image_width) * tile_row + tile_col];
                tile.resize(tile_width * tile_height);
                tile.shrink_to_fit();

                // If this bitmap has a path and `pixels` doesn't cover the entire tile,
                // take a snapshot of the part of the bitmap corresponding to the tile and use it to fill in the rest of the tile
                if (!path.empty() && (x_start != 0 || y_start != 0 || x_end < tile_width || y_end < tile_height))
                {
                    if (megaSurface != nullptr)
                    {
                        for (size_t y = 0; y < tile_height; ++y)
                            std::memcpy(tile.data() + tile_width * y, (const uint32_t *)megaSurface->pixels + megaSurface->w * (DIFF_TILE_SIZE * tile_row + y) + DIFF_TILE_SIZE * tile_col, tile_width);
                    }
                    else
                    {
                        bindFBO();
                        ::gl.ReadPixels(DIFF_TILE_SIZE * tile_col, DIFF_TILE_SIZE * tile_row, tile_width, tile_height, GL_RGBA, GL_UNSIGNED_BYTE, tile.data());
                    }
                }

                for (size_t y = y_start; y < y_end; ++y)
                    std::memcpy(tile.data() + tile_width * y + x_start, (const uint32_t *)pixels + rect.w * (DIFF_TILE_SIZE * tile_row + y - rect.y) + DIFF_TILE_SIZE * tile_col + x_start - rect.x, 4 * (x_end - x_start));

                // If the path is empty, that means the bitmap was originally empty when it was created, so empty tiles can be removed from the diff
                if (path.empty())
                {
                    bool tile_is_empty = true;
                    const uint8_t *data = (uint8_t *)tile.data();
                    for (size_t i = 0; i < 4 * tile.size(); ++i)
                    {
                        if (data[i] != 0)
                        {
                            tile_is_empty = false;
                            break;
                        }
                    }
                    if (tile_is_empty)
                    {
                        tile.clear();
                    }
                }
            }
        }
    }

    void pushDiff(IntRect rect)
    {
        int image_width = megaSurface != nullptr ? megaSurface->w : animation.enabled ? animation.width : gl.width;
        int image_height = megaSurface != nullptr ? megaSurface->h : animation.enabled ? animation.height : gl.height;
        rect = normalizedRect(rect);
        rect.x = clamp(rect.x, 0, image_width - 1);
        rect.y = clamp(rect.y, 0, image_height - 1);
        rect.w = clamp(rect.w, 0, image_width - rect.x);
        rect.h = clamp(rect.h, 0, image_height - rect.y);

        if (diff.empty() || rect.w <= 0 || rect.h <= 0)
            return;

        IntRect expanded_rect(rect);
        const std::string &path = animation.enabled ? animation.currentFrame().path : this->path;
        // If the path is not empty, expand the rect so that it doesn't partially cover any tiles
        if (!path.empty()) {
            expanded_rect.x = DIFF_TILE_SIZE * FLOOR_DIV_DIFF_TILE_SIZE(rect.x);
            expanded_rect.y = DIFF_TILE_SIZE * FLOOR_DIV_DIFF_TILE_SIZE(rect.y);
            expanded_rect.w = DIFF_TILE_SIZE * CEIL_DIV_DIFF_TILE_SIZE(rect.w + (rect.x - expanded_rect.x));
            expanded_rect.h = DIFF_TILE_SIZE * CEIL_DIV_DIFF_TILE_SIZE(rect.h + (rect.y - expanded_rect.y));
            expanded_rect.x = clamp(expanded_rect.x, 0, image_width - 1);
            expanded_rect.y = clamp(expanded_rect.y, 0, image_height - 1);
            expanded_rect.w = clamp(expanded_rect.w, 0, image_width - expanded_rect.x);
            expanded_rect.h = clamp(expanded_rect.h, 0, image_height - expanded_rect.y);
        }

        if (expanded_rect.w <= 0 || expanded_rect.h <= 0)
            return;

        uint32_t *pixels = (uint32_t *)STBI_MALLOC(4 * expanded_rect.w * expanded_rect.h);
        if (pixels == nullptr)
            MKXPZ_THROW(std::bad_alloc());

        if (megaSurface != nullptr)
        {
            for (size_t y = 0; y < (size_t)expanded_rect.h; ++y)
                std::memcpy(pixels + expanded_rect.w * y, (const uint32_t *)megaSurface->pixels + megaSurface->w * (expanded_rect.y + y) + expanded_rect.x, expanded_rect.w);
        }
        else
        {
            bindFBO();
            ::gl.ReadPixels(expanded_rect.x, expanded_rect.y, expanded_rect.w, expanded_rect.h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        }

        pushDiff(pixels, expanded_rect);

        stbi_image_free(pixels);
    }
#endif // MKXPZ_RETRO
};

struct BitmapOpenHandler : FileSystem::OpenHandler
{
    // Non-GIF
#ifdef MKXPZ_RETRO
    stbi_uc *image;
    int width;
    int height;
#else
    SDL_Surface *surface;
#endif // MKXPZ_RETRO

    // GIF
    std::string error;
    gif_animation *gif;
    unsigned char *gif_data;
    size_t gif_data_size;

    BitmapOpenHandler()
#ifdef MKXPZ_RETRO
    : image(0),
#else
    : surface(0),
#endif // MKXPZ_RETRO
    gif(0), gif_data(0), gif_data_size(0)
    {}
    
#ifdef MKXPZ_RETRO
    bool tryRead(std::shared_ptr<struct FileSystem::File> ops, const char *ext)
#else
    bool tryRead(SDL_RWops &ops, const char *ext)
#endif // MKXPZ_RETRO
    {
#ifdef MKXPZ_RETRO
        uint8_t header_buffer[6];
        PHYSFS_seek(ops->get_read(), 0);
        if (PHYSFS_readBytes(ops->get_read(), header_buffer, 6) == 6 && (!std::memcmp(header_buffer, "GIF87a", 6) || !std::memcmp(header_buffer, "GIF89a", 6))) {
#else
        if (IMG_isGIF(&ops)) {
#endif // MKXPZ_RETRO
            // Use libnsgif to initialise the gif data
            gif = new gif_animation;
            
            gif_bitmap_callback_vt gif_bitmap_callbacks = {
                gif_bitmap_create,
                gif_bitmap_destroy,
                gif_bitmap_get_buffer,
                gif_bitmap_set_opaque,
                gif_bitmap_test_opaque,
                gif_bitmap_modified
            };
            
            gif_create(gif, &gif_bitmap_callbacks);
            
#ifdef MKXPZ_RETRO
            gif_data_size = PHYSFS_fileLength(ops->get_read());
#else
            gif_data_size = ops.size(&ops);
#endif // MKXPZ_RETRO
            
            gif_data = new unsigned char[gif_data_size];
#ifdef MKXPZ_RETRO
            PHYSFS_seek(ops->get_read(), 0);
            PHYSFS_readBytes(ops->get_read(), gif_data, gif_data_size);
#else
            ops.seek(&ops, 0, RW_SEEK_SET);
            ops.read(&ops, gif_data, gif_data_size, 1);
#endif // MKXPZ_RETRO
            
            int status;
            do {
                status = gif_initialise(gif, gif_data_size, gif_data);
                if (status != GIF_OK && status != GIF_WORKING) {
                    gif_finalise(gif);
                    delete gif;
                    delete gif_data;
                    error = "Failed to initialize GIF (Error " + std::to_string(status) + ")";
                    return false;
                }
            } while (status != GIF_OK);
            
            // Decode the first frame
            status = gif_decode_frame(gif, 0);
            if (status != GIF_OK && status != GIF_WORKING) {
                error = "Failed to decode first GIF frame. (Error " + std::to_string(status) + ")";
                gif_finalise(gif);
                delete gif;
                delete gif_data;
                return false;
            }
        } else {
#ifdef MKXPZ_RETRO
            PHYSFS_seek(ops->get_read(), 0);

            struct file {
                struct FileSystem::File *handle;
                uint64_t offset;
            };

            const static stbi_io_callbacks callbacks = {
                [](void *handle, char *buf, int size) {
                    assert(size >= 0);
                    int n = PHYSFS_readBytes(((struct file *)handle)->handle->get_read(), buf, size);
                    assert(((struct file *)handle)->offset + (uint64_t)n >= ((struct file *)handle)->offset);
                    ((struct file *)handle)->offset += n;
                    return n;
                },
                [](void *handle, int size) {
                    assert(size >= 0);
                    assert(((struct file *)handle)->offset + (uint64_t)size >= ((struct file *)handle)->offset);
                    PHYSFS_seek(((struct file *)handle)->handle->get_read(), (((struct file *)handle)->offset += (uint64_t)size));
                },
                [](void *handle) {
                    return PHYSFS_eof(((struct file *)handle)->handle->get_read());
                },
            };

            struct file file {
                ops.get(),
                0,
            };

            image = stbi_load_from_callbacks(&callbacks, &file, &width, &height, nullptr, STBI_rgb_alpha);
#else
            surface = IMG_LoadTyped_RW(&ops, 1, ext);
#endif // MKXPZ_RETRO
        }

#ifdef MKXPZ_RETRO
        return (image || gif);
#else
        return (surface || gif);
#endif // MKXPZ_RETRO
    }
};

Bitmap::Bitmap(Exception &exception, const char *filename, bool useDiff) :
#ifdef MKXPZ_RETRO
    id(next_id++),
#endif // MKXPZ_RETRO
    p(nullptr)
{
    initFromFilename(exception, filename, useDiff);
}

void Bitmap::initFromFilename(Exception &exception, const char *filename, bool useDiff)
{
    std::string hiresPrefix = "Hires/";
    std::string filenameStd = filename;
    Bitmap *hiresBitmap = nullptr;
#ifndef MKXPZ_RETRO
    // TODO: once C++20 is required, switch to filenameStd.starts_with(hiresPrefix)
    if (shState->config().enableHires && filenameStd.compare(0, hiresPrefix.size(), hiresPrefix) != 0) {
        // Look for a high-res version of the file.
        std::string hiresFilename = hiresPrefix + filenameStd;
        Exception e;
        hiresBitmap = new Bitmap(e, hiresFilename.c_str());
        if (e.is_error())
        {
            Debug() << "No high-res Bitmap found at" << hiresFilename;
            delete hiresBitmap;
            hiresBitmap = nullptr;
        }
        else
        {
            hiresBitmap->setLores(e, this);
            if (e.is_error())
            {
                Debug() << "No high-res Bitmap found at" << hiresFilename;
                delete hiresBitmap;
                hiresBitmap = nullptr;
            }
        }
    }
#endif // MKXPZ_RETRO

    BitmapOpenHandler handler;
#ifdef MKXPZ_RETRO
    mkxp_retro::fs->openRead(handler, filename); // TODO: move into shState
#else
    shState->fileSystem().openRead(handler, filename);
#endif // MKXPZ_RETRO

    if (handler.exception.is_error()) {
        if (hiresBitmap)
            delete hiresBitmap;
        exception = handler.exception;
        return;
    }
    else if (!handler.error.empty()) {
        if (hiresBitmap)
            delete hiresBitmap;
        // Not loaded with SDL, but I want it to be caught with the same exception type
        exception = Exception(Exception::SDLError, "Error loading image '%s': %s", filename, handler.error.c_str());
        return;
    }
#ifdef MKXPZ_RETRO
    else if (!handler.gif && !handler.image) {
        if (hiresBitmap)
            delete hiresBitmap;
        exception = Exception(Exception::SDLError, "Error loading image '%s': %s", filename, stbi_failure_reason());
        return;
    }
#else
    else if (!handler.gif && !handler.surface) {
        if (hiresBitmap)
            delete hiresBitmap;
        exception = Exception(Exception::SDLError, "Error loading image '%s': %s", filename, SDL_GetError());
        return;
    }
#endif // MKXPZ_RETRO
    
    if (handler.gif) {
        if (handler.gif->width >= (uint32_t)glState.caps.maxTexSize || handler.gif->height > (uint32_t)glState.caps.maxTexSize)
        {
            if (hiresBitmap)
                delete hiresBitmap;
            exception = Exception(Exception::MKXPError, "Animation too large (%ix%i, max %ix%i)",
                                handler.gif->width, handler.gif->height, glState.caps.maxTexSize, glState.caps.maxTexSize);
            return;
        }
        
        p = new BitmapPrivate(this);
        
        p->selfHires = hiresBitmap;
        
        if (handler.gif->frame_count == 1) {
            TEXFBO texfbo = shState->texPool().request(exception, handler.gif->width, handler.gif->height);
            if (exception.is_error())
            {
                gif_finalise(handler.gif);
                delete handler.gif;
                delete handler.gif_data;
                
                delete p;
                if (hiresBitmap)
                    delete hiresBitmap;
                
                return;
            }
            
            TEX::bind(texfbo.tex);
            TEX::uploadImage(handler.gif->width, handler.gif->height, handler.gif->frame_image, GL_RGBA);
            gif_finalise(handler.gif);
            delete handler.gif;
            delete handler.gif_data;
            
            p->gl = texfbo;
            if (p->selfHires != nullptr) {
                p->gl.selfHires = &p->selfHires->getGLTypes();
            }
            p->addTaintedArea(rect());
            return;
        }
        
        p->animation.enabled = true;
        p->animation.width = handler.gif->width;
        p->animation.height = handler.gif->height;
        
        // Guess framerate based on the first frame's delay
        p->animation.fps = 1 / ((float)handler.gif->frames[handler.gif->decoded_frame].frame_delay / 100);
        if (p->animation.fps < 0) p->animation.fps = shState->graphics().getFrameRate();
        
        // Loop gif (Either it's looping or it's not, at the moment)
        p->animation.loop = handler.gif->loop_count >= 0;
        
        int fcount = handler.gif->frame_count;
        int fcount_partial = handler.gif->frame_count_partial;
        if (fcount > fcount_partial) {
            Debug() << "Non-fatal error reading" << filename << ": Only decoded" << fcount_partial << "out of" << fcount << "frames";
        }
        for (int i = 0; i < fcount_partial; i++) {
            if (i > 0) {
                int status = gif_decode_frame(handler.gif, i);
                if (status != GIF_OK && status != GIF_WORKING) {
                    releaseResources();
                    
                    gif_finalise(handler.gif);
                    delete handler.gif;
                    delete handler.gif_data;
                    
                    exception = Exception(Exception::MKXPError, "Failed to decode GIF frame %i out of %i (Status %i)",
                                    i + 1, fcount_partial, status);
                    return;
                }
            }
            
            TEXFBO texfbo = shState->texPool().request(exception, p->animation.width, p->animation.height);
            if (exception.is_error())
            {
                releaseResources();
                
                gif_finalise(handler.gif);
                delete handler.gif;
                delete handler.gif_data;
                
                return;
            }
            
            TEX::bind(texfbo.tex);
            TEX::uploadImage(p->animation.width, p->animation.height, handler.gif->frame_image, GL_RGBA);
#ifdef MKXPZ_RETRO
            p->animation.frames.push_back({texfbo, {}, {}, i});
#else
            p->animation.frames.push_back({texfbo});
#endif // MKXPZ_RETRO
        }

#ifdef MKXPZ_RETRO
        p->diff.clear();
        if (useDiff)
            p->diff.resize(CEIL_DIV_DIFF_TILE_SIZE(p->animation.width) * CEIL_DIV_DIFF_TILE_SIZE(p->animation.height));
        p->path = mkxp_retro::fs->normalize(filename, false, true);
#endif // MKXPZ_RETRO

        gif_finalise(handler.gif);
        delete handler.gif;
        delete handler.gif_data;
        p->addTaintedArea(rect());
        return;
    }

#ifdef MKXPZ_RETRO
    SDL_Surface *imgSurf = new SDL_Surface;
    imgSurf->pixels = handler.image;
    imgSurf->w = handler.width;
    imgSurf->h = handler.height;
#else
    SDL_Surface *imgSurf = handler.surface;
#endif // MKXPZ_RETRO
    GUARD(initFromSurface(exception, imgSurf, hiresBitmap, false, useDiff));
#ifdef MKXPZ_RETRO
    p->path = mkxp_retro::fs->normalize(filename, false, true);
#endif // MKXPZ_RETRO
}

Bitmap::Bitmap(Exception &exception, int width, int height, bool isHires, bool useDiff) :
#ifdef MKXPZ_RETRO
    id(next_id++),
#endif // MKXPZ_RETRO
    p(nullptr)
{
    initFromDimensions(exception, width, height, isHires, useDiff);
}

void Bitmap::initFromDimensions(Exception &exception, int width, int height, bool isHires, bool useDiff)
{
    if (width <= 0 || height <= 0) {
        exception = Exception(Exception::RGSSError, "failed to create bitmap");
        return;
    }
    
    Bitmap *hiresBitmap = nullptr;

    if (shState->config().enableHires && !isHires) {
        // Create a high-res version as well.
        double scalingFactor = shState->config().textureScalingFactor;
        int hiresWidth = (int)lround(scalingFactor * width);
        int hiresHeight = (int)lround(scalingFactor * height);
        hiresBitmap = new Bitmap(exception, hiresWidth, hiresHeight, true);
        if (exception.is_error()) {
            delete hiresBitmap;
            return;
        }
        hiresBitmap->setLores(exception, this);
        if (exception.is_error()) {
            delete hiresBitmap;
            return;
        }
    }

    TEXFBO tex = shState->texPool().request(exception, width, height);
    if (exception.is_error()) {
        if (hiresBitmap)
            delete hiresBitmap;
        return;
    }
    
    p = new BitmapPrivate(this);
    p->gl = tex;
    p->selfHires = hiresBitmap;
    if (p->selfHires != nullptr) {
        p->gl.selfHires = &p->selfHires->getGLTypes();
    }
    
#ifdef MKXPZ_RETRO
    p->diff.clear();
    if (useDiff)
        p->diff.resize(CEIL_DIV_DIFF_TILE_SIZE(width) * CEIL_DIV_DIFF_TILE_SIZE(height));
    p->path.clear();
#endif // MKXPZ_RETRO
    GUARD(clear(exception));
}

Bitmap::Bitmap(Exception &exception, void *pixeldata, int width, int height, bool useDiff) :
#ifdef MKXPZ_RETRO
    id(next_id++),
#endif // MKXPZ_RETRO
    p(nullptr)
{
#ifdef MKXPZ_RETRO
    SDL_Surface *surface = new SDL_Surface;

    stbi_uc *image = (stbi_uc *)STBI_MALLOC((size_t)4 * (size_t)width * (size_t)height * sizeof(stbi_uc));
    if (image == nullptr)
        MKXPZ_THROW(std::bad_alloc());

    surface->pixels = image;
    surface->w = width;
    surface->h = height;
#else // TODO
    SDL_Surface *surface = SDL_CreateRGBSurface(0, width, height, p->format->BitsPerPixel,
                                                p->format->Rmask,
                                                p->format->Gmask,
                                                p->format->Bmask,
                                                p->format->Amask);
    
    if (!surface)
        MKXPZ_THROW(std::bad_alloc());
    
    memcpy(surface->pixels, pixeldata, width*height*(p->format->BitsPerPixel/8));
#endif // MKXPZ_RETRO
    
    if (surface->w > glState.caps.maxTexSize || surface->h > glState.caps.maxTexSize)
    {
        p = new BitmapPrivate(this);
        p->megaSurface = surface;
#ifndef MKXPZ_RETRO
        SDL_SetSurfaceBlendMode(p->megaSurface, SDL_BLENDMODE_NONE);
#endif // MKXPZ_RETRO
    }
    else
    {
        TEXFBO tex = shState->texPool().request(exception, surface->w, surface->h);
        if (exception.is_error())
        {
#ifdef MKXPZ_RETRO
            stbi_image_free(surface->pixels);
            delete surface;
#else
            SDL_FreeSurface(surface);
#endif // MKXPZ_RETRO
            return;
        }
        
        p = new BitmapPrivate(this);
        p->gl = tex;
        
        TEX::bind(p->gl.tex);
        TEX::uploadImage(p->gl.width, p->gl.height, surface->pixels, GL_RGBA);
        
#ifdef MKXPZ_RETRO
        stbi_image_free(surface->pixels);
        delete surface;
#else
        SDL_FreeSurface(surface);
#endif // MKXPZ_RETRO
    }

#ifdef MKXPZ_RETRO
    p->diff.clear();
    if (useDiff)
        p->diff.resize(CEIL_DIV_DIFF_TILE_SIZE(width) * CEIL_DIV_DIFF_TILE_SIZE(height));
    p->path.clear();
    p->pushDiff(pixeldata, rect());
#endif // MKXPZ_RETRO

    p->addTaintedArea(rect());
}

// frame is -2 for "any and all", -1 for "current", anything else for a specific frame
Bitmap::Bitmap(Exception &exception, const Bitmap &other, int frame, bool useDiff) :
#ifdef MKXPZ_RETRO
    id(next_id++),
#endif // MKXPZ_RETRO
    p(nullptr)
{
    GUARD(other.guardDisposed(exception));
    GUARD(other.ensureNonMega(exception));
    if (frame > -2) GUARD(other.ensureAnimated(exception));
    
    if (other.hasHires()) {
        Debug() << "BUG: High-res Bitmap from animation not implemented";
    }

    p = new BitmapPrivate(this);
    
    // TODO: Clean me up
    if (!other.isAnimated() || frame >= -1) {
        p->gl = shState->texPool().request(exception, other.width(), other.height());
        if (exception.is_error()) {
            delete p;
            return;
        }
        
        GLMeta::blitBegin(p->gl, false, SameScale);
        // Blit just the current frame of the other animated bitmap
        if (!other.isAnimated() || frame == -1) {
            GLMeta::blitSource(other.getGLTypes(), SameScale);
        }
        else {
            auto &frames = other.getFrames();
            GLMeta::blitSource(frames[clamp(frame, 0, (int)frames.size() - 1)].gl, SameScale);
        }
        GLMeta::blitRectangle(rect(), rect());
        GLMeta::blitEnd();
    }
    else {
        p->animation.enabled = true;
        p->animation.fps = other.animationFPS();
        p->animation.width = other.width();
        p->animation.height = other.height();
        p->animation.lastFrame = 0;
        p->animation.playTime = 0;
        p->animation.startTime = 0;
        p->animation.loop = other.looping();
        
        for (BitmapFrame &sourceframe : other.getFrames()) {
            TEXFBO newframe = shState->texPool().request(exception, p->animation.width, p->animation.height);
            if (exception.is_error()) {
                releaseResources();
                return;
            }
            
            GLMeta::blitBegin(newframe, false, SameScale);
            GLMeta::blitSource(sourceframe.gl, SameScale);
            GLMeta::blitRectangle(rect(), rect());
            GLMeta::blitEnd();
            
#ifdef MKXPZ_RETRO
            p->animation.frames.push_back({newframe, sourceframe.diff, sourceframe.path, sourceframe.originalFrameIndex});
#else
            p->animation.frames.push_back({newframe});
#endif // MKXPZ_RETRO
        }
    }

#ifdef MKXPZ_RETRO
    if (useDiff)
        p->diff = other.p->diff;
    p->path = other.p->path;
#endif // MKXPZ_RETRO

    p->addTaintedArea(rect());
}

Bitmap::Bitmap(Exception &exception, TEXFBO &other, bool useDiff) :
#ifdef MKXPZ_RETRO
    id(next_id++),
#endif // MKXPZ_RETRO
    p(nullptr)
{
    Bitmap *hiresBitmap = nullptr;

    if (other.selfHires != nullptr) {
        // Create a high-res version as well.
        hiresBitmap = new Bitmap(exception, *other.selfHires);
        if (exception.is_error()) {
            delete hiresBitmap;
            return;
        }
        hiresBitmap->setLores(exception, this);
        if (exception.is_error()) {
            delete hiresBitmap;
            return;
        }
    }

    p = new BitmapPrivate(this);
    p->selfHires = hiresBitmap;

    p->gl = shState->texPool().request(exception, other.width, other.height);
    if (exception.is_error()) {
        delete p;
        return;
    }

    if (p->selfHires != nullptr) {
        p->gl.selfHires = &p->selfHires->getGLTypes();
    }

    // Skip blitting to lores texture, since only the hires one will be displayed.
    if (p->selfHires == nullptr) {
        GLMeta::blitBegin(p->gl, false, SameScale);
        GLMeta::blitSource(other, SameScale);
        GLMeta::blitRectangle(rect(), rect());
        GLMeta::blitEnd();
    }

#ifdef MKXPZ_RETRO
    p->diff.clear();
    if (useDiff)
        p->diff.resize(CEIL_DIV_DIFF_TILE_SIZE(width()) * CEIL_DIV_DIFF_TILE_SIZE(height()));
    p->path.clear();
    p->pushDiff(rect());
#endif // MKXPZ_RETRO

    p->addTaintedArea(rect());
}

Bitmap::Bitmap(Exception &exception, SDL_Surface *imgSurf, SDL_Surface *imgSurfHires, bool forceMega, bool useDiff) :
#ifdef MKXPZ_RETRO
    id(next_id++),
#endif // MKXPZ_RETRO
    p(nullptr)
{
    Bitmap *hiresBitmap = nullptr;

    if (imgSurfHires != nullptr) {
        // Create a high-res version as well.
        hiresBitmap = new Bitmap(exception, imgSurfHires, nullptr);
        if (exception.is_error()) {
            delete hiresBitmap;
            return;
        }
        hiresBitmap->setLores(exception, this);
        if (exception.is_error()) {
            delete hiresBitmap;
            return;
        }
    }

    GUARD(initFromSurface(exception, imgSurf, hiresBitmap, forceMega, useDiff));
}

Bitmap::~Bitmap()
{
    dispose();

    loresDispCon.disconnect();
}

void Bitmap::initFromSurface(Exception &exception, SDL_Surface *imgSurf, Bitmap *hiresBitmap, bool forceMega, bool useDiff)
{
#ifndef MKXPZ_RETRO
    p->ensureFormat(imgSurf, SDL_PIXELFORMAT_ABGR8888);
#endif // MKXPZ_RETRO
    
    if (imgSurf->w > glState.caps.maxTexSize || imgSurf->h > glState.caps.maxTexSize || forceMega)
    {
        /* Mega surface */

        p = new BitmapPrivate(this);
        p->selfHires = hiresBitmap;
        p->megaSurface = imgSurf;
#ifndef MKXPZ_RETRO
        SDL_SetSurfaceBlendMode(p->megaSurface, SDL_BLENDMODE_NONE);
#endif // MKXPZ_RETRO
    }
    else
    {
        /* Regular surface */
        TEXFBO tex = shState->texPool().request(exception, imgSurf->w, imgSurf->h);
        if (exception.is_error())
        {
            if (hiresBitmap)
                delete hiresBitmap;
#ifdef MKXPZ_RETRO
            stbi_image_free(imgSurf->pixels);
            delete imgSurf;
#else
            SDL_FreeSurface(imgSurf);
#endif // MKXPZ_RETRO
            return;
        }
        
        p = new BitmapPrivate(this);
        p->selfHires = hiresBitmap;
        p->gl = tex;
        if (p->selfHires != nullptr) {
            p->gl.selfHires = &p->selfHires->getGLTypes();
        }
        
        TEX::bind(p->gl.tex);
        TEX::uploadImage(p->gl.width, p->gl.height, imgSurf->pixels, GL_RGBA);
        
#ifdef MKXPZ_RETRO
        stbi_image_free(imgSurf->pixels);
        delete imgSurf;
#else
        SDL_FreeSurface(imgSurf);
#endif // MKXPZ_RETRO
    }

#ifdef MKXPZ_RETRO
    p->diff.clear();
    if (useDiff)
        p->diff.resize(CEIL_DIV_DIFF_TILE_SIZE(width()) * CEIL_DIV_DIFF_TILE_SIZE(height()));
    p->path.clear();
#endif // MKXPZ_RETRO

    p->addTaintedArea(rect());
}

int Bitmap::width() const
{
    if (p->megaSurface) {
        return p->megaSurface->w;
    }
    
    if (p->animation.enabled) {
        return p->animation.width;
    }
    
    return p->gl.width;
}

int Bitmap::height() const
{
    if (p->megaSurface)
        return p->megaSurface->h;
    
    if (p->animation.enabled)
        return p->animation.height;
    
    return p->gl.height;
}

bool Bitmap::hasHires() const{
    return p->selfHires;
}

Bitmap *Bitmap::getHires(Exception &exception) const {
    GUARD_V(nullptr, guardDisposed(exception));

    return p->selfHires;
}

void Bitmap::setHiresRaw(Exception &exception, Bitmap *hires) {
    GUARD(guardDisposed(exception));

    GUARD(hires->setLoresRaw(exception, this));
    p->selfHires = hires;
}

void Bitmap::setHires(Exception &exception, Bitmap *hires) {
    GUARD(guardDisposed(exception));

    Debug() << "BUG: High-res Bitmap setHires not fully implemented, expect bugs";
    GUARD(hires->setLores(exception, this));
    p->selfHires = hires;
}

void Bitmap::setLoresRaw(Exception &exception, Bitmap *lores) {
    GUARD(guardDisposed(exception));

    p->selfLores = lores;
}

void Bitmap::setLores(Exception &exception, Bitmap *lores) {
    GUARD(guardDisposed(exception));

    p->selfLores = lores;
    loresDispCon = lores->wasDisposed.connect(&Bitmap::loresDisposal, this);

    if (p->font && p->font != &shState->defaultFont())
        p->font->setHiresMult((float)width() / (float)lores->width());
}

bool Bitmap::isMega() const{
    return p->megaSurface;
}

bool Bitmap::isAnimated() const {
    return p->animation.enabled;
}

IntRect Bitmap::rect() const
{
    return IntRect(0, 0, width(), height());
}

int Bitmap::getWidth(Exception &exception) const
{
    GUARD_V(0, guardDisposed(exception));
    return width();
}

int Bitmap::getHeight(Exception &exception) const
{
    GUARD_V(0, guardDisposed(exception));
    return height();
}

bool Bitmap::getHasHires(Exception &exception) const{
    GUARD_V(false, guardDisposed(exception));
    return hasHires();
}

bool Bitmap::getIsMega(Exception &exception) const{
    GUARD_V(false, guardDisposed(exception));
    return isMega();
}

bool Bitmap::getIsAnimated(Exception &exception) const {
    GUARD_V(false, guardDisposed(exception));
    return isAnimated();
}

IntRect Bitmap::getRect(Exception &exception) const
{
    GUARD_V(IntRect(), guardDisposed(exception));
    return rect();
}

void Bitmap::blt(Exception &exception,
                 int x, int y,
                 const Bitmap &source, const IntRect &rect,
                 int opacity)
{
    if (source.isDisposed())
        return;
    
    GUARD(stretchBlt(exception, IntRect(x, y, abs(rect.w), abs(rect.h)),
                     source, rect, opacity));
}

static bool shrinkRects(float &sourcePos, float &sourceLen, const int &sBitmapLen,
                         float &destPos, float &destLen, const int &dBitmapLen, bool normalize = false)
{
    float sStart = sourceLen > 0 ? sourcePos : sourceLen + sourcePos;
    float sEnd = sourceLen > 0 ? sourceLen + sourcePos : sourcePos;
    float sLength = sEnd - sStart;
    
    if (sStart >= 0 && sEnd < sBitmapLen)
        return false;
    
    if (sStart >= sBitmapLen || sEnd < 0)
        return true;
    
    float dStart = destLen > 0 ? destPos: destLen + destPos;
    float dEnd = destLen > 0 ? destLen + destPos : destPos;
    float dLength = dEnd - dStart;
    
    float delta = sEnd - sBitmapLen;
    float dDelta;
    if (delta > 0)
    {
        dDelta = (delta / sLength) * dLength;
        sLength -= delta;
        sEnd = sBitmapLen;
        dEnd -= dDelta;
        dLength -= dDelta;
    }
    if (sStart < 0)
    {
        dDelta = (sStart / sLength) * dLength;
        sLength += sStart;
        sStart = 0;
        dStart -= dDelta;
        dLength += dDelta;
    }
    
    if (!normalize)
    {
        sourcePos = sourceLen > 0 ? sStart : sEnd;
        sourceLen = sourceLen > 0 ? sLength : -sLength;
        destPos = destLen > 0  ? dStart : dEnd;
        destLen = destLen > 0 ? dLength : -dLength;
    }
    else
    {
        // Ensure the source rect has positive dimensions, for blitting from mega surfaces
        destPos = ((destLen > 0) == (sourceLen > 0)) ? dStart : dEnd;
        destLen = ((destLen > 0) == (sourceLen > 0)) ? dLength : -dLength;
        sourcePos = sStart;
        sourceLen = sLength;
    }
    
    return false;
}

static bool shrinkRects(int &sourcePos, int &sourceLen, const int &sBitmapLen,
                         int &destPos, int &destLen, const int &dBitmapLen)
{
    float fSourcePos = sourcePos;
    float fSourceLen = sourceLen;
    float fDestPos = destPos;
    float fDestLen = destLen;
    
    bool ret = shrinkRects(fSourcePos, fSourceLen, sBitmapLen, fDestPos, fDestLen, dBitmapLen, true);
    
    if (!ret)
        ret = shrinkRects(fDestPos, fDestLen, dBitmapLen, fSourcePos, fSourceLen, sBitmapLen);
    
    sourcePos = round(fSourcePos);
    sourceLen = round(fSourceLen);
    destPos = round(fDestPos);
    destLen = round(fDestLen);
    
    return ret || sourceLen == 0 || destLen == 0;
}

void Bitmap::stretchBlt(Exception &exception,
                        IntRect destRect,
                        const Bitmap &source, IntRect sourceRect,
                        int opacity, bool smooth)
{
    GUARD(guardDisposed(exception));

    // Don't need this, right? This function is fine with megasurfaces it seems
    //GUARD_MEGA;

    if (source.isDisposed())
        return;

    if (hasHires()) {
        int destX, destY, destWidth, destHeight;
        destX = destRect.x * p->selfHires->width() / width();
        destY = destRect.y * p->selfHires->height() / height();
        destWidth = destRect.w * p->selfHires->width() / width();
        destHeight = destRect.h * p->selfHires->height() / height();

        GUARD(p->selfHires->stretchBlt(exception, IntRect(destX, destY, destWidth, destHeight), source, sourceRect, opacity));
        return;
    }

    if (source.hasHires()) {
        int sourceX, sourceY, sourceWidth, sourceHeight;
        Bitmap *hires;
        GUARD(hires = source.getHires(exception));
        sourceX = sourceRect.x * hires->width() / source.width();
        sourceY = sourceRect.y * hires->height() / source.height();
        sourceWidth = sourceRect.w * hires->width() / source.width();
        sourceHeight = sourceRect.h * hires->height() / source.height();

        GUARD(stretchBlt(exception, destRect, *hires, IntRect(sourceX, sourceY, sourceWidth, sourceHeight), opacity));
        return;
    }

    opacity = clamp(opacity, 0, 255);
    
    if (opacity == 0)
        return;
    
    if(shrinkRects(sourceRect.x, sourceRect.w, source.width(), destRect.x, destRect.w, width()))
        return;
    if(shrinkRects(sourceRect.y, sourceRect.h, source.height(), destRect.y, destRect.h, height()))
        return;
    
    SDL_Surface *srcSurf = source.megaSurface();
    SDL_Surface *blitTemp = 0;
    bool touchesTaintedArea = p->touchesTaintedArea(destRect);
    bool unpack_subimage = srcSurf && gl.unpack_subimage;

    const bool scaleIsOne = sourceRect.w == destRect.w && sourceRect.h == destRect.h;
    if (scaleIsOne) {
        smooth = false;
    }

    if (!srcSurf && opacity == 255 && !touchesTaintedArea)
    {
        /* Fast blit */
        // TODO: Use bitmapSmoothScaling/bitmapSmoothScalingDown configs for this.
        GLMeta::blitBegin(getGLTypes());
        GLMeta::blitSource(source.getGLTypes());
        GLMeta::blitRectangle(sourceRect, destRect, smooth);
        GLMeta::blitEnd();
    }
    else
    {
        if (srcSurf)
        {
            SDL_Rect srcRect = sourceRect;
#ifdef MKXPZ_RETRO
            bool subImageFix = mkxp_retro::sub_image_fix_override == 1 || (mkxp_retro::sub_image_fix_override != 0 && shState->config().subImageFix);
#else
            bool subImageFix = shState->config().subImageFix;
#endif // MKXPZ_RETRO
            bool srcRectTooBig = srcRect.w > glState.caps.maxTexSize ||
                                 srcRect.h > glState.caps.maxTexSize;
            bool srcSurfTooBig = !unpack_subimage && (
                                     srcSurf->w > glState.caps.maxTexSize || 
                                     srcSurf->h > glState.caps.maxTexSize
                                 );
            
            if (srcRectTooBig || srcSurfTooBig)
            {
#ifdef MKXPZ_RETRO // TODO
                MKXPZ_FORCED_ASSERT_WITH_MESSAGE(false, "not implemented: stretchBlt for sources larger than the max texture size");
#else
                int error;
                if (srcRectTooBig)
                {
                    /* We have to resize it here anyway, so use software resizing */
                    blitTemp =
                        SDL_CreateRGBSurface(0, abs(destRect.w), abs(destRect.h), p->format->BitsPerPixel,
                                             p->format->Rmask, p->format->Gmask,
                                             p->format->Bmask, p->format->Amask);
                    if (!blitTemp)
                        MKXPZ_THROW(std::bad_alloc());
                    
                    if (smooth)
                    {
                        error = SDL_SoftStretchLinear(srcSurf, &srcRect, blitTemp, 0);
                        smooth = false;
                    }
                    else
                    {
                        SDL_Rect tmpRect = {0, 0, blitTemp->w, blitTemp->h};
                        error = SDL_LowerBlitScaled(srcSurf, &srcRect, blitTemp, &tmpRect);
                    }
                    unpack_subimage = false;
                }
                else
                {
                    /* Just crop it, let the shader resize it later */
                    blitTemp =
                        SDL_CreateRGBSurface(0, sourceRect.w, sourceRect.h, p->format->BitsPerPixel,
                                             p->format->Rmask, p->format->Gmask,
                                             p->format->Bmask, p->format->Amask);
                    if (!blitTemp)
                        MKXPZ_THROW(std::bad_alloc());
                    
                    SDL_Rect tmpRect = {0, 0, blitTemp->w, blitTemp->h};
                    error = SDL_LowerBlit(srcSurf, &srcRect, blitTemp, &tmpRect);
                }
                
                if (error)
                {
                    SDL_FreeSurface(blitTemp);
                    exception = Exception(Exception::SDLError, "Failed to blit surface: %s", SDL_GetError());
                    return;
                }
                
                srcSurf = blitTemp;
                
                sourceRect.w = srcSurf->w;
                sourceRect.h = srcSurf->h;
                sourceRect.x = 0;
                sourceRect.y = 0;
#endif // MKXPZ_RETRO
            }
            
            if (opacity == 255 && !touchesTaintedArea)
            {
                if (!subImageFix &&
                    sourceRect.w == destRect.w && sourceRect.h == destRect.h &&
                    (unpack_subimage || (srcSurf->w == sourceRect.w && srcSurf->h == sourceRect.h))
                   )
                {
                    /* No scaling needed */
                    TEX::bind(getGLTypes().tex);
                    if (unpack_subimage)
                    {
                        gl.PixelStorei(GL_UNPACK_ROW_LENGTH, srcSurf->w);
                        gl.PixelStorei(GL_UNPACK_SKIP_PIXELS, sourceRect.x);
                        gl.PixelStorei(GL_UNPACK_SKIP_ROWS, sourceRect.y);
                    }
                    TEX::uploadSubImage(destRect.x, destRect.y,
                                        destRect.w, destRect.h,
                                        srcSurf->pixels, GL_RGBA);
                    
                    if (unpack_subimage)
                        GLMeta::subRectImageEnd();
                }
                else
                {
                    /* Resizing or subImageFix involved: need to use intermediary TexFBO */
                    TEXFBO *gpTF;
                    if (unpack_subimage)
                        gpTF = &shState->gpTexFBO(sourceRect.w, sourceRect.h);
                    else
                        gpTF = &shState->gpTexFBO(srcSurf->w, srcSurf->h);
                    TEX::bind(gpTF->tex);
                    
                    if (unpack_subimage)
                    {
                        gl.PixelStorei(GL_UNPACK_ROW_LENGTH, srcSurf->w);
                        gl.PixelStorei(GL_UNPACK_SKIP_PIXELS, sourceRect.x);
                        gl.PixelStorei(GL_UNPACK_SKIP_ROWS, sourceRect.y);
                        sourceRect.x = 0;
                        sourceRect.y = 0;
                        TEX::uploadSubImage(0, 0, sourceRect.w, sourceRect.h, srcSurf->pixels, GL_RGBA);
                        GLMeta::subRectImageEnd();
                    }
                    else
                    {
                        TEX::uploadSubImage(0, 0, srcSurf->w, srcSurf->h, srcSurf->pixels, GL_RGBA);
                    }
                    
                    GLMeta::blitBegin(getGLTypes());
                    GLMeta::blitSource(*gpTF);
                    GLMeta::blitRectangle(sourceRect, destRect, smooth);
                    GLMeta::blitEnd();
                }
            }
        }
        if (opacity < 255 || touchesTaintedArea)
        {
            /* We're touching a tainted area or still need to reduce opacity */
             
            /* Fragment pipeline */
            float normOpacity = (float) opacity / 255.0f;
            
            TEXFBO &gpTex = shState->gpTexFBO(abs(destRect.w), abs(destRect.h));
            Vec2i gpTexSize;
            
            GLMeta::blitBegin(gpTex, false, SameScale);
            GLMeta::blitSource(getGLTypes(), SameScale);
            GLMeta::blitRectangle(destRect, IntRect(0, 0, abs(destRect.w), abs(destRect.h)));
            GLMeta::blitEnd();
            
            int sourceWidth, sourceHeight;
            FloatRect bltSubRect;
            if (srcSurf)
            {
                if (unpack_subimage)
                {
                    shState->ensureTexSize(sourceRect.w, sourceRect.h, gpTexSize);
                }
                else
                {
                    shState->ensureTexSize(srcSurf->w, srcSurf->h, gpTexSize);
                }
                sourceWidth = gpTexSize.x;
                sourceHeight = gpTexSize.y;
                
                shState->bindTex();
                
                if (unpack_subimage)
                {
                    gl.PixelStorei(GL_UNPACK_ROW_LENGTH, srcSurf->w);
                    gl.PixelStorei(GL_UNPACK_SKIP_PIXELS, sourceRect.x);
                    gl.PixelStorei(GL_UNPACK_SKIP_ROWS, sourceRect.y);
                    sourceRect.x = 0;
                    sourceRect.y = 0;
                    
                    TEX::uploadSubImage(0, 0, sourceRect.w, sourceRect.h, srcSurf->pixels, GL_RGBA);
                    GLMeta::subRectImageEnd();
                }
                else
                {
                    TEX::uploadSubImage(0, 0, srcSurf->w, srcSurf->h, srcSurf->pixels, GL_RGBA);
                }
            }
            else
            {
                sourceWidth = source.width();
                sourceHeight = source.height();
            }
            bltSubRect = FloatRect((float) sourceRect.x / sourceWidth,
                                   (float) sourceRect.y / sourceHeight,
                                   ((float) sourceWidth / sourceRect.w) * ((float) abs(destRect.w) / gpTex.width),
                                   ((float) sourceHeight / sourceRect.h) * ((float) abs(destRect.h) / gpTex.height));
            
            BltShader &shader = shState->shaders().blt;
            shader.bind();
            if (srcSurf)
            {
                shader.setTexSize(gpTexSize);
            }
            else
            {
                source.p->bindTexture(shader, false);
            }
            shader.setSource();
            shader.setDestination(gpTex.tex);
            shader.setSubRect(bltSubRect);
            shader.setOpacity(normOpacity);
            
            Quad &quad = shState->gpQuad();
            quad.setTexPosRect(sourceRect, destRect);
            quad.setColor(Vec4(1, 1, 1, normOpacity));
            
            p->bindFBO();
            p->pushSetViewport(shader);
            
            if (smooth)
                TEX::setSmooth(true);
            
            p->blitQuad(quad);
            
            p->popViewport();
            
            if (smooth)
                TEX::setSmooth(false);
        }
    }
    
    if (blitTemp)
#ifdef MKXPZ_RETRO
    {
        stbi_image_free(blitTemp->pixels);
        delete blitTemp;
    }
#else
        SDL_FreeSurface(blitTemp);
#endif // MKXPZ_RETRO

#ifdef MKXPZ_RETRO
    p->pushDiff(destRect);
#endif // MKXPZ_RETRO

    p->addTaintedArea(destRect);
    p->onModified();
}

void Bitmap::fillRect(Exception &exception,
                      int x, int y,
                      int width, int height,
                      const Vec4 &color)
{
    GUARD(fillRect(exception, IntRect(x, y, width, height), color));
}

void Bitmap::fillRect(Exception &exception, const IntRect &rect, const Vec4 &color)
{
    GUARD(guardDisposed(exception));
    
    GUARD_MEGA();
    GUARD_ANIMATED();
    
    if (hasHires()) {
        int destX, destY, destWidth, destHeight;
        destX = rect.x * p->selfHires->width() / width();
        destY = rect.y * p->selfHires->height() / height();
        destWidth = rect.w * p->selfHires->width() / width();
        destHeight = rect.h * p->selfHires->height() / height();

        GUARD(p->selfHires->fillRect(exception, IntRect(destX, destY, destWidth, destHeight), color));
    }

    p->fillRect(rect, color);

#ifdef MKXPZ_RETRO
    uint32_t *pixels = (uint32_t *)STBI_MALLOC(4 * rect.w * rect.h);
    if (pixels == nullptr)
        MKXPZ_THROW(std::bad_alloc());
    const uint8_t pixel[4] = {
        (uint8_t)clamp(color.x * 255.0f, 0.0f, 255.0f),
        (uint8_t)clamp(color.y * 255.0f, 0.0f, 255.0f),
        (uint8_t)clamp(color.z * 255.0f, 0.0f, 255.0f),
        (uint8_t)clamp(color.w * 255.0f, 0.0f, 255.0f),
    };
    for (size_t i = 0; i < (size_t)rect.w * (size_t)rect.h; ++i)
        std::memcpy(pixels + i, pixel, 4);
    p->pushDiff(pixels, rect);
    stbi_image_free(pixels);
#endif // MKXPZ_RETRO
    
    if (color.w == 0)
    /* Clear op */
        p->substractTaintedArea(rect);
    else
    /* Fill op */
        p->addTaintedArea(rect);
    
    p->onModified();
}

void Bitmap::gradientFillRect(Exception &exception,
                              int x, int y,
                              int width, int height,
                              const Vec4 &color1, const Vec4 &color2,
                              bool vertical)
{
    GUARD(gradientFillRect(exception, IntRect(x, y, width, height), color1, color2, vertical));
}

void Bitmap::gradientFillRect(Exception &exception,
                              const IntRect &rect,
                              const Vec4 &color1, const Vec4 &color2,
                              bool vertical)
{
    GUARD(guardDisposed(exception));
    
    GUARD_MEGA();
    GUARD_ANIMATED();
    
    if (hasHires()) {
        int destX, destY, destWidth, destHeight;
        destX = rect.x * p->selfHires->width() / width();
        destY = rect.y * p->selfHires->height() / height();
        destWidth = rect.w * p->selfHires->width() / width();
        destHeight = rect.h * p->selfHires->height() / height();

        GUARD(p->selfHires->gradientFillRect(exception, IntRect(destX, destY, destWidth, destHeight), color1, color2, vertical));
    }

    SimpleColorShader &shader = shState->shaders().simpleColor;
    shader.bind();
    shader.setTranslation(Vec2i());
    
    Quad &quad = shState->gpQuad();
    
    if (vertical)
    {
        quad.vert[0].color = color1;
        quad.vert[1].color = color1;
        quad.vert[2].color = color2;
        quad.vert[3].color = color2;
    }
    else
    {
        quad.vert[0].color = color1;
        quad.vert[3].color = color1;
        quad.vert[1].color = color2;
        quad.vert[2].color = color2;
    }
    
    quad.setPosRect(rect);
    
    p->bindFBO();
    p->pushSetViewport(shader);
    
    p->blitQuad(quad);
    
    p->popViewport();

#ifdef MKXPZ_RETRO
    p->pushDiff(rect);
#endif // MKXPZ_RETRO

    p->addTaintedArea(rect);
    
    p->onModified();
}

void Bitmap::clearRect(Exception &exception, int x, int y, int width, int height)
{
    GUARD(clearRect(exception, IntRect(x, y, width, height)));
}

void Bitmap::clearRect(Exception &exception, const IntRect &rect)
{
    GUARD(guardDisposed(exception));
    
    GUARD_MEGA();
    GUARD_ANIMATED();
    
    if (hasHires()) {
        int destX, destY, destWidth, destHeight;
        destX = rect.x * p->selfHires->width() / width();
        destY = rect.y * p->selfHires->height() / height();
        destWidth = rect.w * p->selfHires->width() / width();
        destHeight = rect.h * p->selfHires->height() / height();

        GUARD(p->selfHires->clearRect(exception, IntRect(destX, destY, destWidth, destHeight)));
    }

    p->fillRect(rect, Vec4());

#ifdef MKXPZ_RETRO
    void *pixels = STBI_MALLOC(4 * rect.w * rect.h);
    if (pixels == nullptr)
        MKXPZ_THROW(std::bad_alloc());
    std::memset(pixels, 0, 4 * rect.w * rect.h);
    p->pushDiff(pixels, rect);
    stbi_image_free(pixels);
#endif // MKXPZ_RETRO
    
    p->onModified();
}

void Bitmap::blur(Exception &exception)
{
    GUARD(guardDisposed(exception));
    
    GUARD_MEGA();
    GUARD_ANIMATED();
    
    if (hasHires()) {
        GUARD(p->selfHires->blur(exception));
    }

    // TODO: Is there some kind of blur radius that we need to handle for high-res mode?

    Quad &quad = shState->gpQuad();
    FloatRect rect(0, 0, width(), height());
    quad.setTexPosRect(rect, rect);
    
    TEXFBO auxTex;
    GUARD(auxTex = shState->texPool().request(exception, width(), height()));
    
    BlurShader &shader = shState->shaders().blur;
    BlurShader::HPass &pass1 = shader.pass1;
    BlurShader::VPass &pass2 = shader.pass2;
    
    glState.blend.pushSet(false);
    glState.viewport.pushSet(IntRect(0, 0, width(), height()));
    
    TEX::bind(p->gl.tex);
    FBO::bind(auxTex.fbo);
    
    pass1.bind();
    pass1.setTexSize(Vec2i(width(), height()));
    pass1.applyViewportProj();
    
    quad.draw();
    
    TEX::bind(auxTex.tex);
    p->bindFBO();
    
    pass2.bind();
    pass2.setTexSize(Vec2i(width(), height()));
    pass2.applyViewportProj();
    
    quad.draw();
    
    glState.viewport.pop();
    glState.blend.pop();
    
    shState->texPool().release(auxTex);

#ifdef MKXPZ_RETRO
    p->pushDiff(this->rect());
#endif // MKXPZ_RETRO

    p->onModified();
}

void Bitmap::radialBlur(Exception &exception, int angle, int divisions)
{
    GUARD(guardDisposed(exception));
    
    GUARD_MEGA();
    GUARD_ANIMATED();
    
    if (hasHires()) {
        GUARD(p->selfHires->radialBlur(exception, angle, divisions));
        return;
    }

    angle     = clamp<int>(angle, 0, 359);
    divisions = clamp<int>(divisions, 2, 100);
    
    const int _width = width();
    const int _height = height();
    
    float angleStep = (float) angle / (divisions-1);
    float opacity   = 1.0f / divisions;
    float baseAngle = -((float) angle / 2);
    
    ColorQuadArray qArray;
    qArray.resize(5);
    
    std::vector<Vertex> &vert = qArray.vertices;
    
    int i = 0;
    
    /* Center */
    FloatRect texRect(0, 0, _width, _height);
    FloatRect posRect(0, 0, _width, _height);
    
    i += Quad::setTexPosRect(&vert[i*4], texRect, posRect);
    
    /* Upper */
    posRect = FloatRect(0, 0, _width, -_height);
    
    i += Quad::setTexPosRect(&vert[i*4], texRect, posRect);
    
    /* Lower */
    posRect = FloatRect(0, _height*2, _width, -_height);
    
    i += Quad::setTexPosRect(&vert[i*4], texRect, posRect);
    
    /* Left */
    posRect = FloatRect(0, 0, -_width, _height);
    
    i += Quad::setTexPosRect(&vert[i*4], texRect, posRect);
    
    /* Right */
    posRect = FloatRect(_width*2, 0, -_width, _height);
    
    i += Quad::setTexPosRect(&vert[i*4], texRect, posRect);
    
    for (int i = 0; i < 4*5; ++i)
        vert[i].color = Vec4(1, 1, 1, opacity);
    
    qArray.commit();
    
    TEXFBO newTex;
    GUARD(newTex = shState->texPool().request(exception, _width, _height));
    
    FBO::bind(newTex.fbo);
    
    glState.clearColor.pushSet(Vec4());
    FBO::clear();
    
    Transform trans;
    trans.setOrigin(Vec2(_width / 2.0f, _height / 2.0f));
    trans.setPosition(Vec2(_width / 2.0f, _height / 2.0f));
    
    glState.blendMode.pushSet(BlendAddition);
    
    SimpleMatrixShader &shader = shState->shaders().simpleMatrix;
    shader.bind();
    
    p->bindTexture(shader, false);
    TEX::setSmooth(true);
    
    p->pushSetViewport(shader);
    
    for (int i = 0; i < divisions; ++i)
    {
        trans.setRotation(baseAngle + i*angleStep);
        shader.setMatrix(trans.getMatrix());
        qArray.draw();
    }
    
    p->popViewport();
    
    TEX::setSmooth(false);
    
    glState.blendMode.pop();
    glState.clearColor.pop();
    
    shState->texPool().release(p->gl);
    p->gl = newTex;

#ifdef MKXPZ_RETRO
    p->pushDiff(rect());
#endif // MKXPZ_RETRO

    p->onModified();
}

void Bitmap::clear(Exception &exception)
{
    GUARD(guardDisposed(exception));
    
    GUARD_MEGA();
    GUARD_ANIMATED();
    
    if (hasHires()) {
        GUARD(p->selfHires->clear(exception));
    }

    p->bindFBO();
    
    glState.clearColor.pushSet(Vec4());
    
    FBO::clear();
    
    glState.clearColor.pop();

#ifdef MKXPZ_RETRO
    if (p->animation.enabled)
    {
        if (!p->animation.currentFrame().diff.empty())
        {
            p->animation.currentFrame().diff.clear();
            p->animation.currentFrame().diff.resize(CEIL_DIV_DIFF_TILE_SIZE(width()) * CEIL_DIV_DIFF_TILE_SIZE(height()));
        }
    }
    else
    {
        if (!p->diff.empty())
        {
            p->diff.clear();
            p->diff.resize(CEIL_DIV_DIFF_TILE_SIZE(width()) * CEIL_DIV_DIFF_TILE_SIZE(height()));
        }
        p->path.clear();
    }
#endif // MKXPZ_RETRO

    p->clearTaintedArea();
    
    p->onModified();
}

#ifndef MKXPZ_RETRO
static uint32_t &getPixelAt(SDL_Surface *surf, SDL_PixelFormat *form, int x, int y)
{
    size_t offset = x*form->BytesPerPixel + y*surf->pitch;
    uint8_t *bytes = (uint8_t*) surf->pixels + offset;
    
    return *((uint32_t*) bytes);
}
#endif // MKXPZ_RETRO

Color Bitmap::getPixel(Exception &exception, int x, int y) const
{
    GUARD_V(Color(), guardDisposed(exception));
    
    GUARD_MEGA(Color());
    GUARD_ANIMATED(Color());
    
    if (hasHires()) {
        Debug() << "GAME BUG: Game is calling getPixel on low-res Bitmap; you may want to patch the game to improve graphics quality.";

        int xHires = x * p->selfHires->width() / width();
        int yHires = y * p->selfHires->height() / height();

        // We take the average color from the high-res Bitmap.
        // RGB channels skip fully transparent pixels when averaging.
        int w = p->selfHires->width() / width();
        int h = p->selfHires->height() / height();

        if (w >= 1 && h >= 1) {
            double rSum = 0.;
            double gSum = 0.;
            double bSum = 0.;
            double aSum = 0.;

            long long rgbCount = 0;
            long long aCount = 0;

            for (int thisX = xHires; thisX < xHires+w && thisX < p->selfHires->width(); thisX++) {
                for (int thisY = yHires; thisY < yHires+h && thisY < p->selfHires->height(); thisY++) {
                    Color thisColor;
                    GUARD_V(Color(), thisColor = p->selfHires->getPixel(exception, thisX, thisY));
                    if (thisColor.getAlpha() >= 1.0) {
                        rSum += thisColor.getRed();
                        gSum += thisColor.getGreen();
                        bSum += thisColor.getBlue();
                        rgbCount++;
                    }
                    aSum += thisColor.getAlpha();
                    aCount++;
                }
            }

            double rAvg = rSum / (double)rgbCount;
            double gAvg = gSum / (double)rgbCount;
            double bAvg = bSum / (double)rgbCount;
            double aAvg = aSum / (double)aCount;

            return Color(rAvg, gAvg, bAvg, aAvg);
        }
    }

    if (x < 0 || y < 0 || x >= width() || y >= height())
        return Vec4();

    if (!p->surface)
    {
        p->allocSurface();
        
        FBO::bind(p->gl.fbo);
        
        glState.viewport.pushSet(IntRect(0, 0, width(), height()));
        
        gl.ReadPixels(0, 0, width(), height(), GL_RGBA, GL_UNSIGNED_BYTE, p->surface->pixels);
        
        glState.viewport.pop();
    }
    
#ifdef MKXPZ_RETRO
    return Color(((uint8_t *)p->surface->pixels)[4 * (p->surface->w * y + x)],
                 ((uint8_t *)p->surface->pixels)[4 * (p->surface->w * y + x) + 1],
                 ((uint8_t *)p->surface->pixels)[4 * (p->surface->w * y + x) + 2],
                 ((uint8_t *)p->surface->pixels)[4 * (p->surface->w * y + x) + 3]);
#else
    uint32_t pixel = getPixelAt(p->surface, p->format, x, y);
    
    return Color((pixel >> p->format->Rshift) & 0xFF,
                 (pixel >> p->format->Gshift) & 0xFF,
                 (pixel >> p->format->Bshift) & 0xFF,
                 (pixel >> p->format->Ashift) & 0xFF);
#endif // MKXPZ_RETRO
}

void Bitmap::setPixel(Exception &exception, int x, int y, const Color &color)
{
    GUARD(guardDisposed(exception));
    
    GUARD_MEGA();
    GUARD_ANIMATED();
    
    if (hasHires()) {
        Debug() << "GAME BUG: Game is calling setPixel on low-res Bitmap; you may want to patch the game to improve graphics quality.";

        int xHires = x * p->selfHires->width() / width();
        int yHires = y * p->selfHires->height() / height();

        int w = p->selfHires->width() / width();
        int h = p->selfHires->height() / height();

        if (w >= 1 && h >= 1) {
            for (int thisX = xHires; thisX < xHires+w && thisX < p->selfHires->width(); thisX++) {
                for (int thisY = yHires; thisY < yHires+h && thisY < p->selfHires->height(); thisY++) {
                    GUARD(p->selfHires->setPixel(exception, thisX, thisY, color));
                }
            }
        }
    }

    uint8_t pixel[] =
    {
        (uint8_t) clamp<double>(color.red,   0, 255),
        (uint8_t) clamp<double>(color.green, 0, 255),
        (uint8_t) clamp<double>(color.blue,  0, 255),
        (uint8_t) clamp<double>(color.alpha, 0, 255)
    };
    
    TEX::bind(p->gl.tex);
    TEX::uploadSubImage(x, y, 1, 1, &pixel, GL_RGBA);
    
    p->addTaintedArea(IntRect(x, y, 1, 1));
    
    /* Setting just a single pixel is no reason to throw away the
     * whole cached surface; we can just apply the same change */
    
#ifndef MKXPZ_RETRO // TODO
    if (p->surface)
    {
        uint32_t &surfPixel = getPixelAt(p->surface, p->format, x, y);
        surfPixel = SDL_MapRGBA(p->format, pixel[0], pixel[1], pixel[2], pixel[3]);
    }
#endif // MKXPZ_RETRO

#ifdef MKXPZ_RETRO
    p->pushDiff(pixel, IntRect(x, y, 1, 1));
#endif // MKXPZ_RETRO

    p->onModified(false);
}

bool Bitmap::getRaw(Exception &exception, void *output, int output_size)
{
    if (output_size != width()*height()*4) return false;
    
    GUARD_V(false, guardDisposed(exception));
    
    if (hasHires()) {
        Debug() << "GAME BUG: Game is calling getRaw on low-res Bitmap; you may want to patch the game to improve graphics quality.";
    }

    if (!p->animation.enabled && (p->surface || p->megaSurface)) {
        void *src = (p->megaSurface) ? p->megaSurface->pixels : p->surface->pixels;
        memcpy(output, src, output_size);
    }
    else {
        FBO::bind(getGLTypes().fbo);
        gl.ReadPixels(0,0,width(),height(),GL_RGBA,GL_UNSIGNED_BYTE,output);
    }

    return true;
}

void Bitmap::replaceRaw(Exception &exception, void *pixel_data, int size)
{
    GUARD(guardDisposed(exception));
    
    GUARD_MEGA();
    
    if (hasHires()) {
        Debug() << "GAME BUG: Game is calling replaceRaw on low-res Bitmap; you may want to patch the game to improve graphics quality.";
    }

    int w = width();
    int h = height();
    int requiredsize = w*h*4;
    
    if (size != w*h*4) {
        exception = Exception(Exception::MKXPError, "Replacement bitmap data is not large enough (given %i bytes, need %i)", size, requiredsize);
        return;
    }
    
    TEX::bind(getGLTypes().tex);
    TEX::uploadImage(w, h, pixel_data, GL_RGBA);

#ifdef MKXPZ_RETRO
    p->pushDiff(pixel_data, rect());
#endif // MKXPZ_RETRO

    taintArea(IntRect(0,0,w,h));
    p->onModified();
}

void Bitmap::saveToFile(Exception &exception, const char *filename)
{
    GUARD(guardDisposed(exception));
    
    if (hasHires()) {
        Debug() << "GAME BUG: Game is calling saveToFile on low-res Bitmap; you may want to patch the game to improve graphics quality.";
    }

#ifndef MKXPZ_RETRO // TODO: implement
    SDL_Surface *surf;
    
    if (p->surface || p->megaSurface) {
        surf = (p->surface) ? p->surface : p->megaSurface;
    }
    else {
        surf = SDL_CreateRGBSurface(0, width(), height(),p->format->BitsPerPixel, p->format->Rmask,p->format->Gmask,p->format->Bmask,p->format->Amask);
        
        if (!surf)
            MKXPZ_THROW(std::bad_alloc());
        
        GUARD(getRaw(exception, surf->pixels, surf->w * surf->h * 4));
    }
    
    // Try and determine the intended image format from the filename extension
    const char *period = strrchr(filename, '.');
    int filetype = 0;
    if (period) {
        period++;
        std::string ext;
        for (int i = 0; i < (int)strlen(period); i++) {
            ext += tolower(period[i]);
        }
        
        if (!ext.compare("png")) {
            filetype = 1;
        }
        else if (!ext.compare("jpg") || !ext.compare("jpeg")) {
            filetype = 2;
        }
    }
    
    std::string fn_normalized = shState->fileSystem().normalize(filename, 1, 1);
    int rc;
    switch (filetype) {
        case 2:
            rc = IMG_SaveJPG(surf, fn_normalized.c_str(), 90);
            break;
        case 1:
            rc = IMG_SavePNG(surf, fn_normalized.c_str());
            break;
        case 0: default:
            rc = SDL_SaveBMP(surf, fn_normalized.c_str());
            break;
    }
    
    if (!p->surface && !p->megaSurface)
        SDL_FreeSurface(surf);
    
    if (rc) {
        exception = Exception(Exception::SDLError, "%s", SDL_GetError());
        return;
    }
#endif // MKXPZ_RETRO
}

void Bitmap::hueChange(Exception &exception, int hue)
{
    GUARD(guardDisposed(exception));
    
    GUARD_MEGA();
    GUARD_ANIMATED();
    
    if (hasHires()) {
        GUARD(p->selfHires->hueChange(exception, hue));
        return;
    }

    if ((hue % 360) == 0)
        return;
    
    TEXFBO newTex;
    GUARD(newTex = shState->texPool().request(exception, width(), height()));
    
    FloatRect texRect(rect());
    
    Quad &quad = shState->gpQuad();
    quad.setTexPosRect(texRect, texRect);
    quad.setColor(Vec4(1, 1, 1, 1));
    
    HueShader &shader = shState->shaders().hue;
    shader.bind();
    /* Shader expects normalized value */
    shader.setHueAdjust(wrapRange(hue, 0, 359) / 360.0f);
    
    FBO::bind(newTex.fbo);
    p->pushSetViewport(shader);
    p->bindTexture(shader, false);
    
    p->blitQuad(quad);
    
    p->popViewport();
    
    TEX::unbind();
    
    shState->texPool().release(p->gl);
    p->gl = newTex;

#ifdef MKXPZ_RETRO
    p->pushDiff(rect());
#endif // MKXPZ_RETRO

    p->onModified();
}

void Bitmap::drawText(Exception &exception,
                      int x, int y,
                      int width, int height,
                      const char *str, int align)
{
    GUARD(drawText(exception, IntRect(x, y, width, height), str, align));
}

static std::string fixupString(const char *str)
{
    std::string s(str);
    
    /* RMXP actually draws LF as a "missing gylph" box,
     * but since we might have accidentally converted CRs
     * to LFs when editing scripts on a Unix OS, treat them
     * as white space too */
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == '\r' || s[i] == '\n')
            s[i] = ' ';
    
    return s;
}

#ifdef MKXPZ_RETRO
static void applyShadow(SDL_Surface *&in, const SDL_Color &c, int offset)
#else
static void applyShadow(SDL_Surface *&in, const SDL_PixelFormat &fm, const SDL_Color &c, int offset)
#endif // MKXPZ_RETRO
{
#ifdef MKXPZ_RETRO
    SDL_Surface *out = new SDL_Surface {in->w+offset, in->h+offset, STBI_MALLOC(4 * (in->w+offset) * (in->h+offset))};
    if (out->pixels == nullptr) {
        MKXPZ_THROW(std::bad_alloc());
    }
    const int inPitch = 4 * in->w;
    const int outPitch = 4 * out->w;
#else
    SDL_Surface *out = SDL_CreateRGBSurface
    (0, in->w+1, in->h+1, fm.BitsPerPixel, fm.Rmask, fm.Gmask, fm.Bmask, fm.Amask);
    const int inPitch = in->pitch;
    const int outPitch = out->pitch;
#endif // MKXPZ_RETRO
    
    float fr = c.r / 255.0f;
    float fg = c.g / 255.0f;
    float fb = c.b / 255.0f;
    
    /* We allocate an output surface one pixel wider and higher than the input,
     * (implicitly) blit a copy of the input with RGB values set to black into
     * it with x/y offset by 1, then blend the input surface over it at origin
     * (0,0) using the bitmap blit equation (see shader/bitmapBlit.frag) */
    
    for (int y = 0; y < in->h+offset; ++y)
        for (int x = 0; x < in->w+offset; ++x)
        {
            /* src: input pixel, shd: shadow pixel */
            uint32_t src = 0, shd = 0;
            
            /* Output pixel location */
            uint32_t *outP = ((uint32_t*) ((uint8_t*) out->pixels + y*outPitch)) + x;
            
            if (y < in->h && x < in->w)
                src = ((uint32_t*) ((uint8_t*) in->pixels + y*inPitch))[x];
            
            if (y >= offset && x >= offset)
                shd = ((uint32_t*) ((uint8_t*) in->pixels + (y-offset)*inPitch))[x-offset];
            
            /* Set shadow pixel RGB values to 0 (black) */
#ifdef MKXPZ_RETRO
#  ifdef MKXPZ_BIG_ENDIAN
            shd &= 0x000000ffU;
#  else
            shd &= 0xff000000U;
#  endif // MKXPZ_BIG_ENDIAN
#else
            shd &= fm.Amask;
#endif // MKXPZ_RETRO
            
            if (x < offset || y < offset)
            {
                *outP = src;
                continue;
            }
            
            if (x >= in->w || y >= in->h)
            {
                *outP = shd;
                continue;
            }
            
            /* Input and shadow alpha values */
            uint8_t srcA, shdA;
#ifdef MKXPZ_RETRO
#  ifdef MKXPZ_BIG_ENDIAN
            srcA = (src & 0x000000ffU);
            shdA = (shd & 0x000000ffU);
#  else
            srcA = (src & 0xff000000U) >> 24;
            shdA = (shd & 0xff000000U) >> 24;
#  endif // MKXPZ_BIG_ENDIAN
#else
            srcA = (src & fm.Amask) >> fm.Ashift;
            shdA = (shd & fm.Amask) >> fm.Ashift;
#endif // MKXPZ_RETRO
            
            if (srcA == 255 || shdA == 0)
            {
                *outP = src;
                continue;
            }
            
            if (srcA == 0 && shdA == 0)
            {
                *outP = 0;
                continue;
            }
            
            float fSrcA = srcA / 255.0f;
            float fShdA = shdA / 255.0f;
            
            /* Because opacity == 1, co1 == fSrcA */
            float co2 = fShdA * (1.0f - fSrcA);
            /* Result alpha */
            float fa = fSrcA + co2;
            /* Temp value to simplify arithmetic below */
            float co3 = fSrcA / fa;
            
            /* Result colors */
            uint8_t r, g, b, a;
            
            r = clamp<float>(fr * co3, 0, 1) * 255.0f;
            g = clamp<float>(fg * co3, 0, 1) * 255.0f;
            b = clamp<float>(fb * co3, 0, 1) * 255.0f;
            a = clamp<float>(fa, 0, 1) * 255.0f;
            
#ifdef MKXPZ_RETRO
            ((uint8_t *)outP)[0] = r;
            ((uint8_t *)outP)[1] = g;
            ((uint8_t *)outP)[2] = b;
            ((uint8_t *)outP)[3] = a;
#else
            *outP = SDL_MapRGBA(&fm, r, g, b, a);
#endif // MKXPZ_RETRO
        }
    
    /* Store new surface in the input pointer */
#ifdef MKXPZ_RETRO
    stbi_image_free(in->pixels);
    delete in;
#else
    SDL_FreeSurface(in);
#endif // MKXPZ_RETRO
    in = out;
}

/* An implementation of the bitmap blit equation (see shader/bitmapBlit.frag),
 * modified for combining text with its outline. */
static inline void blendText(SDL_Surface *txtSrf, const SDL_Rect &inRect, const SDL_Color &inColor,
                           SDL_Surface *outSrf, const SDL_Rect &outRect, const SDL_Color &outColor, bool hasShadow)
{
#ifdef MKXPZ_RETRO
    const size_t txtSrfBytesPerPixel = 4;
    const size_t outSrfBytesPerPixel = 4;
    const size_t txtSrfPitch = (size_t)4 * (size_t)txtSrf->w;
    const size_t outSrfPitch = (size_t)4 * (size_t)outSrf->w;
#  ifdef MKXPZ_BIG_ENDIAN
    const size_t txtSrfRshift = 24;
    const size_t txtSrfGshift = 16;
    const size_t txtSrfBshift = 8;
    const size_t txtSrfAshift = 0;
    const size_t outSrfAshift = 0;
#  else
    const size_t txtSrfRshift = 0;
    const size_t txtSrfGshift = 8;
    const size_t txtSrfBshift = 16;
    const size_t txtSrfAshift = 24;
    const size_t outSrfAshift = 24;
#  endif // MKXPZ_BIG_ENDIAN
#else
    const size_t txtSrfBytesPerPixel = txtSrf->format->BytesPerPixel;
    const size_t outSrfBytesPerPixel = outSrf->format->BytesPerPixel;
    const size_t txtSrfPitch = (size_t)4 * (size_t)txtSrf->pitch;
    const size_t outSrfPitch = (size_t)4 * (size_t)outSrf->pitch;
    const size_t txtSrfRshift = txtSrf->format->Rshift;
    const size_t txtSrfGshift = txtSrf->format->Gshift;
    const size_t txtSrfBshift = txtSrf->format->Bshift;
    const size_t txtSrfAshift = txtSrf->format->Ashift;
    const size_t outSrfAshift = outSrf->format->Ashift;
#endif // MKXPZ_RETRO

    size_t offset = (inRect.x * txtSrfBytesPerPixel) + (inRect.y * txtSrfPitch);
    uint8_t *txtStart = (uint8_t*)txtSrf->pixels + offset;
    offset = (outRect.x * outSrfBytesPerPixel) + (outRect.y * outSrfPitch);
    uint8_t *outStart = (uint8_t*)outSrf->pixels + offset;
    
    // SDL_TTF sets every pixel to the same RGB value and just adjusts the alpha
    float txtR = inColor.r;
    float txtG = inColor.g;
    float txtB = inColor.b;
    float outR = outColor.r;
    float outG = outColor.g;
    float outB = outColor.b;
    
    /* SDL_ttf blends the glyphs together, which causes overlapping
     * transparent pixels to get too opaque. RGSS probably does it, too,
     * and you'd probably have to zoom in to see it, but if you do see it
     * then it looks kind of ugly so we'll fix it.
     * I don't know if it can actually happen for non-outline text,
     * but we'll handle it, too, just in case.
     * We don't do it for non-outline text if there's a shadow, because I'm not sure how to do this workaround with shadows. */
#ifdef MKXPZ_RETRO
    uint32_t fullTxtPixel;
    ((uint8_t *)&fullTxtPixel)[0] = inColor.r;
    ((uint8_t *)&fullTxtPixel)[1] = inColor.g;
    ((uint8_t *)&fullTxtPixel)[2] = inColor.b;
    ((uint8_t *)&fullTxtPixel)[3] = inColor.a;
    uint32_t fullOutPixel;
    ((uint8_t *)&fullOutPixel)[0] = outColor.r;
    ((uint8_t *)&fullOutPixel)[1] = outColor.g;
    ((uint8_t *)&fullOutPixel)[2] = outColor.b;
    ((uint8_t *)&fullOutPixel)[3] = outColor.a;
#else
    uint32_t fullTxtPixel = SDL_MapRGBA(outSrf->format, inColor.r, inColor.g, inColor.b, inColor.a);
    uint32_t fullOutPixel = SDL_MapRGBA(outSrf->format, outColor.r, outColor.g, outColor.b, outColor.a);
#endif // MKXPZ_RETRO
    
    for (int i=0; i < inRect.h; ++i)
    {
        uint32_t *txtPixel = (uint32_t*)(txtStart + i*txtSrfPitch);
        uint32_t *outPixel = (uint32_t*)(outStart + i*outSrfPitch);
        for (int j=0; j < inRect.w; ++j)
        {
            uint8_t txtA = (*txtPixel >> txtSrfAshift) & 0xFF;
            uint8_t outA = (*outPixel >> outSrfAshift) & 0xFF;
            
            if (txtA >= inColor.a)
            {
                if (hasShadow)
                {
                    *outPixel = *txtPixel;
                }
                else
                {
                    *outPixel = fullTxtPixel;
                }
            } else if (outA == 0) {
                *outPixel = *txtPixel;
            } else if (txtA != 0) {
                /* Use the full text opacity instead of 255. */
                int32_t co1 = (int)txtA * inColor.a;
                int32_t co2 = (int)std::min(outA, outColor.a) * (inColor.a - txtA);
                
                /* Result alpha */
                int32_t fa = co1 + co2;
                
                /* Result colors */
                uint8_t r, g, b, a;
                
                float faInv = 1.0f / fa;
                float co3 = co1 * faInv;
                float co4 = co2 * faInv;

                if (hasShadow)
                {
                    txtR = (*txtPixel >> txtSrfRshift) & 0xFF;
                    txtG = (*txtPixel >> txtSrfGshift) & 0xFF;
                    txtB = (*txtPixel >> txtSrfBshift) & 0xFF;
                }

                // Adding a small number to combat floating point errors.
                r = std::min<int>((txtR * co3 + outR * co4) + 0.001f, 255);
                g = std::min<int>((txtG * co3 + outG * co4) + 0.001f, 255);
                b = std::min<int>((txtB * co3 + outB * co4) + 0.001f, 255);
                
                /* RGSS seems to not round, but our blit shader seemingly does. */
                a = fa / inColor.a;
                
#ifdef MKXPZ_RETRO
                ((uint8_t *)outPixel)[0] = r;
                ((uint8_t *)outPixel)[1] = g;
                ((uint8_t *)outPixel)[2] = b;
                ((uint8_t *)outPixel)[3] = a;
#else
                *outPixel = SDL_MapRGBA(outSrf->format, r, g, b, a);
#endif // MKXPZ_RETRO
            } else if (outA > outColor.a) {
                /* SDL_ttf blends the glyphs together, which causes overlapping
                 * transparent pixels to get too opaque. */
                *outPixel = fullOutPixel;
            }
            
            ++txtPixel;
            ++outPixel;
        }
    }
}

#define UTF8_PARSER_ERROR_BIT 0x80000000U

struct Utf8Parser
{
    inline Utf8Parser() : continuation_length(0), continuation_counter(0) {}

    inline uint32_t operator()(uint8_t byte)
    {
        error = false;

        if ((byte & 0b11000000U) == 0b10000000U)
        {
            if (continuation_counter == 0)
            {
                error = true;
                codepoint = 0;
            }

            else
            {
                codepoint <<= 6;
                codepoint |= byte & 0b00111111U;
                --continuation_counter;
            }
        }
        else
        {
            if (continuation_counter != 0)
                error = true;

            if ((byte & 0b10000000U) == 0b00000000U)
            {
                codepoint = byte & 0b01111111U;
                continuation_counter = continuation_length = 0;
            }

            else if ((byte & 0b11100000U) == 0b11000000U)
            {
                codepoint = byte & 0b00011111U;
                continuation_counter = continuation_length = 1;
            }

            else if ((byte & 0b11110000U) == 0b11100000U)
            {
                codepoint = byte & 0b00001111U;
                continuation_counter = continuation_length = 2;
            }

            else if ((byte & 0b11111000U) == 0b11110000U)
            {
                codepoint = byte & 0b00000111U;
                continuation_counter = continuation_length = 3;
            }

            else
            {
                error = true;
                codepoint = 0;
                continuation_counter = continuation_length = 0;
            }
        }

        // Disallow invalid code points and overlong encodings
        if (continuation_counter == 0)
        {
            if (codepoint > 0x10ffffU)
            {
                error = true;
                codepoint = 0;
            }

            else if (codepoint >= 0xd800U && codepoint <= 0xdfffU)
            {
                error = true;
                codepoint = 0;
            }

            else if (continuation_length == 1 && codepoint < 0x80U)
            {
                error = true;
                codepoint = 0;
            }

            else if (continuation_length == 2 && codepoint < 0x800U)
            {
                error = true;
                codepoint = 0;
            }

            else if (continuation_length == 3 && codepoint < 0x10000U)
            {
                error = true;
                codepoint = 0;
            }

            continuation_length = 0;
        }

        return (error ? UTF8_PARSER_ERROR_BIT : 0U) | (continuation_counter == 0 ? codepoint : 0U);
    }

private:
    uint32_t codepoint;
    uint8_t continuation_length;
    uint8_t continuation_counter;
    bool error;
};

#ifdef MKXPZ_RETRO
/* Returns the bounding box, in pixels, when the maximum possible number of
 * UTF-8 codepoints from `str` that fit in a bounding box at most `max_width`
 * pixels wide is drawn with the given font. Also returns the number of UTF-8
 * codepoints that fit in the bounding box. */
static std::pair<IntRect, size_t> textRect(FT_Face font, const char *str, bool solid, bool bold, bool italic, int max_width = INT_MAX)
{
    const unsigned short bold_width = bold ? GET_BOLD_WIDTH(font) : 0;
    const unsigned short italic_width = italic ? GET_ITALIC_WIDTH(font) : 0;
    int bitmap_left = 0;
    int bitmap_right = 0;
    int bitmap_top = -font->size->metrics.ascender / 64;
    int bitmap_bottom = -font->size->metrics.descender / 64;
    int glyph_x = 0;
    int glyph_y = 0;
    size_t count = 0;

    Utf8Parser parser;
    uint8_t *ptr = (uint8_t *)str;

    do
    {
        uint32_t codepoint = parser(*ptr);

        for (;; ++count)
        {
            uint32_t charcode;
            if (codepoint & UTF8_PARSER_ERROR_BIT)
            {
                charcode = 0xfffd;
                codepoint &= ~UTF8_PARSER_ERROR_BIT;
            }
            else if (codepoint > 0)
            {
                charcode = codepoint;
                codepoint = 0;
            }
            else
                break;

            if (FT_Load_Char(font, charcode, solid ? (FT_LOAD_DEFAULT | FT_LOAD_BITMAP_METRICS_ONLY | FT_LOAD_TARGET_MONO) : (FT_LOAD_DEFAULT | FT_LOAD_BITMAP_METRICS_ONLY | FT_LOAD_TARGET_NORMAL)))
                continue;

            int glyph_left = glyph_x + font->glyph->bitmap_left;
            int glyph_right = glyph_left + font->glyph->bitmap.width + bold_width + italic_width;
            int glyph_top = glyph_y - font->glyph->bitmap_top;
            int glyph_bottom = glyph_top + font->glyph->bitmap.rows;

            glyph_x += font->glyph->advance.x / 64 + bold_width;
            glyph_y += font->glyph->advance.y / 64;

            int new_bitmap_left = std::min(bitmap_left, std::min(glyph_left, glyph_x));
            int new_bitmap_right = std::max(bitmap_right, std::max(glyph_right, glyph_x));
            int new_bitmap_top = std::min(bitmap_top, std::min(glyph_top, glyph_y));
            int new_bitmap_bottom = std::max(bitmap_bottom, std::max(glyph_bottom, glyph_y));

            if (new_bitmap_right - new_bitmap_left > max_width)
                break;

            bitmap_left = new_bitmap_left;
            bitmap_right = new_bitmap_right;
            bitmap_top = new_bitmap_top;
            bitmap_bottom = new_bitmap_bottom;
        }
    }
    while (*ptr++ != 0);

    return {IntRect(bitmap_left, bitmap_top, bitmap_right - bitmap_left, bitmap_bottom - bitmap_top), count};
}

SDL_Surface *Bitmap::drawTextInner(FT_Face font, const char *str, SDL_Color &c, size_t outline)
{
    const bool solid = p->font->isSolid();
    const bool bold = p->font->getBold();
    const unsigned short bold_width = bold ? GET_BOLD_WIDTH(font) : 0;
    const bool italic = p->font->getItalic();
    const bool needs_transform = italic || outline > 0;
    IntRect bitmapRect = textRect(font, str, solid, bold, italic).first;
    bitmapRect.x -= outline;
    bitmapRect.y -= outline;
    bitmapRect.w += 2 * outline;
    bitmapRect.h += 2 * outline;

    SDL_Surface *txtSurf = new SDL_Surface;
    if ((txtSurf->pixels = STBI_MALLOC(4 * bitmapRect.w * bitmapRect.h)) == nullptr)
        MKXPZ_THROW(std::bad_alloc());
    txtSurf->w = bitmapRect.w;
    txtSurf->h = bitmapRect.h;
    std::memset(txtSurf->pixels, 0, 4 * bitmapRect.w * bitmapRect.h);

    int glyph_x = -bitmapRect.x;
    int glyph_y = -bitmapRect.y;

    FT_Glyph glyph;
    Utf8Parser parser;
    uint8_t *ptr = (uint8_t *)str;

    do
    {
        uint32_t codepoint = parser(*ptr);

        for (;;)
        {
            uint32_t charcode;
            if (codepoint & UTF8_PARSER_ERROR_BIT)
            {
                charcode = 0xfffd;
                codepoint &= ~UTF8_PARSER_ERROR_BIT;
            }
            else if (codepoint > 0)
            {
                charcode = codepoint;
                codepoint = 0;
            }
            else
                break;

            if (needs_transform)
            {
                if (FT_Load_Char(font, charcode, solid ? (FT_LOAD_DEFAULT | FT_LOAD_TARGET_MONO) : (FT_LOAD_DEFAULT | FT_LOAD_TARGET_NORMAL)))
                    continue;
                if (italic)
                    FT_Outline_Transform(&font->glyph->outline, &ITALIC_TRANSFORM);
                if (FT_Get_Glyph(font->glyph, &glyph))
                    continue;
                if (outline > 0)
                {
                    FT_Stroker stroker;
                    if (FT_Stroker_New(shState->fontState().getLibrary(), &stroker))
                    {
                        FT_Done_Glyph(glyph);
                        continue;
                    }
                    FT_Stroker_Set(stroker, 64 * (FT_Fixed)outline, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
                    if (FT_Glyph_Stroke(&glyph, stroker, 1))
                    {
                        FT_Stroker_Done(stroker);
                        FT_Done_Glyph(glyph);
                        continue;
                    }
                    FT_Stroker_Done(stroker);
                }
                if (FT_Glyph_To_Bitmap(&glyph, solid ? FT_RENDER_MODE_MONO : FT_RENDER_MODE_NORMAL, NULL, true))
                {
                    FT_Done_Glyph(glyph);
                    continue;
                }
            }
            else
            {
                if (FT_Load_Char(font, charcode, solid ? (FT_LOAD_DEFAULT | FT_LOAD_RENDER | FT_LOAD_TARGET_MONO) : (FT_LOAD_DEFAULT | FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)))
                    continue;
            }

            int glyph_left = std::max(0, glyph_x + (needs_transform ? ((FT_BitmapGlyph)glyph)->left : font->glyph->bitmap_left));
            int glyph_top = std::max(0, glyph_y - (needs_transform ? ((FT_BitmapGlyph)glyph)->top : font->glyph->bitmap_top));
            FT_Bitmap *bitmap = needs_transform ? &((FT_BitmapGlyph)glyph)->bitmap : &font->glyph->bitmap;
            unsigned int glyph_width = std::min((unsigned int)(bitmapRect.w - glyph_left), bitmap->width);
            unsigned int glyph_height = std::min((unsigned int)(bitmapRect.h - glyph_top), bitmap->rows);

            if (solid)
                for (unsigned int y = 0; y < glyph_height; ++y)
                {
                    for (unsigned int x = 0; x < glyph_width + bold_width; ++x)
                    {
                        for (unsigned int i = x < glyph_width ? 0 : x - glyph_width + 1; i <= bold_width && i <= x; ++i)
                        {
                            if (((uint8_t *)bitmap->buffer)[bitmap->pitch * y + (x - i) / 8] & (1 << (7 - ((x - i) % 8))))
                            {
                                ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x)] = c.r;
                                ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x) + 1] = c.g;
                                ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x) + 2] = c.b;
                                ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x) + 3] = -1;
                                break;
                            }
                        }
                    }
                }
            else
                for (unsigned int y = 0; y < glyph_height; ++y)
                {
                    for (unsigned int x = 0; x < glyph_width + bold_width; ++x)
                    {
                        uint8_t alpha = 0;
                        for (unsigned int i = x < glyph_width ? 0 : x - glyph_width + 1; i <= bold_width && i <= x; ++i)
                        {
                            uint8_t new_alpha = alpha + ((uint8_t *)bitmap->buffer)[bitmap->pitch * y + x - i];
                            if (new_alpha < alpha)
                                new_alpha = -1;
                            alpha = new_alpha;
                        }
                        if (alpha > 0)
                        {
                            ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x)] = c.r;
                            ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x) + 1] = c.g;
                            ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x) + 2] = c.b;
                            ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x) + 3] = alpha;
                        }
                    }
                }

            glyph_x += font->glyph->advance.x / 64 + bold_width;
            glyph_y += font->glyph->advance.y / 64;

            if (needs_transform)
                FT_Done_Glyph(glyph);
        }
    }
    while (*ptr++ != 0);

    return txtSurf;
}
#endif // MKXPZ_RETRO

void Bitmap::drawText(Exception &exception, const IntRect &rect, const char *str, int align)
{
    GUARD(guardDisposed(exception));
    
    GUARD_MEGA();
    GUARD_ANIMATED();
    
    // RGSS doesn't let you draw text backwards
    if (rect.w <= 0 || rect.h <= 0 || rect.x >= width() || rect.y >= height() ||
        rect.w < -rect.x || rect.h < -rect.y)
        return;
    
    if (hasHires()) {
        GUARD(p->selfHires->guardDisposed(exception));
        Font *loresFont;
        GUARD(loresFont = &getFont(exception));
        p->selfHires->setFont(*loresFont);

        int rectX = rect.x * p->selfHires->width() / width();
        int rectY = rect.y * p->selfHires->height() / height();
        int rectWidth = rect.w * p->selfHires->width() / width();
        int rectHeight = rect.h * p->selfHires->height() / height();

        GUARD(p->selfHires->drawText(exception, IntRect(rectX, rectY, rectWidth, rectHeight), str, align));

        return;
    }

    std::string fixed = fixupString(str);
    str = fixed.c_str();
    
    if (*str == '\0')
        return;
    
    if (str[0] == ' ' && str[1] == '\0')
        return;
    
#ifdef MKXPZ_RETRO
    FT_Face sdlFont;
    GUARD(sdlFont = p->font->getSdlFont(exception, 0));
#else
    TTF_Font *sdlFont;
    GUARD(sdlFont = p->font->getSdlFont(exception, 0));
#endif // MKXPZ_RETRO
    const Color &fontColor = p->font->getColor();
    const Color &outColor = p->font->getOutColor();
    
    SDL_Color c = fontColor.toSDLColor();
    
    if (c.a == 0)
        return;
    
    // RGSS crops the the text slightly if there's an outline
    int scaledOutlineSize = 0;
    SDL_Color co;
    if (p->font->getOutline()) {
        // Handle high-res for outline.
        if (p->selfLores) {
            scaledOutlineSize = OUTLINE_SIZE * width() / p->selfLores->width();
        } else {
            scaledOutlineSize = OUTLINE_SIZE;
        }
        
        /* RGSS's outline is drawn by blitting a complete set of text four times, offset diagonally.
         * However, this looks very ugly in hires mode, so instead we'll fake the effect by
         * precomputing the final outline and text colors. */
        co = outColor.toSDLColor();

        if (c.a != 255) {
            Debug() << "BUG: Bitmap drawText with outline and translucent text is broken";
        }

        if (c.a != 255 || co.a != 255) {
            /* Step 1: Compute the outline alpha by layering it onto itself */
            uint8_t out_alpha = ((int)co.a * (int)c.a) / 255;
            
            int co1 = out_alpha * 255;
            int co2 = out_alpha * (255 - out_alpha);
            int fa = co1 + co2;
            co.a = (fa + 1 + (fa >> 8)) >> 8;
            /* Use this instead if we decide we want to round 
             * RGSS seems to not round, but our blit shader seemingly does. */
            //co.a = (fa + 128 + ((fa + 128) >> 8)) >> 8;
            
            if (c.a != 255) {
                /* Step 2: Compute the opacity of the outline that would have been drawn behind the text.
                 * In RGSS, there's a 1 pixel wide region at the edge of the text that only has
                 * 2 layers of outline instead of the 4 layers that's behind most of the text,
                 * which combined with the outlines having less opaque corners from how they're drawn
                 * slightly affects the appearance of the text. We can't replicate this in a way that
                 * looks nice in hires mode, however, this will have to be good enough. */
                uint8_t out_alpha_full = co.a; // compute outline alpha - 4 layers
                for (int i = 0; i < 2; ++i) {
                    int co1 = out_alpha * 255;
                    int co2 = out_alpha_full * (255 - out_alpha);
                    int fa = co1 + co2;
                    out_alpha_full = (fa + 1 + (fa >> 8)) >> 8;
                    /* Use this instead if we decide we want to round 
                     * RGSS seems to not round, but our blit shader seemingly does. */
                    //out_alpha_full = (fa + 128 + ((fa + 128) >> 8)) >> 8;
                }
                
                /* Step 3: Calculate the text color using out_alpha_full in place of co.a. */
                int co1 = c.a * 255;
                int co2 = out_alpha_full * (255 - c.a);
                int fa = co1 + co2;
                
                float faInv = 1.0f / fa;
                float co3 = co1 * faInv;
                float co4 = co2 * faInv;
                // Adding a small number to combat floating point errors.
                c.r = std::min<int>((c.r * co3 + co.r * co4) + 0.001f, 255);
                c.g = std::min<int>((c.g * co3 + co.g * co4) + 0.001f, 255);
                c.b = std::min<int>((c.b * co3 + co.g * co4) + 0.001f, 255);
                
                c.a = (fa + 1 + (fa >> 8)) >> 8;
                /* Use this instead if we decide we want to round 
                 * RGSS seems to not round, but our blit shader seemingly does. */
                //c.a = (fa + 128 + ((fa + 128) >> 8)) >> 8;
            }
        }
    }
    int doubleOutlineSize = scaledOutlineSize * 2;
    
    // Use the output of textSize to determine squeezing, since textSize tends to be used to determine
    // rect dimensions.
    // Also use it to determine position, because freetype sometimes treats the last character as
    // being a pixel wider than it should be, and which textSize is currently set to compensate for.
    int alignmentWidth, alignmentHeight;
    {
        IntRect text_size;
        GUARD(text_size = textSize(exception, str));
        alignmentWidth = text_size.w;
        alignmentHeight = text_size.h;
        
        if (!alignmentWidth)
            return;
    }
    
    // Trim the text to only fill double the rect width
    float squeezeLimit = 0.5f;
#ifdef MKXPZ_RETRO
    size_t charLimit = textRect(sdlFont, str, false, false, false, std::min(width() - rect.x, rect.w) / squeezeLimit).second;
#else
    int charLimitInt;
    if (TTF_MeasureUTF8(sdlFont, str, std::min(width() - rect.x, rect.w) / squeezeLimit, nullptr, &charLimitInt) == 0)
#endif // MKXPZ_RETRO
    {
#ifndef MKXPZ_RETRO
        size_t charLimit = charLimitInt;
#endif // MKXPZ_RETRO
        if (charLimit != fixed.size())
        {
            /* TTF_MeasureUTF8 returns the charLimit in codepoints, not bytes,
             * so we have to calculate where that limit is ourselves.
             * Grabbing a few codepoints past the limit in case the next
             * character is a multi codepoint character.*/
            charLimit += 4;
            for(std::string::iterator it=fixed.begin(); it!=fixed.end() && *it != '\0'; ++it)
            {
                /* The first byte of a multibyte character starts with the first
                 * two bits set to 11, with subsequent bytes starting with 10.
                 * Single byte characters start with 0.*/
                if ((*it & 0xC0) != 0x80)
                {
                    if (charLimit-- == 0)
                    {
                        *it = '\0';
                        break;
                    }
                }
            }
        }
    }
    
    SDL_Surface *txtSurf;
    
#ifdef MKXPZ_RETRO
    txtSurf = drawTextInner(sdlFont, str, c, 0);
#else
    if (p->font->isSolid())
        txtSurf = TTF_RenderUTF8_Solid(sdlFont, str, c);
    else
        txtSurf = TTF_RenderUTF8_Blended(sdlFont, str, c);
    
    if (!txtSurf)
        throw Exception(Exception::SDLError, "Error creating text: %s",
                        SDL_GetError());
    
    p->ensureFormat(txtSurf, SDL_PIXELFORMAT_ABGR8888);
#endif // MKXPZ_RETRO
    
    if (p->font->getShadow())
    {
        int scaledShadowSize = 1;
        if (p->selfLores) {
            scaledShadowSize = scaledShadowSize * width() / p->selfLores->width();
        }

#ifdef MKXPZ_RETRO
        applyShadow(txtSurf, c, scaledShadowSize);
#else
        applyShadow(txtSurf, *p->format, c, scaledShadowSize);
#endif // MKXPZ_RETRO
    }
    
    int alignX = rect.x;
    
    switch (align)
    {
        default:
        case Left :
            break;
            
        case Center :
            // Yes, half of the outline size.
            alignX += (rect.w - (alignmentWidth + scaledOutlineSize)) / 2;
            break;
            
        case Right :
            // I don't know why it's double the outline size, but it is.
            alignX += rect.w - alignmentWidth - doubleOutlineSize;
            break;
    }
    
    if (alignX < rect.x)
        alignX = rect.x;
    
    int alignY = rect.y + ((rect.h - alignmentHeight) / 2) - scaledOutlineSize;
    
    alignY = std::max(alignY, rect.y);
    
    /* FIXME: RGSS begins squeezing the text before it fills the rect.
     * While this is extremely undesirable, a number of games will understandably
     * have made the rects bigger to compensate, so we should probably match it */
    float squeeze = (float) rect.w / alignmentWidth;
    
    squeeze = clamp(squeeze, squeezeLimit, 1.0f);

    if (scaledOutlineSize)
    {
        SDL_Surface *outline;
#ifdef MKXPZ_RETRO
        FT_Face sdlOutline;
#else
        TTF_Font *sdlOutline;
#endif // MKXPZ_RETRO
        sdlOutline = p->font->getSdlFont(exception, scaledOutlineSize);
        if (exception.is_error()) {
#ifdef MKXPZ_RETRO
            stbi_image_free(txtSurf->pixels);
            delete txtSurf;
#else
            SDL_FreeSurface(txtSurf);
#endif // MKXPZ_RETRO
            return;
        }
#ifdef MKXPZ_RETRO
        outline = drawTextInner(sdlOutline, str, co, scaledOutlineSize);
#else
        if (p->font->isSolid())
            outline = TTF_RenderUTF8_Solid(sdlOutline, str, co);
        else
            outline = TTF_RenderUTF8_Blended(sdlOutline, str, co);
        
        if (!outline) {
            SDL_FreeSurface(txtSurf);
            throw Exception(Exception::SDLError, "Error creating text outline: %s",
                            SDL_GetError());
        }
        
        p->ensureFormat(outline, SDL_PIXELFORMAT_ABGR8888);
#endif // MKXPZ_RETRO

        // Enterbrain's runtime crops the top row and left column of the text
        // when blitting it onto the outline. We allow the user to optionally
        // disable this cropping, since it's arguably quite ugly.
        int outlineCropUndo = shState->config().fontOutlineCrop ? 0 : scaledOutlineSize;

        /* outline should always be at least doubleOutlineSize bigger than txtSurf,
         * but we may as well validate it here anyway. */
        SDL_Rect inRect = {scaledOutlineSize - outlineCropUndo, scaledOutlineSize - outlineCropUndo,
                           std::min<int>({(int)(rect.w / squeeze) - doubleOutlineSize,
                                          txtSurf->w - scaledOutlineSize,
                                          outline->w - doubleOutlineSize
                                         }) + outlineCropUndo,
                           std::min<int>({rect.h - doubleOutlineSize,
                                          txtSurf->h - scaledOutlineSize,
                                          outline->h - doubleOutlineSize
                                         }) + outlineCropUndo};
        SDL_Rect outRect = {doubleOutlineSize - outlineCropUndo, doubleOutlineSize - outlineCropUndo, 0, 0};
        
        blendText(txtSurf, inRect, c, outline, outRect, co, p->font->getShadow());
#ifdef MKXPZ_RETRO
        stbi_image_free(txtSurf->pixels);
        delete txtSurf;
#else
        SDL_FreeSurface(txtSurf);
#endif // MKXPZ_RETRO
        txtSurf = outline;
    }
    
    IntRect destRect(alignX, alignY,
                    std::min(rect.w, (int)(txtSurf->w * squeeze)),
                    std::min(rect.h, txtSurf->h));
    
    destRect.w = std::min(destRect.w, width() - destRect.x);
    destRect.h = std::min(destRect.h, height() - destRect.y);
    
    IntRect sourceRect(scaledOutlineSize, scaledOutlineSize, destRect.w / squeeze, destRect.h);
    
    Bitmap txtBitmap(exception, txtSurf, nullptr, true, false);
    if (exception.is_error()) {
        return;
    }
    bool smooth = squeeze != 1.0f;
    GUARD(stretchBlt(exception, destRect, txtBitmap, sourceRect, 255, smooth));
}

#ifndef MKXPZ_RETRO
/* http://www.lemoda.net/c/utf8-to-ucs2/index.html */
static uint16_t utf8_to_ucs2(const char *_input,
                             const char **end_ptr)
{
    const unsigned char *input =
    reinterpret_cast<const unsigned char*>(_input);
    *end_ptr = _input;
    
    if (input[0] == 0)
        return -1;
    
    if (input[0] < 0x80)
    {
        *end_ptr = _input + 1;
        
        return input[0];
    }
    
    if ((input[0] & 0xE0) == 0xE0)
    {
        if (input[1] == 0 || input[2] == 0)
            return -1;
        
        *end_ptr = _input + 3;
        
        return (input[0] & 0x0F)<<12 |
        (input[1] & 0x3F)<<6  |
        (input[2] & 0x3F);
    }
    
    if ((input[0] & 0xC0) == 0xC0)
    {
        if (input[1] == 0)
            return -1;
        
        *end_ptr = _input + 2;
        
        return (input[0] & 0x1F)<<6  |
        (input[1] & 0x3F);
    }
    
    return -1;
}
#endif // MKXPZ_RETRO

IntRect Bitmap::textSize(Exception &exception, const char *str)
{
    GUARD_V(IntRect(), guardDisposed(exception));
    
    GUARD_MEGA(IntRect());
    GUARD_ANIMATED(IntRect());
    
    // TODO: High-res Bitmap textSize not implemented, but I think it's the same as low-res?
    // Need to double-check this.

    const bool italic = p->font->getItalic();

#ifdef MKXPZ_RETRO
    const bool bold = p->font->getBold();
    FT_Face font;
    GUARD_V(IntRect(), font = p->font->getSdlFont(exception, 0));
    const IntRect rect = textRect(font, fixupString(str).c_str(), p->font->isSolid(), bold, italic).first;
    return IntRect(0, 0, rect.w, rect.h);
#else
    TTF_Font *sdlFont;
    GUARD_V(IntRect(), sdlFont = p->font->getSdlFont(exception, 0));
    
    // freetype sometimes treats the last character of the string as being
    // a pixel wider than it should be. Adding a space at the end and then
    // removing it's width should make character-by-character text
    // more accurate.
    std::string fixed = fixupString(str) + " ";
    
    int w, h;
    TTF_SizeUTF8(sdlFont, fixed.c_str(), &w, &h);
    
    int ws;
    TTF_SizeUTF8(sdlFont, " ", &ws, 0);
    w -= ws;
    
    /* If str is one character long, *endPtr == 0 */
    const char *endPtr;
    uint16_t ucs2 = utf8_to_ucs2(str, &endPtr);
    
    /* For cursive characters, returning the advance
     * as width yields better results */
    if (italic && *endPtr == '\0')
        TTF_GlyphMetrics(sdlFont, ucs2, 0, 0, 0, 0, &w);

    if (shState->config().fontHeightReporting == 0) {
        if(!w) {
            h = 0;
        } else {
            /* RGSS normalizes the reported heights.
             * Note that this may result in the bottoms
             * of some characters being cut off. */
             h = TTF_FontHeight(sdlFont);
        }
    }
    
    return IntRect(0, 0, w, h);
#endif // MKXPZ_RETRO
}

DEF_ATTR_RD_SIMPLE(Bitmap, Font, Font&, *p->font)

void Bitmap::setFont(Font &value)
{
    *p->font = value;
}

void Bitmap::setInitFont(Font *value)
{
    if (value != &shState->defaultFont()) {
        if (p->selfLores)
            value->setHiresMult((float)width() / (float)p->selfLores->width());
        else
            value->setHiresMult(1.0f);
    }

    p->font = value;
}

TEXFBO &Bitmap::getGLTypes() const
{
    return p->getGLTypes();
}

SDL_Surface *Bitmap::surface() const
{
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap surface not implemented";
    }

    return p->surface;
}

SDL_Surface *Bitmap::megaSurface() const
{
    if (hasHires()) {
        if (p->megaSurface) {
            Debug() << "BUG: High-res Bitmap megaSurface not implemented (low-res has megaSurface)";
        }
        if (p->selfHires->megaSurface()) {
            Debug() << "BUG: High-res Bitmap megaSurface not implemented (high-res has megaSurface)";
        }
    }

    return p->megaSurface;
}

void Bitmap::ensureNonMega(Exception &exception) const
{
    if (isDisposed())
        return;
    
    GUARD_MEGA();
}

void Bitmap::ensureNonAnimated(Exception &exception) const
{
    if (isDisposed())
        return;
    
    GUARD_ANIMATED();
}

void Bitmap::ensureAnimated(Exception &exception) const
{
    if (isDisposed())
        return;
    
    GUARD_UNANIMATED();
}

void Bitmap::stop(Exception &exception)
{
    GUARD(guardDisposed(exception));
    
    GUARD_UNANIMATED();
    if (!p->animation.playing) return;
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap stop not implemented";
    }

    p->animation.stop();
}

void Bitmap::play(Exception &exception)
{
    GUARD(guardDisposed(exception));
    
    GUARD_UNANIMATED();
    if (p->animation.playing) return;

    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap play not implemented";
    }

    p->animation.play();
}

bool Bitmap::isPlaying(Exception &exception) const
{
    GUARD_V(false, guardDisposed(exception));
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap isPlaying not implemented";
    }

    if (!p->animation.playing)
        return false;
    
    if (p->animation.loop)
        return true;
    
    return p->animation.currentFrameIRaw() < p->animation.frames.size();
}

bool Bitmap::getPlaying(Exception &exception) const
{
    bool ret;
    GUARD_V(false, ret = isPlaying(exception));
    return ret;
}

void Bitmap::setPlaying(Exception &exception, bool playing)
{
    if (playing)
        GUARD(play(exception));
    else
        GUARD(stop(exception));
}

void Bitmap::gotoAndStop(Exception &exception, int frame)
{
    GUARD(guardDisposed(exception));
    
    GUARD_UNANIMATED();
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap gotoAndStop not implemented";
    }

    p->animation.stop();
    p->animation.seek(frame);
}
void Bitmap::gotoAndPlay(Exception &exception, int frame)
{
    GUARD(guardDisposed(exception));
    
    GUARD_UNANIMATED();
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap gotoAndPlay not implemented";
    }

    p->animation.stop();
    p->animation.seek(frame);
    p->animation.play();
}

int Bitmap::numFrames(Exception &exception) const
{
    GUARD_V(0, guardDisposed(exception));
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap numFrames not implemented";
    }

    if (!p->animation.enabled) return 1;
    return (int)p->animation.frames.size();
}

int Bitmap::currentFrameI(Exception &exception) const
{
    GUARD_V(0, guardDisposed(exception));
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap currentFrameI not implemented";
    }

    if (!p->animation.enabled) return 0;
    return p->animation.currentFrameI();
}

int Bitmap::addFrame(Exception &exception, Bitmap &source, int position)
{
    GUARD_V(0, guardDisposed(exception));
    GUARD_V(0, source.guardDisposed(exception));
    
    GUARD_MEGA(0);
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap addFrame dest not implemented";
    }

    if (source.hasHires()) {
        Debug() << "BUG: High-res Bitmap addFrame source not implemented";
    }

    if (source.height() != height() || source.width() != width()) {
        exception = Exception(Exception::MKXPError, "Animations with varying dimensions are not supported (%ix%i vs %ix%i)",
                              source.width(), source.height(), width(), height());
        return 0;
    }
    
    TEXFBO newframe;
    GUARD_V(0, newframe = shState->texPool().request(exception, source.width(), source.height()));
    
    // Convert the bitmap into an animated bitmap if it isn't already one
    if (!p->animation.enabled) {
        p->animation.width = p->gl.width;
        p->animation.height = p->gl.height;
        p->animation.enabled = true;
        p->animation.lastFrame = 0;
        p->animation.playTime = 0;
        p->animation.startTime = 0;
        
        if (p->animation.fps <= 0)
            p->animation.fps = shState->graphics().getFrameRate();
        
#ifdef MKXPZ_RETRO
        p->animation.frames.push_back({p->gl, p->diff, p->path, p->originalFrameIndex});
#else
        p->animation.frames.push_back({p->gl});
#endif // MKXPZ_RETRO
        
        if (p->surface)
#ifdef MKXPZ_RETRO
        {
            stbi_image_free(p->surface->pixels);
            delete p->surface;
        }
#else
            SDL_FreeSurface(p->surface);
#endif // MKXPZ_RETRO
        p->gl = TEXFBO();
    }
    
    if (source.surface()) {
        TEX::bind(newframe.tex);
        TEX::uploadImage(source.width(), source.height(), source.surface()->pixels, GL_RGBA);
#ifdef MKXPZ_RETRO
        stbi_image_free(p->surface->pixels);
        delete p->surface;
#else
        SDL_FreeSurface(p->surface);
#endif // MKXPZ_RETRO
        p->surface = 0;
    }
    else {
        GLMeta::blitBegin(newframe, false, SameScale);
        GLMeta::blitSource(source.getGLTypes(), SameScale);
        GLMeta::blitRectangle(rect(), rect());
        GLMeta::blitEnd();
    }
    
    int ret;
    
    if (position < 0) {
#ifdef MKXPZ_RETRO
        p->animation.frames.push_back({newframe, source.isAnimated() ? source.p->animation.currentFrame().diff : source.p->diff, source.isAnimated() ? source.p->animation.currentFrame().path : source.p->path, source.isAnimated() ? source.p->animation.currentFrame().originalFrameIndex : source.p->originalFrameIndex});
#else
        p->animation.frames.push_back({newframe});
#endif // MKXPZ_RETRO
        ret = (int)p->animation.frames.size();
    }
    else {
#ifdef MKXPZ_RETRO
        p->animation.frames.insert(p->animation.frames.begin() + clamp(position, 0, (int)p->animation.frames.size()), {newframe, source.isAnimated() ? source.p->animation.currentFrame().diff : source.p->diff, source.isAnimated() ? source.p->animation.currentFrame().path : source.p->path, source.isAnimated() ? source.p->animation.currentFrame().originalFrameIndex : source.p->originalFrameIndex});
#else
        p->animation.frames.insert(p->animation.frames.begin() + clamp(position, 0, (int)p->animation.frames.size()), {newframe});
#endif // MKXPZ_RETRO
        ret = position;
    }
    
    return ret;
}

void Bitmap::removeFrame(Exception &exception, int position) {
    GUARD(guardDisposed(exception));
    
    GUARD_UNANIMATED();
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap removeFrame not implemented";
    }

    int pos = (position < 0) ? (int)p->animation.frames.size() - 1 : clamp(position, 0, (int)(p->animation.frames.size() - 1));
    shState->texPool().release(p->animation.frames[pos].gl);
    p->animation.frames.erase(p->animation.frames.begin() + pos);
    
    // Change the animated bitmap back to a normal one if there's only one frame left
    if (p->animation.frames.size() == 1) {
        
        p->animation.enabled = false;
        p->animation.playing = false;
        p->animation.width = 0;
        p->animation.height = 0;
        p->animation.lastFrame = 0;
        
        p->gl = p->animation.frames[0].gl;
#ifdef MKXPZ_RETRO
        p->diff = p->animation.frames[0].diff;
        p->path = p->animation.frames[0].path;
        p->originalFrameIndex = p->animation.frames[0].originalFrameIndex;
#endif // MKXPZ_RETRO
        p->animation.frames.erase(p->animation.frames.begin());
        
        FBO::bind(p->gl.fbo);
        taintArea(rect());
    }
}

void Bitmap::nextFrame(Exception &exception)
{
    GUARD(guardDisposed(exception));
    
    GUARD_UNANIMATED();
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap nextFrame not implemented";
    }

    GUARD(stop(exception));
    if ((uint32_t)p->animation.lastFrame >= p->animation.frames.size() - 1)  {
        if (!p->animation.loop) return;
        p->animation.lastFrame = 0;
        return;
    }
    
    p->animation.lastFrame++;
}

void Bitmap::previousFrame(Exception &exception)
{
    GUARD(guardDisposed(exception));
    
    GUARD_UNANIMATED();
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap previousFrame not implemented";
    }

    GUARD(stop(exception));
    if (p->animation.lastFrame <= 0) {
        if (!p->animation.loop) {
            p->animation.lastFrame = 0;
            return;
        }
        p->animation.lastFrame = (int)p->animation.frames.size() - 1;
        return;
    }
    
    p->animation.lastFrame--;
}

void Bitmap::setAnimationFPS(Exception &exception, float FPS)
{
    GUARD(guardDisposed(exception));
    
    GUARD_MEGA();
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap setAnimationFPS not implemented";
    }

    bool restart = p->animation.playing;
    p->animation.stop();
    p->animation.fps = (FPS < 0) ? 0 : FPS;
    if (restart) p->animation.play();
}

std::vector<BitmapFrame> &Bitmap::getFrames() const
{
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap getFrames not implemented";
    }

    return p->animation.frames;
}

float Bitmap::animationFPS() const
{
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap getAnimationFPS not implemented";
    }

    return p->animation.fps;
}

float Bitmap::getAnimationFPS(Exception &exception) const
{
    GUARD_V(0.0f, guardDisposed(exception));
    
    GUARD_MEGA(0.0f);
    
    return animationFPS();
}

void Bitmap::setLooping(Exception &exception, bool loop)
{
    GUARD(guardDisposed(exception));
    
    GUARD_MEGA();
    
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap setLooping not implemented";
    }

    p->animation.loop = loop;
}

bool Bitmap::looping() const
{
    if (hasHires()) {
        Debug() << "BUG: High-res Bitmap getLooping not implemented";
    }

    return p->animation.loop;
}

bool Bitmap::getLooping(Exception &exception) const
{
    GUARD_V(false, guardDisposed(exception));
    
    GUARD_MEGA(false);
    
    return looping();
}

void Bitmap::bindTex(ShaderBase &shader, bool substituteLoresSize)
{
    // Hires mode is handled by p->bindTexture.

    p->bindTexture(shader, substituteLoresSize);
}

void Bitmap::taintArea(const IntRect &rect)
{
    if (hasHires()) {
        int destX, destY, destWidth, destHeight;
        destX = rect.x * p->selfHires->width() / width();
        destY = rect.y * p->selfHires->height() / height();
        destWidth = rect.w * p->selfHires->width() / width();
        destHeight = rect.h * p->selfHires->height() / height();

        p->selfHires->taintArea(IntRect(destX, destY, destWidth, destHeight));
    }

    p->addTaintedArea(rect);
}

int Bitmap::maxSize(){
    return glState.caps.maxTexSize;
}

void Bitmap::assumeRubyGC(bool value)
{
    p->assumingRubyGC = value;
}

void Bitmap::releaseResources()
{
    // p can be null if there was an error creating this bitmap
    if (p == nullptr)
        return;

    if (p->selfHires && !p->assumingRubyGC) {
        delete p->selfHires;
    }

    if (p->megaSurface)
#ifdef MKXPZ_RETRO
    {
        stbi_image_free(p->megaSurface->pixels);
        delete p->megaSurface;
    }
#else
        SDL_FreeSurface(p->megaSurface);
#endif // MKXPZ_RETRO
    else if (p->animation.enabled) {
        p->animation.enabled = false;
        p->animation.playing = false;
        for (BitmapFrame &frame : p->animation.frames)
            shState->texPool().release(frame.gl);
    }
    else
        shState->texPool().release(p->gl);
    
    delete p;
}

void Bitmap::loresDisposal()
{
    loresDispCon.disconnect();
    dispose();
}

#ifdef MKXPZ_RETRO
bool Bitmap::sandbox_serialize_without_hires(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
    if (!mkxp_sandbox::sandbox_serialize((int32_t)width(), data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)height(), data, max_size)) return false;

    if (!mkxp_sandbox::sandbox_serialize(p->animation.enabled, data, max_size)) return false;

    if (p->animation.enabled) {
        if (!mkxp_sandbox::sandbox_serialize((mkxp_sandbox::wasm_size_t)p->animation.frames.size(), data, max_size)) return false;
        for (const BitmapFrame &frame : p->animation.frames) {
            if (!mkxp_sandbox::sandbox_serialize(frame.path, data, max_size)) return false;
            if (!mkxp_sandbox::sandbox_serialize((mkxp_sandbox::wasm_size_t)frame.originalFrameIndex, data, max_size)) return false;
            if (!sandbox_serialize_pixels(data, max_size, frame.diff)) return false;
        }
    } else {
        if (!mkxp_sandbox::sandbox_serialize(p->path, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize((mkxp_sandbox::wasm_size_t)p->originalFrameIndex, data, max_size)) return false;
        if (!sandbox_serialize_pixels(data, max_size, p->diff)) return false;
    }

    if (p->animation.enabled) {
        if (!mkxp_sandbox::sandbox_serialize(p->animation.playing, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(p->animation.fps, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(p->animation.loop, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize((int32_t)p->animation.lastFrame, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(p->animation.startTime, data, max_size)) return false;
    }

    if (!mkxp_sandbox::sandbox_serialize(p->font == &shState->defaultFont() ? nullptr : p->font, data, max_size)) return false;

    return true;
}

bool Bitmap::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
    if (!sandbox_serialize_without_hires(data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->selfHires, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->selfLores, data, max_size)) return false;
    return true;
}

bool Bitmap::sandbox_serialize_pixels(void *&data, mkxp_sandbox::wasm_size_t &max_size, const std::vector<std::vector<uint32_t>> &diff) const
{
    if (!mkxp_sandbox::sandbox_serialize((mkxp_sandbox::wasm_size_t)diff.size(), data, max_size)) return false;
    mkxp_sandbox::wasm_size_t num_empty_tiles = 0;
    mkxp_sandbox::wasm_size_t tile_number = 0;
    for (const std::vector<uint32_t> &tile : diff) {
        if (tile.empty()) {
            ++num_empty_tiles;
        } else {
            if (num_empty_tiles > 0) {
                if (!mkxp_sandbox::sandbox_serialize(false, data, max_size)) return false;
                if (!mkxp_sandbox::sandbox_serialize(num_empty_tiles, data, max_size)) return false;
                num_empty_tiles = 0;
            }
            if (!mkxp_sandbox::sandbox_serialize(true, data, max_size)) return false;
            size_t tile_col = tile_number % CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_row = tile_number / CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_width = std::min(DIFF_TILE_SIZE, width() - DIFF_TILE_SIZE * tile_col);
            size_t tile_height = std::min(DIFF_TILE_SIZE, height() - DIFF_TILE_SIZE * tile_row);
            MKXPZ_FORCED_ASSERT(tile.size() == tile_width * tile_height);
            if (max_size < 4 * tile_width * tile_height) return false;
            std::memcpy(data, tile.data(), 4 * tile_width * tile_height);
            data = (uint8_t *)data + 4 * tile_width * tile_height;
            max_size -= 4 * tile_width * tile_height;
        }
        ++tile_number;
    }
    if (num_empty_tiles > 0) {
        if (!mkxp_sandbox::sandbox_serialize(false, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(num_empty_tiles, data, max_size)) return false;
        num_empty_tiles = 0;
    }

    return true;
}

bool Bitmap::sandbox_deserialize_without_hires(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
    int32_t old_width = width();
    int32_t old_height = height();
    int32_t new_width;
    int32_t new_height;
    if (!mkxp_sandbox::sandbox_deserialize(new_width, data, max_size)) return false;
    if (new_width != old_width) {
        deserModified = true;
        deserSizeChanged = true;
    }
    if (!mkxp_sandbox::sandbox_deserialize(new_height, data, max_size)) return false;
    if (new_height != old_height) {
        deserModified = true;
        deserSizeChanged = true;
    }

    bool old_animation_enabled = p->animation.enabled;
    if (!mkxp_sandbox::sandbox_deserialize(p->animation.enabled, data, max_size)) return false;
    if (old_animation_enabled != p->animation.enabled) {
        deserModified = true;
    }

    if (p->animation.enabled) {
        mkxp_sandbox::wasm_size_t num_frames;
        if (!mkxp_sandbox::sandbox_deserialize(num_frames, data, max_size)) return false;

        // Check if any animation frames have had their paths or frame indices changed, or need to be reloaded based on the diffs in the save state, and if so, reload the bitmap
        bool need_reload = num_frames != p->animation.frames.size();
        if (!need_reload) {
            const void *tmp_data = data;
            mkxp_sandbox::wasm_size_t tmp_max_size = max_size;
            for (const BitmapFrame &frame : p->animation.frames) {
                std::string path;
                if (!mkxp_sandbox::sandbox_deserialize(path, tmp_data, tmp_max_size)) return false;
                if (path != frame.path) {
                    need_reload = true;
                    break;
                }
                mkxp_sandbox::wasm_size_t index;
                if (!mkxp_sandbox::sandbox_deserialize(index, tmp_data, tmp_max_size)) return false;
                if (index != (mkxp_sandbox::wasm_size_t)frame.originalFrameIndex) {
                    need_reload = true;
                    break;
                }
                bool diff_need_reload;
                bool diff_need_reload_if_path_not_empty;
                if (!sandbox_deserialize_pixels_check_need_reload(tmp_data, max_size, frame.diff, diff_need_reload, diff_need_reload_if_path_not_empty, true)) return false;
                if (diff_need_reload || (diff_need_reload_if_path_not_empty && !path.empty())) {
                    need_reload = true;
                    break;
                }
            }
        }

        // Reload the bitmap if needed
        if (need_reload) {
            delete p;
            {
                Exception e;
                initFromDimensions(e, new_width, new_height, true);
                if (e.is_error() || isMega()) {
                    return false;
                }
                p->animation.enabled = true;
                p->animation.playing = false;
                p->animation.width = new_width;
                p->animation.height = new_height;
                p->animation.lastFrame = 0;
                p->diff.clear();
                for (BitmapFrame &frame : p->animation.frames) {
                    shState->texPool().release(frame.gl);
                }
                p->animation.frames.clear();
            }
            deserModified = true;

            std::unordered_map<std::string, Bitmap *> sources;

            for (mkxp_sandbox::wasm_size_t i = 0; i < num_frames; ++i) {
                std::string path;
                if (!mkxp_sandbox::sandbox_deserialize(path, data, max_size)) return false;

                Bitmap *source;
                {
                    const auto it = sources.find(path);
                    if (it == sources.end()) {
                        Exception e;
                        source = path.empty() ? new Bitmap(e, new_width, new_height, true, false) : new Bitmap(e, path.c_str(), false);
                        if (e.is_error()) {
                            delete source;
                            return false;
                        }
                        sources.insert({path, source});
                    } else {
                        source = it->second;
                    }
                }

                mkxp_sandbox::wasm_size_t index;
                if (!mkxp_sandbox::sandbox_deserialize(index, data, max_size)) return false;

                TEXFBO *src_texfbo;
                if (source->isAnimated()) {
                    if (index >= source->p->animation.frames.size()) {
                        delete source;
                        return false;
                    }
                    src_texfbo = &source->p->animation.frames[index].gl;
                } else {
                    if (index != 0) {
                        delete source;
                        return false;
                    }
                    src_texfbo = &source->p->gl;
                }

                TEXFBO new_texfbo = *src_texfbo;
                TEXFBO::clear(*src_texfbo);

                p->animation.frames.push_back({new_texfbo, std::vector<std::vector<uint32_t>>(CEIL_DIV_DIFF_TILE_SIZE(p->animation.width) * CEIL_DIV_DIFF_TILE_SIZE(p->animation.height)), path, (int)index});

                delete source;
            }
        } else {
            for (mkxp_sandbox::wasm_size_t i = 0; i < num_frames; ++i) {
                std::string path;
                if (!mkxp_sandbox::sandbox_deserialize(path, data, max_size)) return false;
                mkxp_sandbox::wasm_size_t index;
                if (!mkxp_sandbox::sandbox_deserialize(index, data, max_size)) return false;
            }
        }

        for (BitmapFrame &frame : p->animation.frames) {
            if (!sandbox_deserialize_pixels(data, max_size, frame.diff)) return false;
        }
    } else {
        std::string old_path = p->path;
        if (!mkxp_sandbox::sandbox_deserialize(p->path, data, max_size)) return false;
        mkxp_sandbox::wasm_size_t old_index = p->originalFrameIndex;
        {
            mkxp_sandbox::wasm_size_t index;
            if (!mkxp_sandbox::sandbox_deserialize(index, data, max_size)) return false;
            p->originalFrameIndex = index;
        }
        bool need_reload;
        bool need_reload_if_path_not_empty;
        if (!sandbox_deserialize_pixels_check_need_reload(data, max_size, p->diff, need_reload, need_reload_if_path_not_empty, false)) return false;

        // Reload bitmap if its path has changed, or its size has changed, or if it needs to be reloaded based on the diff in the save state
        if (deserSizeChanged || need_reload || (need_reload_if_path_not_empty && !p->path.empty()) || (mkxp_sandbox::wasm_size_t)p->originalFrameIndex != old_index || p->path != old_path) {
            if (p->path.empty()) {
                delete p;
                {
                    Exception e;
                    initFromDimensions(e, new_width, new_height, true);
                    if (e.is_error()) {
                        return false;
                    }
                }
            } else {
                bool new_animation_enabled = p->animation.enabled;
                std::string path(p->path);
                delete p;
                {
                    Exception e;
                    initFromFilename(e, path.c_str());
                    if (e.is_error() || p->path.empty() || (!p->animation.enabled && new_animation_enabled) || width() != new_width || height() != new_height) {
                        return false;
                    }
                }

                // If the newly reloaded bitmap is animated but the save state has a non-animated bitmap,
                // turn it into a non-animated one
                if (p->animation.enabled && !new_animation_enabled) {
                    if (p->originalFrameIndex < 0 || (size_t)p->originalFrameIndex >= p->animation.frames.size()) {
                        return false;
                    }
                    p->animation.enabled = false;
                    p->animation.playing = false;
                    p->animation.width = 0;
                    p->animation.height = 0;
                    p->animation.lastFrame = 0;
                    p->gl = p->animation.frames[p->originalFrameIndex].gl;
                    p->diff = p->animation.frames[p->originalFrameIndex].diff;
                    p->path = p->animation.frames[p->originalFrameIndex].path;
                    p->originalFrameIndex = p->animation.frames[p->originalFrameIndex].originalFrameIndex;
                    for (BitmapFrame &frame : p->animation.frames) {
                        shState->texPool().release(frame.gl);
                    }
                    p->animation.frames.clear();
                }
            }
            deserModified = true;
        }

        if (!sandbox_deserialize_pixels(data, max_size, p->diff)) return false;
    }

    if (p->animation.enabled) {
        p->animation.width = p->gl.width;
        p->animation.height = p->gl.height;
        {
            bool old_playing = p->animation.playing;
            bool new_playing;
            if (!mkxp_sandbox::sandbox_deserialize(new_playing, data, max_size)) return false;
            if (new_playing != old_playing) {
                if (new_playing) {
                    p->animation.play();
                } else {
                    p->animation.stop();
                }
            }
        }
        {
            float value = p->animation.fps;
            if (!mkxp_sandbox::sandbox_deserialize(p->animation.fps, data, max_size)) return false;
            if (p->animation.fps < 0) {
                p->animation.fps = 0;
            }
            if (p->animation.fps != value) {
                bool restart = p->animation.playing;
                p->animation.stop();
                if (restart) {
                    p->animation.play();
                }
            }
        }
        if (!mkxp_sandbox::sandbox_deserialize(p->animation.loop, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->animation.lastFrame, data, max_size)) return false;
        p->animation.lastFrame = clamp(p->animation.lastFrame, 0, (int)p->animation.frames.size());
        if (!mkxp_sandbox::sandbox_deserialize(p->animation.startTime, data, max_size)) return false;
    }

    if (!mkxp_sandbox::sandbox_deserialize(p->font, data, max_size)) return false;
    if (p->font == nullptr) {
        p->font = &shState->defaultFont();
    }

    return true;
}

bool Bitmap::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
    if (!sandbox_deserialize_without_hires(data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->selfHires, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->selfLores, data, max_size)) return false;
    return true;
}

bool Bitmap::sandbox_deserialize_pixels_check_need_reload(const void *&data, mkxp_sandbox::wasm_size_t &max_size, const std::vector<std::vector<uint32_t>> &diff, bool &need_reload, bool &need_reload_if_path_not_empty, bool modify_data_and_max_size) const
{
    need_reload = false;
    need_reload_if_path_not_empty = false;

    const void *tmp_data = data;
    mkxp_sandbox::wasm_size_t tmp_max_size = max_size;

    mkxp_sandbox::wasm_size_t num_tiles;
    if (!mkxp_sandbox::sandbox_deserialize(num_tiles, tmp_data, tmp_max_size)) return false;
    if (num_tiles != diff.size()) {
        need_reload = true;
        need_reload_if_path_not_empty = true;
        if (!modify_data_and_max_size) {
            return true;
        }
    }

    mkxp_sandbox::wasm_size_t tile_number = 0;
    while (num_tiles > 0) {
        bool is_not_empty;
        if (!mkxp_sandbox::sandbox_deserialize(is_not_empty, tmp_data, tmp_max_size)) return false;

        if (!is_not_empty) {
            mkxp_sandbox::wasm_size_t num_empty_tiles;
            if (!mkxp_sandbox::sandbox_deserialize(num_empty_tiles, tmp_data, tmp_max_size)) return false;

            while (num_empty_tiles > 0) {
                // Check for tiles that are empty in the save state but not currently empty
                if (!need_reload && !diff[tile_number].empty()) {
                    need_reload_if_path_not_empty = true;
                    if (!modify_data_and_max_size) {
                        return true;
                    }
                }

                ++tile_number;
                --num_tiles;
                --num_empty_tiles;
            }
        } else {
            size_t tile_col = tile_number % CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_row = tile_number / CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_width = std::min(DIFF_TILE_SIZE, width() - DIFF_TILE_SIZE * tile_col);
            size_t tile_height = std::min(DIFF_TILE_SIZE, height() - DIFF_TILE_SIZE * tile_row);

            if (tmp_max_size < 4 * tile_width * tile_height) return false;
            tmp_data = (uint8_t *)tmp_data + 4 * tile_width * tile_height;
            tmp_max_size -= 4 * tile_width * tile_height;
            ++tile_number;
            --num_tiles;
        }
    }

    if (modify_data_and_max_size) {
        data = tmp_data;
        max_size = tmp_max_size;
    }
    return true;
}

bool Bitmap::sandbox_deserialize_pixels(const void *&data, mkxp_sandbox::wasm_size_t &max_size, std::vector<std::vector<uint32_t>> &diff, mkxp_sandbox::wasm_size_t frame_number)
{
    mkxp_sandbox::wasm_size_t num_tiles;
    if (!mkxp_sandbox::sandbox_deserialize(num_tiles, data, max_size)) return false;
    if (num_tiles != diff.size()) {
        return false;
    }

    mkxp_sandbox::wasm_size_t tile_number = 0;
    while (num_tiles > 0) {
        bool is_not_empty;
        if (!mkxp_sandbox::sandbox_deserialize(is_not_empty, data, max_size)) return false;

        if (!is_not_empty) {
            mkxp_sandbox::wasm_size_t num_empty_tiles;
            if (!mkxp_sandbox::sandbox_deserialize(num_empty_tiles, data, max_size)) return false;

            while (num_empty_tiles > 0) {
                std::vector<uint32_t> &tile = diff[tile_number];

                // Clear tiles that are empty in the save state but not currently empty
                if (!tile.empty()) {
                    tile.clear();

                    size_t tile_col = tile_number % CEIL_DIV_DIFF_TILE_SIZE(width());
                    size_t tile_row = tile_number / CEIL_DIV_DIFF_TILE_SIZE(width());
                    size_t tile_width = std::min(DIFF_TILE_SIZE, width() - DIFF_TILE_SIZE * tile_col);
                    size_t tile_height = std::min(DIFF_TILE_SIZE, height() - DIFF_TILE_SIZE * tile_row);
                    IntRect rect = IntRect(DIFF_TILE_SIZE * tile_col, DIFF_TILE_SIZE * tile_row, tile_width, tile_height);

                    FBO::bind(p->animation.enabled ? p->animation.frames[frame_number].gl.fbo : p->gl.fbo);

                    glState.scissorTest.pushSet(true);
                    glState.scissorBox.pushSet(rect);
                    glState.clearColor.pushSet(Vec4());

                    FBO::clear();

                    glState.clearColor.pop();
                    glState.scissorBox.pop();
                    glState.scissorTest.pop();

                    p->substractTaintedArea(rect);
                    deserModified = true;
                }

                ++tile_number;
                --num_tiles;
                --num_empty_tiles;
            }
        } else {
            size_t tile_col = tile_number % CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_row = tile_number / CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_width = std::min(DIFF_TILE_SIZE, width() - DIFF_TILE_SIZE * tile_col);
            size_t tile_height = std::min(DIFF_TILE_SIZE, height() - DIFF_TILE_SIZE * tile_row);
            IntRect rect = IntRect(DIFF_TILE_SIZE * tile_col, DIFF_TILE_SIZE * tile_row, tile_width, tile_height);

            if (max_size < 4 * tile_width * tile_height) return false;

            bool tile_modified = false;

            std::vector<uint32_t> &tile = diff[tile_number];

            if (tile.size() != tile_width * tile_height) {
                tile.clear();
                tile.resize(tile_width * tile_height);
                tile_modified = true;
            }

            if (!tile_modified && std::memcmp(tile.data(), data, 4 * tile_width * tile_height)) {
                tile_modified = true;
            }

            // Upload modified tiles to the bitmap
            if (tile_modified) {
                std::memcpy(tile.data(), data, 4 * tile_width * tile_height);

                if (isMega()) {
                    for (size_t y = 0; y < (size_t)rect.h; ++y) {
                        std::memcpy((uint32_t *)p->megaSurface + p->megaSurface->w * (rect.y + y) + rect.x, (const uint32_t *)data + rect.w * y, 4 * rect.w);
                    }
                } else {
                    TEX::bind(p->animation.enabled ? p->animation.frames[frame_number].gl.tex : p->gl.tex);
                    TEX::uploadSubImage(rect.x, rect.y, rect.w, rect.h, data, GL_RGBA);
                }

                p->addTaintedArea(rect);
                deserModified = true;
            }

            data = (uint8_t *)data + 4 * tile_width * tile_height;
            max_size -= 4 * tile_width * tile_height;
            ++tile_number;
            --num_tiles;
        }
    }

    return true;
}

void Bitmap::sandbox_deserialize_begin(bool is_new)
{
    loresDispCon.disconnect();

    deserModified = is_new;

    deserSizeChanged = is_new;
}

void Bitmap::sandbox_deserialize_end(bool is_sandbox_object)
{
    if (isDisposed()) return;
    if (p->selfLores != nullptr) {
        loresDispCon = p->selfLores->wasDisposed.connect(&Bitmap::loresDisposal, this);
        if (p->selfLores->isDisposed()) {
            loresDisposal();
        }
    }

    if (isDisposed()) return;
    if ((p->selfHires != nullptr && p->selfHires->deserModified) || (p->selfLores != nullptr && p->selfLores->deserModified)) {
        deserModified = true;
    }

    if (isDisposed()) return;
    assumeRubyGC(is_sandbox_object && p->selfHires != nullptr);
}

void Bitmap::sandbox_reinit()
{
    if (isDisposed()) return;

    if (p->animation.enabled) {
        std::unordered_map<std::string, Bitmap *> sources;

        for (BitmapFrame &frame : p->animation.frames) {
            Bitmap *source;
            {
                const auto it = sources.find(frame.path);
                if (it == sources.end()) {
                    Exception e;
                    source = frame.path.empty() ? new Bitmap(e, p->animation.width, p->animation.height, true, false) : new Bitmap(e, frame.path.c_str(), false);
                    if (e.is_error()) {
                        delete source;
                        return;
                    }
                    sources.insert({frame.path, source});
                } else {
                    source = it->second;
                }
            }

            TEXFBO *src_texfbo;
            if (source->isAnimated()) {
                if (frame.originalFrameIndex < 0 || (size_t)frame.originalFrameIndex >= source->p->animation.frames.size()) {
                    delete source;
                    return;
                }
                src_texfbo = &source->p->animation.frames[frame.originalFrameIndex].gl;
            } else {
                if (frame.originalFrameIndex != 0) {
                    delete source;
                    return;
                }
                src_texfbo = &source->p->gl;
            }

            frame.gl = *src_texfbo;
            TEXFBO::clear(*src_texfbo);

            delete source;

            size_t tile_number = 0;
            for (const std::vector<uint32_t> &tile : frame.diff) {
                if (tile.empty()) {
                    ++tile_number;
                    continue;
                }

                size_t tile_col = tile_number % CEIL_DIV_DIFF_TILE_SIZE(width());
                size_t tile_row = tile_number / CEIL_DIV_DIFF_TILE_SIZE(width());
                size_t tile_width = std::min(DIFF_TILE_SIZE, width() - DIFF_TILE_SIZE * tile_col);
                size_t tile_height = std::min(DIFF_TILE_SIZE, height() - DIFF_TILE_SIZE * tile_row);
                IntRect rect = IntRect(DIFF_TILE_SIZE * tile_col, DIFF_TILE_SIZE * tile_row, tile_width, tile_height);

                if (tile.size() != tile_width * tile_height) {
                    continue;
                }

                TEX::bind(frame.gl.tex);
                TEX::uploadSubImage(rect.x, rect.y, rect.w, rect.h, tile.data(), GL_RGBA);

                p->addTaintedArea(rect);

                ++tile_number;
            }
        }
    } else {
        Bitmap *source;
        {
            Exception e;
            source = p->path.empty() ? new Bitmap(e, width(), height(), true, false) : new Bitmap(e, p->path.c_str(), false);
            if (e.is_error() || source->width() != width() || source->height() != height()) {
                delete source;
                return;
            }
        }

        if (isMega()) {
            std::memcpy(p->megaSurface->pixels, source->p->megaSurface->pixels, (size_t)4 * (size_t)p->megaSurface->w * (size_t)p->megaSurface->h);
        } else {
            p->gl = source->p->gl;
            TEXFBO::clear(source->p->gl);
        }

        delete source;

        size_t tile_number = 0;
        for (const std::vector<uint32_t> &tile : p->diff) {
            if (tile.empty()) {
                ++tile_number;
                continue;
            }

            size_t tile_col = tile_number % CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_row = tile_number / CEIL_DIV_DIFF_TILE_SIZE(width());
            size_t tile_width = std::min(DIFF_TILE_SIZE, width() - DIFF_TILE_SIZE * tile_col);
            size_t tile_height = std::min(DIFF_TILE_SIZE, height() - DIFF_TILE_SIZE * tile_row);
            IntRect rect = IntRect(DIFF_TILE_SIZE * tile_col, DIFF_TILE_SIZE * tile_row, tile_width, tile_height);

            if (tile.size() != tile_width * tile_height) {
                continue;
            }

            if (isMega()) {
                for (size_t y = 0; y < (size_t)rect.h; ++y) {
                    std::memcpy((uint32_t *)p->megaSurface + p->megaSurface->w * (rect.y + y) + rect.x, tile.data() + rect.w * y, 4 * rect.w);
                }
            } else {
                TEX::bind(p->gl.tex);
                TEX::uploadSubImage(rect.x, rect.y, rect.w, rect.h, tile.data(), GL_RGBA);
            }

            p->addTaintedArea(rect);

            ++tile_number;
        }
    }
}
#endif // MKXPZ_RETRO
