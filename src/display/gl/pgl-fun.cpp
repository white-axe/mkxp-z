/*
 ** pgl-fun.cpp
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

#include <portablegl.h>
#include <KHR/khrplatform.h>
#include <string>
#include <unordered_map>

extern std::unordered_map<GLenum, GLenum> mkxp_pgl_enum_map;
static inline GLenum enumconv(GLenum x) {
    const auto it = mkxp_pgl_enum_map.find(x);
    return it == mkxp_pgl_enum_map.end() ? -1 : it->second;
}

static KHRONOS_APIENTRY void _glViewport(int x, int y, GLsizei width, GLsizei height) {
    glViewport(x, y, width, height);
}

static KHRONOS_APIENTRY GLubyte *_glGetString(GLenum name) {
    return glGetString(enumconv(name));
}

static KHRONOS_APIENTRY void _glGetBooleanv(GLenum pname, GLboolean* data) {
    glGetBooleanv(enumconv(pname), data);
}

static KHRONOS_APIENTRY void _glGetFloatv(GLenum pname, GLfloat* data) {
    return glGetFloatv(enumconv(pname), data);
}

static KHRONOS_APIENTRY void _glGetIntegerv(GLenum pname, GLint* data) {
    glGetIntegerv(enumconv(pname), data);
}

static KHRONOS_APIENTRY GLboolean _glIsEnabled(GLenum cap) {
    return glIsEnabled(enumconv(cap));
}

static KHRONOS_APIENTRY GLboolean _glIsProgram(GLuint program) {
    return glIsProgram(program);
}

static KHRONOS_APIENTRY void _glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    glClearColor(red, green, blue, alpha);
}

static KHRONOS_APIENTRY void _glClearDepth(GLclampf depth) {
    glClearDepth(depth);
}

static KHRONOS_APIENTRY void _glDepthFunc(GLenum func) {
    glDepthFunc(enumconv(func));
}

static KHRONOS_APIENTRY void _glDepthRange(GLclampf nearVal, GLclampf farVal) {
    glDepthRange(nearVal, farVal);
}

static KHRONOS_APIENTRY void _glDepthMask(GLboolean flag) {
    glDepthMask(flag);
}

static KHRONOS_APIENTRY void _glBlendFunc(GLenum sfactor, GLenum dfactor) {
    glBlendFunc(enumconv(sfactor), enumconv(dfactor));
}

static KHRONOS_APIENTRY void _glBlendEquation(GLenum mode) {
    glBlendEquation(enumconv(mode));
}

static KHRONOS_APIENTRY void _glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    glBlendFuncSeparate(enumconv(srcRGB), enumconv(dstRGB), enumconv(srcAlpha), enumconv(dstAlpha));
}

static KHRONOS_APIENTRY void _glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    glBlendEquationSeparate(enumconv(modeRGB), enumconv(modeAlpha));
}

static KHRONOS_APIENTRY void _glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    glBlendColor(red, green, blue, alpha);
}

static KHRONOS_APIENTRY void _glClear(GLbitfield mask) {
    glClear(GL_COLOR_BUFFER_BIT);
}

static KHRONOS_APIENTRY void _glProvokingVertex(GLenum provokeMode) {
    glProvokingVertex(enumconv(provokeMode));
}

static KHRONOS_APIENTRY void _glEnable(GLenum cap) {
    glEnable(enumconv(cap));
}

static KHRONOS_APIENTRY void _glDisable(GLenum cap) {
    glDisable(enumconv(cap));
}

static KHRONOS_APIENTRY void _glCullFace(GLenum mode) {
    glCullFace(enumconv(mode));
}

static KHRONOS_APIENTRY void _glFrontFace(GLenum mode) {
    glFrontFace(enumconv(mode));
}

static KHRONOS_APIENTRY void _glPolygonMode(GLenum face, GLenum mode) {
    glPolygonMode(enumconv(face), enumconv(mode));
}

static KHRONOS_APIENTRY void _glPointSize(GLfloat size) {
    glPointSize(size);
}

static KHRONOS_APIENTRY void _glPointParameteri(GLenum pname, GLint param) {
    glPointParameteri(enumconv(pname), param);
}

static KHRONOS_APIENTRY void _glLineWidth(GLfloat width) {
    glLineWidth(width);
}

static KHRONOS_APIENTRY void _glLogicOp(GLenum opcode) {
    glLogicOp(enumconv(opcode));
}

static KHRONOS_APIENTRY void _glPolygonOffset(GLfloat factor, GLfloat units) {
    glPolygonOffset(factor, units);
}

static KHRONOS_APIENTRY void _glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    glScissor(x, y, width, height);
}

static KHRONOS_APIENTRY void _glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    glStencilFunc(enumconv(func), ref, mask);
}

static KHRONOS_APIENTRY void _glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    glStencilFuncSeparate(enumconv(face), enumconv(func), ref, mask);
}

static KHRONOS_APIENTRY void _glStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass) {
    glStencilOp(enumconv(sfail), enumconv(dpfail), enumconv(dppass));
}

static KHRONOS_APIENTRY void _glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    glStencilOpSeparate(enumconv(face), enumconv(sfail), enumconv(dpfail), enumconv(dppass));
}

static KHRONOS_APIENTRY void _glClearStencil(GLint s) {
    glClearStencil(s);
}

static KHRONOS_APIENTRY void _glStencilMask(GLuint mask) {
    glStencilMask(mask);
}

static KHRONOS_APIENTRY void _glStencilMaskSeparate(GLenum face, GLuint mask) {
    glStencilMaskSeparate(enumconv(face), mask);
}

static KHRONOS_APIENTRY void _glGenTextures(GLsizei n, GLuint* textures) {
    glGenTextures(n, textures);
}

static KHRONOS_APIENTRY void _glDeleteTextures(GLsizei n, const GLuint* textures) {
    glDeleteTextures(n, textures);
}

static KHRONOS_APIENTRY void _glBindTexture(GLenum target, GLuint texture) {
    glBindTexture(enumconv(target), texture);
}

static KHRONOS_APIENTRY void _glTexParameteri(GLenum target, GLenum pname, GLint param) {
    glTexParameteri(enumconv(target), enumconv(pname), param);
}

static KHRONOS_APIENTRY void _glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
    glTexParameterfv(enumconv(target), enumconv(pname), params);
}

static KHRONOS_APIENTRY void _glTextureParameteri(GLuint texture, GLenum pname, GLint param) {
    glTextureParameteri(texture, enumconv(pname), param);
}

static KHRONOS_APIENTRY void _glPixelStorei(GLenum pname, GLint param) {
    glPixelStorei(enumconv(pname), param);
}

static KHRONOS_APIENTRY void _glTexImage1D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid* data) {
    glTexImage1D(enumconv(target), level, internalFormat, width, border, enumconv(format), enumconv(type), data);
}

static KHRONOS_APIENTRY void _glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* data) {
    glTexImage2D(enumconv(target), level, internalFormat, width, height, border, enumconv(format), enumconv(type), data);
}

static KHRONOS_APIENTRY void _glTexImage3D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid* data) {
    glTexImage3D(enumconv(target), level, internalFormat, width, height, depth, border, enumconv(format), enumconv(type), data);
}

static KHRONOS_APIENTRY void _glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid* data) {
    glTexSubImage1D(enumconv(target), level, xoffset, width, enumconv(format), enumconv(type), data);
}

static KHRONOS_APIENTRY void _glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* data) {
    glTexSubImage2D(enumconv(target), level, xoffset, yoffset, width, height, enumconv(format), enumconv(type), data);
}

static KHRONOS_APIENTRY void _glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid* data) {
    glTexSubImage3D(enumconv(target), level, xoffset, yoffset, zoffset, width, height, depth, enumconv(format), enumconv(type), data);
}

static KHRONOS_APIENTRY void _glGenVertexArrays(GLsizei n, GLuint* arrays) {
    glGenVertexArrays(n, arrays);
}

static KHRONOS_APIENTRY void _glDeleteVertexArrays(GLsizei n, const GLuint* arrays) {
    glDeleteVertexArrays(n, arrays);
}

static KHRONOS_APIENTRY void _glBindVertexArray(GLuint array) {
    glBindVertexArray(array);
}

static KHRONOS_APIENTRY void _glGenBuffers(GLsizei n, GLuint* buffers) {
    glGenBuffers(n, buffers);
}

static KHRONOS_APIENTRY void _glDeleteBuffers(GLsizei n, const GLuint* buffers) {
    glDeleteBuffers(n, buffers);
}

static KHRONOS_APIENTRY void _glBindBuffer(GLenum target, GLuint buffer) {
    glBindBuffer(enumconv(target), buffer);
}

static KHRONOS_APIENTRY void _glBufferData(GLenum target, GLsizei size, const GLvoid* data, GLenum usage) {
    glBufferData(enumconv(target), size, data, enumconv(usage));
}

static KHRONOS_APIENTRY void _glBufferSubData(GLenum target, GLsizei offset, GLsizei size, const GLvoid* data) {
    glBufferSubData(enumconv(target), offset, size, data);
}

static KHRONOS_APIENTRY void* _glMapBuffer(GLenum target, GLenum access) {
    return glMapBuffer(enumconv(target), enumconv(access));
}

static KHRONOS_APIENTRY void _glVertexAttribDivisor(GLuint index, GLuint divisor) {
    glVertexAttribDivisor(index, divisor);
}

static KHRONOS_APIENTRY void _glEnableVertexAttribArray(GLuint index) {
    glEnableVertexAttribArray(index);
}

static KHRONOS_APIENTRY void _glDisableVertexAttribArray(GLuint index) {
    glDisableVertexAttribArray(index);
}

static KHRONOS_APIENTRY void _glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    glDrawArrays(enumconv(mode), first, count);
}

static KHRONOS_APIENTRY void _glMultiDrawArrays(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount) {
    glMultiDrawArrays(enumconv(mode), first, count, drawcount);
}

static KHRONOS_APIENTRY void _glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    glDrawElements(enumconv(mode), count, enumconv(type), indices);
}

static KHRONOS_APIENTRY void _glMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, const GLvoid* const* indices, GLsizei drawcount) {
    glMultiDrawElements(enumconv(mode), count, enumconv(type), indices, drawcount);
}

static KHRONOS_APIENTRY void _glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount) {
    glDrawArraysInstanced(enumconv(mode), first, count, primcount);
}

static KHRONOS_APIENTRY void _glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei primcount, GLuint baseinstance) {
    glDrawArraysInstancedBaseInstance(enumconv(mode), first, count, primcount, baseinstance);
}

static KHRONOS_APIENTRY void _glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices, GLsizei primcount) {
    glDrawElementsInstanced(enumconv(mode), count, enumconv(type), indices, primcount);
}

static KHRONOS_APIENTRY void _glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices, GLsizei primcount, GLuint baseinstance) {
    glDrawElementsInstancedBaseInstance(enumconv(mode), count, enumconv(type), indices, primcount, baseinstance);
}

static KHRONOS_APIENTRY void _glNamedBufferData(GLuint buffer, GLsizei size, const GLvoid* data, GLenum usage) {
    glNamedBufferData(buffer, size, data, enumconv(usage));
}

static KHRONOS_APIENTRY void _glNamedBufferSubData(GLuint buffer, GLsizei offset, GLsizei size, const GLvoid* data) {
    glNamedBufferSubData(buffer, offset, size, data);
}

static KHRONOS_APIENTRY void* _glMapNamedBuffer(GLuint buffer, GLenum access) {
    return glMapNamedBuffer(buffer, enumconv(access));
}

static KHRONOS_APIENTRY void _glCreateTextures(GLenum target, GLsizei n, GLuint* textures) {
    glCreateTextures(enumconv(target), n, textures);
}

static KHRONOS_APIENTRY void _glDeleteProgram(GLuint program) {
    glDeleteProgram(program);
}

static KHRONOS_APIENTRY void _glUseProgram(GLuint program) {
    glUseProgram(program);
}

#define BIND_PROC(x) {"gl" #x, (void *)_gl##x}

void *mkxp_pgl_get_proc_address(const char *proc_name) {
    static std::unordered_map<std::string, void *> proc_map = {
	BIND_PROC(Viewport),
	BIND_PROC(GetString),
	BIND_PROC(GetBooleanv),
	BIND_PROC(GetFloatv),
	BIND_PROC(GetIntegerv),
	BIND_PROC(IsEnabled),
	BIND_PROC(IsProgram),
	BIND_PROC(ClearColor),
	BIND_PROC(ClearDepth),
	BIND_PROC(DepthFunc),
	BIND_PROC(DepthRange),
	BIND_PROC(DepthMask),
	BIND_PROC(BlendFunc),
	BIND_PROC(BlendEquation),
	BIND_PROC(BlendFuncSeparate),
	BIND_PROC(BlendEquationSeparate),
	BIND_PROC(BlendColor),
	BIND_PROC(Clear),
	BIND_PROC(ProvokingVertex),
	BIND_PROC(Enable),
	BIND_PROC(Disable),
	BIND_PROC(CullFace),
	BIND_PROC(FrontFace),
	BIND_PROC(PolygonMode),
	BIND_PROC(PointSize),
	BIND_PROC(PointParameteri),
	BIND_PROC(LineWidth),
	BIND_PROC(LogicOp),
	BIND_PROC(PolygonOffset),
	BIND_PROC(Scissor),
	BIND_PROC(StencilFunc),
	BIND_PROC(StencilFuncSeparate),
	BIND_PROC(StencilOp),
	BIND_PROC(StencilOpSeparate),
	BIND_PROC(ClearStencil),
	BIND_PROC(StencilMask),
	BIND_PROC(StencilMaskSeparate),
	BIND_PROC(GenTextures),
	BIND_PROC(DeleteTextures),
	BIND_PROC(BindTexture),
	BIND_PROC(TexParameteri),
	BIND_PROC(TexParameterfv),
	BIND_PROC(TextureParameteri),
	BIND_PROC(PixelStorei),
	BIND_PROC(TexImage1D),
	BIND_PROC(TexImage2D),
	BIND_PROC(TexImage3D),
	BIND_PROC(TexSubImage1D),
	BIND_PROC(TexSubImage2D),
	BIND_PROC(TexSubImage3D),
	BIND_PROC(GenVertexArrays),
	BIND_PROC(DeleteVertexArrays),
	BIND_PROC(BindVertexArray),
	BIND_PROC(GenBuffers),
	BIND_PROC(DeleteBuffers),
	BIND_PROC(BindBuffer),
	BIND_PROC(BufferData),
	BIND_PROC(BufferSubData),
	BIND_PROC(MapBuffer),
	BIND_PROC(VertexAttribDivisor),
	BIND_PROC(EnableVertexAttribArray),
	BIND_PROC(DisableVertexAttribArray),
	BIND_PROC(DrawArrays),
	BIND_PROC(MultiDrawArrays),
	BIND_PROC(DrawElements),
	BIND_PROC(MultiDrawElements),
	BIND_PROC(DrawArraysInstanced),
	BIND_PROC(DrawArraysInstancedBaseInstance),
	BIND_PROC(DrawElementsInstanced),
	BIND_PROC(DrawElementsInstancedBaseInstance),
	BIND_PROC(NamedBufferData),
	BIND_PROC(NamedBufferSubData),
	BIND_PROC(MapNamedBuffer),
	BIND_PROC(CreateTextures),
	BIND_PROC(DeleteProgram),
	BIND_PROC(UseProgram),
    };
    const auto it = proc_map.find(proc_name);
    return it == proc_map.end() ? NULL : it->second;
}
