/*
 ** gl-fun.cpp
 **
 ** This file is part of mkxp.
 **
 ** Copyright (C) 2014 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
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

#include "gl-fun.h"

#include "boost-hash.h"
#include "exception.h"

#ifdef MKXPZ_RETRO
#  include "core.h"
#else
#  include <SDL_video.h>
#endif // MKXPZ_RETRO
#include <cstring>
#include <string>

GLFunctions gl;

typedef const GLubyte* (APIENTRYP _PFNGLGETSTRINGIPROC) (GLenum, GLuint);

static void parseExtensionsCore(_PFNGLGETINTEGERVPROC GetIntegerv, BoostSet<std::string> &out)
{
    _PFNGLGETSTRINGIPROC GetStringi =
#ifdef MKXPZ_RETRO
    (_PFNGLGETSTRINGIPROC) mkxp_retro::hw_render.get_proc_address("glGetStringi");
#else
    (_PFNGLGETSTRINGIPROC) SDL_GL_GetProcAddress("glGetStringi");
#endif // MKXPZ_RETRO
    
    GLint extCount = 0;
    GetIntegerv(GL_NUM_EXTENSIONS, &extCount);
    
    for (GLint i = 0; i < extCount; ++i)
        out.insert((const char*) GetStringi(GL_EXTENSIONS, i));
}

static void parseExtensionsCompat(_PFNGLGETSTRINGPROC GetString, BoostSet<std::string> &out)
{
    const char *ext = (const char*) GetString(GL_EXTENSIONS);
    
    if (!ext)
        return;
    
    char buffer[0x100];
    size_t bufferI;
    
    while (*ext)
    {
        bufferI = 0;
        while (*ext && *ext != ' ')
            buffer[bufferI++] = *ext++;
        
        buffer[bufferI] = '\0';
        
        out.insert(buffer);
        
        if (*ext == ' ')
            ++ext;
    }
}

#ifdef MKXPZ_RETRO
#  define GL_FUN(name, type) mkxp_retro::log_printf(RETRO_LOG_INFO, "gl" #name " %p\n", (gl.name = (type) mkxp_retro::hw_render.get_proc_address("gl" #name EXT_SUFFIX)));
#else
#  define GL_FUN(name, type) gl.name = (type) SDL_GL_GetProcAddress("gl" #name EXT_SUFFIX);
#endif // MKXPZ_RETRO

#define EXC(msg) \
Exception(Exception::MKXPError, "%s", msg)

void initGLFunctions()
{
#define EXT_SUFFIX ""
    mkxp_retro::log_printf(RETRO_LOG_INFO, "Initializing OpenGL 2.0 functions\n");
    GL_20_FUN;
    mkxp_retro::log_printf(RETRO_LOG_INFO, "Initialized OpenGL 2.0 functions\n");
    
    /* Determine GL version */
    mkxp_retro::log_printf(RETRO_LOG_INFO, "Getting OpenGL version (%p)\n", gl.GetString);
    const char *ver = (const char*) gl.GetString(GL_VERSION);
    mkxp_retro::log_printf(RETRO_LOG_INFO, "Got OpenGL version: %s\n", ver);
    
    const char glesPrefix[] = "OpenGL ES ";
    const size_t glesPrefixN = sizeof(glesPrefix)-1;
    
    bool gles = false;
    
    if (!strncmp(ver, glesPrefix, glesPrefixN))
    {
        gles = true;
        gl.glsles = true;
        
        ver += glesPrefixN;
    }
    
    /* Assume single digit */
    int glMajor = *ver - '0';
    
    if (glMajor < 2)
#ifndef GLES2_HEADER
        throw Exception(Exception::MKXPError,
                  "A graphics card that supports OpenGL 2.0 or later is required.\n\n"
                  "Driver information:\n"
                  "Vendor: %s\n"
                  "Renderer: %s\n"
                  "Version: %s\n"
                  "GLSL Version: %s\n",
                  gl.GetString(GL_VENDOR), gl.GetString(GL_RENDERER), gl.GetString(GL_VERSION),
                  gl.GetString(GL_SHADING_LANGUAGE_VERSION));
#else
        // on macOS, we're actually using either desktop GL or Metal due to ANGLE, but every Mac that supports Sierra
        // (officially or otherwise) should support ANGLE, so this should never be seen. Probably, anyway. Don't @ me
        throw EXC("A graphics card that supports OpenGL ES 2.0 or later is required.");
#endif
    
    if (gles)
    {
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initializing OpenGL ES functions\n");
        GL_ES_FUN;
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initialized OpenGL ES functions\n");
    }
    
    BoostSet<std::string> ext;
    
    if (glMajor >= 3) {
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initializing extensions (core)\n");
        parseExtensionsCore(gl.GetIntegerv, ext);
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initialized extensions (core)\n");
    } else {
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initializing extensions (compatibility)\n");
        parseExtensionsCompat(gl.GetString, ext);
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initialized extensions (compatibility)\n");
    }
    
#define HAVE_EXT(_ext) ext.contains("GL_" #_ext)
    
    /* FBO entrypoints */
    if (glMajor >= 3 || HAVE_EXT(ARB_framebuffer_object))
    {
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initializing FBO functions for OpenGL 3\n");
#undef EXT_SUFFIX
#define EXT_SUFFIX ""
        GL_FBO_FUN;
        GL_FBO_BLIT_FUN;
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initialized FBO functions for OpenGL 3\n");
    }
    else if (gles && glMajor == 2)
    {
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initializing FBO functions for OpenGL 2\n");
        GL_FBO_FUN;
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initialized FBO functions for OpenGL 2\n");
    }
    else if (HAVE_EXT(EXT_framebuffer_object))
    {
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initializing FBO functions for extension\n");
#undef EXT_SUFFIX
#define EXT_SUFFIX "EXT"
        GL_FBO_FUN;
        
        if (HAVE_EXT(EXT_framebuffer_blit))
        {
            GL_FBO_BLIT_FUN;
        }
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initialized FBO functions for extension\n");
    }
    else
    {
        throw EXC("No FBO support available");
    }
    
    /* VAO entrypoints */
    if (HAVE_EXT(ARB_vertex_array_object) || glMajor >= 3)
    {
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initializing VAO functions (ARB)\n");
#undef EXT_SUFFIX
#define EXT_SUFFIX ""
        GL_VAO_FUN;
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initialized VAO functions (ARB)\n");
    }
    else if (HAVE_EXT(APPLE_vertex_array_object))
    {
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initializing VAO functions (Apple)\n");
#undef EXT_SUFFIX
#define EXT_SUFFIX "APPLE"
        GL_VAO_FUN;
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initialized VAO functions (Apple)\n");
    }
    else if (HAVE_EXT(OES_vertex_array_object))
    {
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initializing VAO functions (OES)\n");
#undef EXT_SUFFIX
#define EXT_SUFFIX "OES"
        GL_VAO_FUN;
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initialized VAO functions (OES)\n");
    }
    
    /* Debug callback entrypoints */
    if (HAVE_EXT(KHR_debug))
    {
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initializing debug functions (KHR)\n");
#undef EXT_SUFFIX
#define EXT_SUFFIX ""
        GL_DEBUG_KHR_FUN;
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initialized debug functions (KHR)\n");
    }
    else if (HAVE_EXT(ARB_debug_output))
    {
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initializing debug functions (ARB)\n");
#undef EXT_SUFFIX
#define EXT_SUFFIX "ARB"
        GL_DEBUG_KHR_FUN;
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initialized debug functions (ARB)\n");
    }
    
    if (HAVE_EXT(GREMEDY_string_marker))
    {
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initializing debug functions (Graphic Remedy)\n");
#undef EXT_SUFFIX
#define EXT_SUFFIX "GREMEDY"
        GL_GREMEMDY_FUN;
        mkxp_retro::log_printf(RETRO_LOG_INFO, "Initialized debug functions (Graphic Remedy)\n");
    }
    
    /* Misc caps */
    if (!gles || glMajor >= 3 || HAVE_EXT(EXT_unpack_subimage))
        gl.unpack_subimage = true;
    
    if (!gles || glMajor >= 3 || HAVE_EXT(OES_texture_npot))
        gl.npot_repeat = true;
}
