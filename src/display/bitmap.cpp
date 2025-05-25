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
#  include "mkxp-polyfill.h" // std::lround
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
        std::vector<TEXFBO> frames;
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
        
        inline TEXFBO &currentFrame() {
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
    std::string path;
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
        return (animation.enabled) ? animation.currentFrame() : gl;
    }
    
    void prepare()
    {
        if (!animation.enabled || !animation.playing) return;
        
        animation.updateTimer();
    }
    
    void allocSurface()
    {
#ifdef MKXPZ_RETRO
        surface = new SDL_Surface {.w = gl.width, .h = gl.height, .pixels = STBI_MALLOC(4 * gl.width * gl.height)};
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

            TEXFBO cframe = animation.currentFrame();
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
        FBO::bind((animation.enabled) ? animation.currentFrame().fbo : gl.fbo);
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
        PHYSFS_seek(ops->get(), 0);
        if (PHYSFS_readBytes(ops->get(), header_buffer, 6) == 6 && (!std::memcmp(header_buffer, "GIF87a", 6) || !std::memcmp(header_buffer, "GIF89a", 6))) {
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
            gif_data_size = PHYSFS_fileLength(ops->get());
#else
            gif_data_size = ops.size(&ops);
#endif // MKXPZ_RETRO
            
            gif_data = new unsigned char[gif_data_size];
#ifdef MKXPZ_RETRO
            PHYSFS_seek(ops->get(), 0);
            PHYSFS_readBytes(ops->get(), gif_data, gif_data_size);
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
            PHYSFS_seek(ops->get(), 0);

            struct file {
                struct FileSystem::File *handle;
                uint64_t offset;
            };

            const static stbi_io_callbacks callbacks = {
                .read = [](void *handle, char *buf, int size) {
                    assert(size >= 0);
                    int n = PHYSFS_readBytes(((struct file *)handle)->handle->get(), buf, size);
                    assert(((struct file *)handle)->offset + (uint64_t)n >= ((struct file *)handle)->offset);
                    ((struct file *)handle)->offset += n;
                    return n;
                },
                .skip = [](void *handle, int size) {
                    assert(size >= 0);
                    assert(((struct file *)handle)->offset + (uint64_t)size >= ((struct file *)handle)->offset);
                    PHYSFS_seek(((struct file *)handle)->handle->get(), (((struct file *)handle)->offset += (uint64_t)size));
                },
                .eof = [](void *handle) {
                    return PHYSFS_eof(((struct file *)handle)->handle->get());
                },
            };

            struct file file {
                .handle = ops.get(),
                .offset = 0,
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

Bitmap::Bitmap(Exception &exception, const char *filename)
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
#ifdef MKXPZ_RETRO
        p->path = filename;
#endif // MKXPZ_RETRO
        
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
            p->animation.frames.push_back(texfbo);
        }
        
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
    GUARD(initFromSurface(exception, imgSurf, hiresBitmap, false));
#ifdef MKXPZ_RETRO
    p->path = filename;
#endif // MKXPZ_RETRO
}

Bitmap::Bitmap(Exception &exception, int width, int height, bool isHires)
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
    
    GUARD(clear(exception));
}

Bitmap::Bitmap(Exception &exception, void *pixeldata, int width, int height)
{
#ifdef MKXPZ_RETRO
    SDL_Surface *surface = new SDL_Surface;

    stbi_uc *image = (stbi_uc *)STBI_MALLOC((size_t)4 * (size_t)width * (size_t)height * sizeof(stbi_uc));
    if (image == nullptr)
        MKXPZ_THROW(std::bad_alloc());

    surface->pixels = image;
    surface->w = width;
    surface->h = height;
#else
    SDL_Surface *surface = SDL_CreateRGBSurface(0, width, height, p->format->BitsPerPixel,
                                                p->format->Rmask,
                                                p->format->Gmask,
                                                p->format->Bmask,
                                                p->format->Amask);
    
    if (!surface)
        MKXPZ_THROW(std::bad_alloc());
    
    memcpy(surface->pixels, pixeldata, width*height*(p->format->BitsPerPixel/8));
#endif // MKXPZ_RERTRO
    
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
    
    p->addTaintedArea(rect());
}

// frame is -2 for "any and all", -1 for "current", anything else for a specific frame
Bitmap::Bitmap(Exception &exception, const Bitmap &other, int frame)
{
    GUARD(other.guardDisposed(exception));
    GUARD(other.ensureNonMega(exception));
    if (frame > -2) GUARD(other.ensureAnimated(exception));
    
    if (other.hasHires()) {
        Debug() << "BUG: High-res Bitmap from animation not implemented";
    }

    p = new BitmapPrivate(this);
#ifdef MKXPZ_RETRO
    p->path = other.p->path;
#endif // MKXPZ_RETRO
    
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
            GLMeta::blitSource(frames[clamp(frame, 0, (int)frames.size() - 1)], SameScale);
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
        
        for (TEXFBO &sourceframe : other.getFrames()) {
            TEXFBO newframe = shState->texPool().request(exception, p->animation.width, p->animation.height);
            if (exception.is_error()) {
                releaseResources();
                return;
            }
            
            GLMeta::blitBegin(newframe, false, SameScale);
            GLMeta::blitSource(sourceframe, SameScale);
            GLMeta::blitRectangle(rect(), rect());
            GLMeta::blitEnd();
            
            p->animation.frames.push_back(newframe);
        }
    }
    
    p->addTaintedArea(rect());
}

Bitmap::Bitmap(Exception &exception, TEXFBO &other)
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

    p->addTaintedArea(rect());
}

Bitmap::Bitmap(Exception &exception, SDL_Surface *imgSurf, SDL_Surface *imgSurfHires, bool forceMega)
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

    GUARD(initFromSurface(exception, imgSurf, hiresBitmap, forceMega));
}

Bitmap::~Bitmap()
{
    dispose();

    loresDispCon.disconnect();
}

void Bitmap::initFromSurface(Exception &exception, SDL_Surface *imgSurf, Bitmap *hiresBitmap, bool forceMega)
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

void Bitmap::setHires(Exception &exception, Bitmap *hires) {
    GUARD(guardDisposed(exception));

    Debug() << "BUG: High-res Bitmap setHires not fully implemented, expect bugs";
    GUARD(hires->setLores(exception, this));
    p->selfHires = hires;
}

void Bitmap::setLores(Exception &exception, Bitmap *lores) {
    GUARD(guardDisposed(exception));

    p->selfLores = lores;
    loresDispCon = lores->wasDisposed.connect(&Bitmap::loresDisposal, this);
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
        destPos = (destLen > 0 == sourceLen > 0) ? dStart : dEnd;
        destLen = (destLen > 0 == sourceLen > 0) ? dLength : -dLength;
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
                mkxp_retro::log_printf(RETRO_LOG_ERROR, "not implemented: stretchBlt for sources larger than the max texture size\n");
                std::abort();
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
    
#ifndef MKXPZ_RETRO // TODO
    if (blitTemp)
        SDL_FreeSurface(blitTemp);
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
    
#ifndef MKXPZ_RETRO
    if (p->surface)
    {
        uint32_t &surfPixel = getPixelAt(p->surface, p->format, x, y);
        surfPixel = SDL_MapRGBA(p->format, pixel[0], pixel[1], pixel[2], pixel[3]);
    }
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

#if defined(MKXPZ_RETRO) && defined(MKXPZ_BIG_ENDIAN)
    output = (uint8_t *)output - output_size;
#endif

    if (!p->animation.enabled && (p->surface || p->megaSurface)) {
        void *src = (p->megaSurface) ? p->megaSurface->pixels : p->surface->pixels;
        memcpy(output, src, output_size);
    }
    else {
        FBO::bind(getGLTypes().fbo);
        gl.ReadPixels(0,0,width(),height(),GL_RGBA,GL_UNSIGNED_BYTE,output);
    }

#if defined(MKXPZ_RETRO) && defined(MKXPZ_BIG_ENDIAN)
    std::reverse((uint8_t *)output, (uint8_t *)output + output_size);
#endif

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
    
#if defined(MKXPZ_RETRO) && defined(MKXPZ_BIG_ENDIAN)
    pixel_data = (uint8_t *)pixel_data - size;
    std::reverse((uint8_t *)pixel_data, (uint8_t *)pixel_data + size);
#endif

    TEX::bind(getGLTypes().tex);
    TEX::uploadImage(w, h, pixel_data, GL_RGBA);

#if defined(MKXPZ_RETRO) && defined(MKXPZ_BIG_ENDIAN)
    std::reverse((uint8_t *)pixel_data, (uint8_t *)pixel_data + size);
#endif
    
    taintArea(IntRect(0,0,w,h));
    p->onModified();
}

void Bitmap::saveToFile(Exception &exception, const char *filename)
{
    GUARD(guardDisposed(exception));
    
    if (hasHires()) {
        Debug() << "GAME BUG: Game is calling saveToFile on low-res Bitmap; you may want to patch the game to improve graphics quality.";
    }

    SDL_Surface *surf;
    
    if (p->surface || p->megaSurface) {
        surf = (p->surface) ? p->surface : p->megaSurface;
    }
    else {
#ifndef MKXPZ_RETRO
        surf = SDL_CreateRGBSurface(0, width(), height(),p->format->BitsPerPixel, p->format->Rmask,p->format->Gmask,p->format->Bmask,p->format->Amask);
        
        if (!surf)
            MKXPZ_THROW(std::bad_alloc());
        
        GUARD(getRaw(exception, surf->pixels, surf->w * surf->h * 4));
#endif // MKXPZ_RETRO
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
    
#ifndef MKXPZ_RETRO
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
static void applyShadow(SDL_Surface *&in, const SDL_Color &c)
#else
static void applyShadow(SDL_Surface *&in, const SDL_PixelFormat &fm, const SDL_Color &c)
#endif // MKXPZ_RETRO
{
#ifdef MKXPZ_RETRO
    SDL_Surface *out = new SDL_Surface {.w = in->w+1, .h = in->h+1, .pixels = STBI_MALLOC(4 * (in->w+1) * (in->h+1))};
    if (out->pixels == nullptr) {
        MKXPZ_THROW(std::bad_alloc());
    }
    const int inPitch = in->w;
    const int outPitch = out->w;
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
    
    for (int y = 0; y < in->h+1; ++y)
        for (int x = 0; x < in->w+1; ++x)
        {
            /* src: input pixel, shd: shadow pixel */
            uint32_t src = 0, shd = 0;
            
            /* Output pixel location */
            uint32_t *outP = ((uint32_t*) ((uint8_t*) out->pixels + y*outPitch)) + x;
            
            if (y < in->h && x < in->w)
                src = ((uint32_t*) ((uint8_t*) in->pixels + y*inPitch))[x];
            
            if (y > 0 && x > 0)
                shd = ((uint32_t*) ((uint8_t*) in->pixels + (y-1)*inPitch))[x-1];
            
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
            
            if (x == 0 || y == 0)
            {
                *outP = src;
                continue;
            }
            
            if (x == in->w || y == in->h)
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
IntRect Bitmap::textRect(Exception &exception, const char *str, bool solid)
{
    FT_Face font;
    GUARD_V(IntRect(), font = p->font->getSdlFont(exception));

    // TODO: handle kerning
    int bitmap_left = 0;
    int bitmap_right = 0;
    int bitmap_top = -font->size->metrics.ascender / 64;
    int bitmap_bottom = -font->size->metrics.descender / 64;
    int glyph_x = 0;
    int glyph_y = 0;

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

            if (FT_Load_Char(font, charcode, solid ? (FT_LOAD_DEFAULT | FT_LOAD_BITMAP_METRICS_ONLY | FT_LOAD_TARGET_MONO) : (FT_LOAD_DEFAULT | FT_LOAD_BITMAP_METRICS_ONLY | FT_LOAD_TARGET_NORMAL)))
                continue;

            int glyph_left = glyph_x + font->glyph->bitmap_left;
            int glyph_right = glyph_left + font->glyph->bitmap.width;
            int glyph_top = glyph_y - font->glyph->bitmap_top;
            int glyph_bottom = glyph_top + font->glyph->bitmap.rows;

            glyph_x += font->glyph->advance.x / 64;
            glyph_y += font->glyph->advance.y / 64;

            bitmap_left = std::min(bitmap_left, std::min(glyph_left, glyph_x));
            bitmap_right = std::max(bitmap_right, std::max(glyph_right, glyph_x));
            bitmap_top = std::min(bitmap_top, std::min(glyph_top, glyph_y));
            bitmap_bottom = std::max(bitmap_bottom, std::max(glyph_bottom, glyph_y));
        }
    }
    while (*ptr++ != 0);

    return IntRect(bitmap_left, bitmap_top, bitmap_right - bitmap_left, bitmap_bottom - bitmap_top);
}

SDL_Surface *Bitmap::drawTextInner(Exception &exception, FT_Face font, const char *str, SDL_Color &c, size_t outline)
{
    // TODO: handle kerning
    const bool solid = p->font->isSolid();
    IntRect bitmapRect;
    GUARD_V(nullptr, bitmapRect = textRect(exception, str, solid));
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

            if (outline > 0)
            {
                if (FT_Load_Char(font, charcode, solid ? (FT_LOAD_DEFAULT | FT_LOAD_TARGET_MONO) : (FT_LOAD_DEFAULT | FT_LOAD_TARGET_NORMAL)))
                    continue;
                if (FT_Get_Glyph(font->glyph, &glyph))
                    continue;
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

            int glyph_left = glyph_x + (outline > 0 ? ((FT_BitmapGlyph)glyph)->left : font->glyph->bitmap_left);
            int glyph_top = glyph_y - (outline > 0 ? ((FT_BitmapGlyph)glyph)->top : font->glyph->bitmap_top);
            FT_Bitmap *bitmap = outline > 0 ? &((FT_BitmapGlyph)glyph)->bitmap : &font->glyph->bitmap;
            unsigned int glyph_width = bitmap->width;
            unsigned int glyph_height = bitmap->rows;

            if (solid)
                for (unsigned int y = 0; y < glyph_height; ++y)
                {
                    for (unsigned int x = 0; x < glyph_width; ++x)
                    {
                        if (((uint8_t *)bitmap->buffer)[bitmap->pitch * y + x / 8] & (1 << (7 - (x % 8))))
                        {
                            ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x)] = c.r;
                            ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x) + 1] = c.g;
                            ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x) + 2] = c.b;
                            ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x) + 3] = 255;
                        }
                    }
                }
            else
                for (unsigned int y = 0; y < glyph_height; ++y)
                {
                    for (unsigned int x = 0; x < glyph_width; ++x)
                    {
                        uint8_t alpha = ((uint8_t *)bitmap->buffer)[bitmap->pitch * y + x];
                        if (alpha > 0)
                        {
                            ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x)] = c.r;
                            ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x) + 1] = c.g;
                            ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x) + 2] = c.b;
                            ((uint8_t *)txtSurf->pixels)[4 * (bitmapRect.w * (glyph_top + y) + glyph_left + x) + 3] = alpha;
                        }
                    }
                }

            glyph_x += font->glyph->advance.x / 64;
            glyph_y += font->glyph->advance.y / 64;

            if (outline > 0)
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
    
    if (hasHires()) {
        Font *_loresFont, *_hiresFont;
        GUARD(_loresFont = &getFont(exception));
        GUARD(_hiresFont = &p->selfHires->getFont(exception));
        Font &loresFont = *_loresFont;
        Font &hiresFont = *_hiresFont;
        // Disable the illegal font size check when creating a high-res font.
        hiresFont.setSizeNoCheck(loresFont.getSize() * p->selfHires->width() / width());
        hiresFont.setBold(loresFont.getBold());
        hiresFont.setColor(loresFont.getColor());
        hiresFont.setItalic(loresFont.getItalic());
        hiresFont.setShadow(loresFont.getShadow());
        hiresFont.setOutline(loresFont.getOutline());
        hiresFont.setOutColor(loresFont.getOutColor());

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
    FT_Face font;
    GUARD(font = p->font->getSdlFont(exception));
#else
    TTF_Font *font;
    GUARD(font = p->font->getSdlFont(exception));
#endif // MKXPZ_RETRO
    const Color &fontColor = p->font->getColor();
    const Color &outColor = p->font->getOutColor();
    
    SDL_Color c = fontColor.toSDLColor();
    c.a = 255;
    
    SDL_Surface *txtSurf;
    
#ifdef MKXPZ_RETRO
    GUARD(txtSurf = drawTextInner(exception, font, str, c, 0));
#else
    if (p->font->isSolid())
        txtSurf = TTF_RenderUTF8_Solid(font, str, c);
    else
        txtSurf = TTF_RenderUTF8_Blended(font, str, c);
    
    p->ensureFormat(txtSurf, SDL_PIXELFORMAT_ABGR8888);
#endif // MKXPZ_RETRO
    
    int rawTxtSurfH = txtSurf->h;
    
    if (p->font->getShadow())
#ifdef MKXPZ_RETRO
        applyShadow(txtSurf, c);
#else
        applyShadow(txtSurf, *p->format, c);
#endif // MKXPZ_RETRO
    
    /* outline using TTF_Outline and blending it together with SDL_BlitSurface
     * FIXME: outline is forced to have the same opacity as the font color */
    if (p->font->getOutline())
    {
        SDL_Color co = outColor.toSDLColor();
        co.a = 255;
        SDL_Surface *outline;
        // Handle high-res for outline.
        int scaledOutlineSize = OUTLINE_SIZE;
        if (p->selfLores) {
            scaledOutlineSize = scaledOutlineSize * width() / p->selfLores->width();
        }
#ifdef MKXPZ_RETRO
        outline = drawTextInner(exception, font, str, co, scaledOutlineSize);
        if (exception.is_error())
        {
            stbi_image_free(txtSurf->pixels);
            delete txtSurf;
            return;
        }
        for (size_t y = 0; y < txtSurf->h; ++y)
        {
            for (size_t x = 0; x < txtSurf->w; ++x)
            {
                uint8_t *src = (uint8_t *)&((uint32_t *)txtSurf->pixels)[txtSurf->w * y + x];
                uint8_t *dst = (uint8_t *)&((uint32_t *)outline->pixels)[(txtSurf->w + 2 * scaledOutlineSize) * (y + scaledOutlineSize) + (x + scaledOutlineSize)];
                float srcAlpha = src[3] / 255.0f;
                float dstAlpha = dst[3] / 255.0f;
                float outAlpha = srcAlpha + (1 - srcAlpha) * dstAlpha;
                for (size_t i = 0; i < 3; ++i)
                    dst[i] = (uint8_t)clamp(std::lround((srcAlpha * src[i] + ((1 - srcAlpha) * dstAlpha) * dst[i]) / outAlpha), 0L, 255L);
                dst[3] = (uint8_t)clamp(std::lround(255.0f * outAlpha), 0L, 255L);
            }
        }
        stbi_image_free(txtSurf->pixels);
        delete txtSurf;
        txtSurf = outline;
#else
        /* set the next font render to render the outline */
        TTF_SetFontOutline(font, scaledOutlineSize);
        if (p->font->isSolid())
            outline = TTF_RenderUTF8_Solid(font, str, co);
        else
            outline = TTF_RenderUTF8_Blended(font, str, co);
        
        p->ensureFormat(outline, SDL_PIXELFORMAT_ABGR8888);
        SDL_Rect outRect = {scaledOutlineSize, scaledOutlineSize, txtSurf->w, txtSurf->h};
        
        SDL_SetSurfaceBlendMode(txtSurf, SDL_BLENDMODE_BLEND);
        SDL_BlitSurface(txtSurf, NULL, outline, &outRect);
        SDL_FreeSurface(txtSurf);
        txtSurf = outline;
        /* reset outline to 0 */
        TTF_SetFontOutline(font, 0);
#endif // MKXPZ_RETRO
    }
    
    int alignX = rect.x;
    
    switch (align)
    {
        default:
        case Left :
            break;
            
        case Center :
            alignX += (rect.w - txtSurf->w) / 2;
            break;
            
        case Right :
            alignX += rect.w - txtSurf->w;
            break;
    }
    
    if (alignX < rect.x)
        alignX = rect.x;
    
    int alignY = rect.y + (rect.h - rawTxtSurfH) / 2;
    
    float squeeze = (float) rect.w / txtSurf->w;
    
    if (squeeze > 1)
        squeeze = 1;
    
    IntRect destRect(alignX, alignY, 0, 0);
    destRect.w = std::min(rect.w, (int)(txtSurf->w * squeeze));
    destRect.h = std::min(rect.h, txtSurf->h);
    
    destRect.w = std::min(destRect.w, width() - destRect.x);
    destRect.h = std::min(destRect.h, height() - destRect.y);
    
    IntRect sourceRect;
    sourceRect.w = destRect.w / squeeze;
    sourceRect.h = destRect.h;
    
    Bitmap txtBitmap(exception, txtSurf, nullptr, true);
    if (exception.is_error()) {
        return;
    }
    bool smooth = squeeze != 1.0f;
    GUARD(stretchBlt(exception, destRect, txtBitmap, sourceRect, fontColor.alpha, smooth));
}

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

IntRect Bitmap::textSize(Exception &exception, const char *str)
{
    GUARD_V(IntRect(), guardDisposed(exception));
    
    GUARD_MEGA(IntRect());
    GUARD_ANIMATED(IntRect());
    
    // TODO: High-res Bitmap textSize not implemented, but I think it's the same as low-res?
    // Need to double-check this.

    std::string fixed = fixupString(str);
    str = fixed.c_str();

#ifdef MKXPZ_RETRO
    IntRect rect;
    GUARD_V(IntRect(), rect = textRect(exception, str, p->font->isSolid()));
    return IntRect(0, 0, rect.w, rect.h);
#else
    TTF_Font *font;
    GUARD_V(IntRect(), font = p->font->getSdlFont(exception));
    
    int w, h;
    TTF_SizeUTF8(font, str, &w, &h);
    
    /* If str is one character long, *endPtr == 0 */
    const char *endPtr;
    uint16_t ucs2 = utf8_to_ucs2(str, &endPtr);
    
    /* For cursive characters, returning the advance
     * as width yields better results */
    if (p->font->getItalic() && *endPtr == '\0')
        TTF_GlyphMetrics(font, ucs2, 0, 0, 0, 0, &w);
    
    return IntRect(0, 0, w, h);
#endif // MKXPZ_RETRO
}

DEF_ATTR_RD_SIMPLE(Bitmap, Font, Font&, *p->font)

void Bitmap::setFont(Exception &exception, Font &value)
{
    GUARD(guardDisposed(exception));

    // High-res support handled in drawText, not here.
    *p->font = value;
}

void Bitmap::setInitFont(Font *value)
{
#ifndef MKXPZ_RETRO
    if (hasHires()) {
        Font *hiresFont = p->selfHires->p->font;
        if (hiresFont && hiresFont != &shState->defaultFont())
        {
            // Disable the illegal font size check when creating a high-res font.
            hiresFont->setSizeNoCheck(hiresFont->getSize() * p->selfHires->width() / width());
        }
    }
#endif // MKXPZ_RETRO

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
        
        p->animation.frames.push_back(p->gl);
        
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
        p->animation.frames.push_back(newframe);
        ret = (int)p->animation.frames.size();
    }
    else {
        p->animation.frames.insert(p->animation.frames.begin() + clamp(position, 0, (int)p->animation.frames.size()), newframe);
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
    shState->texPool().release(p->animation.frames[pos]);
    p->animation.frames.erase(p->animation.frames.begin() + pos);
    
    // Change the animated bitmap back to a normal one if there's only one frame left
    if (p->animation.frames.size() == 1) {
        
        p->animation.enabled = false;
        p->animation.playing = false;
        p->animation.width = 0;
        p->animation.height = 0;
        p->animation.lastFrame = 0;
        
        p->gl = p->animation.frames[0];
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

std::vector<TEXFBO> &Bitmap::getFrames() const
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

void Bitmap::assumeRubyGC()
{
    p->assumingRubyGC = true;
}

void Bitmap::releaseResources()
{
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
        for (TEXFBO &tex : p->animation.frames)
            shState->texPool().release(tex);
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
bool Bitmap::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
    if (isDisposed()) return mkxp_sandbox::sandbox_serialize(false, data, max_size);
    if (!mkxp_sandbox::sandbox_serialize(true, data, max_size)) return false;

    if (!mkxp_sandbox::sandbox_serialize(p->path, data, max_size)) return false;

    if (!mkxp_sandbox::sandbox_serialize((int32_t)width(), data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize((int32_t)height(), data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->animation.enabled, data, max_size)) return false;

    if (p->animation.enabled) {
        if (!mkxp_sandbox::sandbox_serialize(p->animation.fps, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(p->animation.playing, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(p->animation.needsReset, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(p->animation.loop, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize((int32_t)p->animation.lastFrame, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(p->animation.playTime, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_serialize(p->animation.startTime, data, max_size)) return false;
    }

    if (!mkxp_sandbox::sandbox_serialize(p->font, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->selfHires, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_serialize(p->selfLores, data, max_size)) return false;

    // TODO: serialize bitmap pixels

    return true;
}

bool Bitmap::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
    {
        bool active;
        if (!mkxp_sandbox::sandbox_deserialize(active, data, max_size)) return false;
        if (!active) {
            dispose();
            return true;
        }
        // TODO: undispose
    }

    if (!mkxp_sandbox::sandbox_deserialize(p->path, data, max_size)) return false;

    if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->gl.width, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->gl.height, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->animation.enabled, data, max_size)) return false;

    if (p->animation.enabled) {
        p->animation.width = p->gl.width;
        p->animation.height = p->gl.height;
        if (!mkxp_sandbox::sandbox_deserialize(p->animation.fps, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_deserialize(p->animation.playing, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_deserialize(p->animation.needsReset, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_deserialize(p->animation.loop, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->animation.lastFrame, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_deserialize(p->animation.playTime, data, max_size)) return false;
        if (!mkxp_sandbox::sandbox_deserialize(p->animation.startTime, data, max_size)) return false;
    }

    if (!mkxp_sandbox::sandbox_deserialize(p->font, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->selfHires, data, max_size)) return false;
    if (!mkxp_sandbox::sandbox_deserialize(p->selfLores, data, max_size)) return false;

    // TODO: deserialize bitmap pixels

    // TODO: reload the bitmap

    return true;
}
#endif // MKXPZ_RETRO
