/*
** shader.cpp
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

#include "shader.h"
#include "config.h"
#include "graphics.h"
#include "sharedstate.h"
#include "glstate.h"
#include "exception.h"

#include <assert.h>
#include <string.h>
#include <iostream>

#ifndef MKXPZ_BUILD_XCODE
#include "common.h.xxd"
#include "sprite.frag.xxd"
#include "hue.frag.xxd"
#include "trans.frag.xxd"
#include "transSimple.frag.xxd"
#include "bitmapBlit.frag.xxd"
#include "plane.frag.xxd"
#include "gray.frag.xxd"
#include "flatColor.frag.xxd"
#include "simple.frag.xxd"
#include "simpleColor.frag.xxd"
#include "simpleAlpha.frag.xxd"
#include "simpleAlphaUni.frag.xxd"
#include "tilemap.frag.xxd"
#include "flashMap.frag.xxd"
#include "bicubic.frag.xxd"
#include "lanczos3.frag.xxd"
#ifdef MKXPZ_SSL
#include "xbrz.frag.xxd"
#endif
#include "minimal.vert.xxd"
#include "simple.vert.xxd"
#include "simpleColor.vert.xxd"
#include "sprite.vert.xxd"
#include "tilemap.vert.xxd"
#include "blur.frag.xxd"
#include "simpleMatrix.vert.xxd"
#include "blurH.vert.xxd"
#include "blurV.vert.xxd"
#include "tilemapvx.vert.xxd"
#endif

#ifdef MKXPZ_BUILD_XCODE
#include "filesystem/filesystem.h"
#define INIT_SHADER(vert, frag, name) \
{ \
    std::string v = mkxp_fs::contentsOfAssetAsString("Shaders/" #vert, "vert"); \
    std::string f = mkxp_fs::contentsOfAssetAsString("Shaders/" #frag, "frag"); \
    Shader::init((const unsigned char*)v.c_str(), v.length(), (const unsigned char*)f.c_str(), f.length(), #vert, #frag, #name); \
}
#else
#define INIT_SHADER(vert, frag, name) \
{ \
	Shader::init(mkxp_shader_##vert##_vert, sizeof mkxp_shader_##vert##_vert, mkxp_shader_##frag##_frag, sizeof mkxp_shader_##frag##_frag, \
	#vert, #frag, #name); \
}
#endif

#define GET_U(name) u_##name = gl.GetUniformLocation(program, #name)

#ifdef MKXPZ_BUILD_XCODE
    std::string Shader::shaderCommon = "";
#endif

static void printShaderLog(GLuint shader)
{
	GLint logLength;
	gl.GetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

	std::string log(logLength, '\0');
	gl.GetShaderInfoLog(shader, log.size(), 0, &log[0]);

	std::clog << "Shader log:\n" << log;
}

static void printProgramLog(GLuint program)
{
	GLint logLength;
	gl.GetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

	std::string log(logLength, '\0');
	gl.GetProgramInfoLog(program, log.size(), 0, &log[0]);

	std::clog << "Program log:\n" << log;
}

Shader::Shader()
{
#ifndef MKXPZ_PGL
#ifdef MKXPZ_BUILD_XCODE
    if (Shader::shaderCommon.empty())
        Shader::shaderCommon = mkxp_fs::contentsOfAssetAsString("Shaders/common", "h");
#endif
	vertShader = gl.CreateShader(GL_VERTEX_SHADER);
	fragShader = gl.CreateShader(GL_FRAGMENT_SHADER);

	program = gl.CreateProgram();
#endif
}

Shader::~Shader()
{
	gl.DeleteProgram(program);
#ifndef MKXPZ_PGL
	gl.DeleteShader(vertShader);
	gl.DeleteShader(fragShader);
#endif
}

void Shader::bind()
{
	glState.program.set(program);
}

void Shader::unbind()
{
	gl.ActiveTexture(GL_TEXTURE0);
	glState.program.set(0);
}

#ifdef MKXPZ_BUILD_XCODE
std::string &Shader::commonHeader() {
    return Shader::shaderCommon;
}
#endif

static void setupShaderSource(GLuint shader, GLenum type,
                              const unsigned char *body, int bodySize)
{
	static const char glesDefine[] = "#define GLSLES\n";
	static const char fragDefine[] = "#define FRAGMENT_SHADER\n";

	const GLchar *shaderSrc[4];
	GLint shaderSrcSize[4];
	size_t i = 0;

	if (gl.glsles)
	{
		shaderSrc[i] = glesDefine;
		shaderSrcSize[i] = sizeof(glesDefine)-1;
		++i;
	}

	if (type == GL_FRAGMENT_SHADER)
	{
		shaderSrc[i] = fragDefine;
		shaderSrcSize[i] = sizeof(fragDefine)-1;
		++i;
	}

#ifndef MKXPZ_BUILD_XCODE
	shaderSrc[i] = (const GLchar*) mkxp_shader_common_h;
	shaderSrcSize[i] = sizeof mkxp_shader_common_h;
#else
    shaderSrc[i] = (const GLchar*) Shader::commonHeader().c_str();
    shaderSrcSize[i] = Shader::commonHeader().length();
#endif
	++i;

	shaderSrc[i] = (const GLchar*) body;
	shaderSrcSize[i] = bodySize;
	++i;

	gl.ShaderSource(shader, i, shaderSrc, shaderSrcSize);
}

void Shader::init(const unsigned char *vert, int vertSize,
                  const unsigned char *frag, int fragSize,
                  const char *vertName, const char *fragName,
                  const char *programName)
{
	GLint success;

	/* Compile vertex shader */
	setupShaderSource(vertShader, GL_VERTEX_SHADER, vert, vertSize);
	gl.CompileShader(vertShader);

	gl.GetShaderiv(vertShader, GL_COMPILE_STATUS, &success);

	if (!success)
	{
		printShaderLog(vertShader);
		throw Exception(Exception::MKXPError,
	                    "GLSL: An error occured while compiling vertex shader '%s' in program '%s'",
	                    vertName, programName);
	}

	/* Compile fragment shader */
	setupShaderSource(fragShader, GL_FRAGMENT_SHADER, frag, fragSize);
	gl.CompileShader(fragShader);

	gl.GetShaderiv(fragShader, GL_COMPILE_STATUS, &success);

	if (!success)
	{
		printShaderLog(fragShader);
		throw Exception(Exception::MKXPError,
	                    "GLSL: An error occured while compiling fragment shader '%s' in program '%s'",
	                    fragName, programName);
	}

	/* Link shader program */
	gl.AttachShader(program, vertShader);
	gl.AttachShader(program, fragShader);

	gl.BindAttribLocation(program, Position, "position");
	gl.BindAttribLocation(program, TexCoord, "texCoord");
	gl.BindAttribLocation(program, Color, "color");

	gl.LinkProgram(program);

	gl.GetProgramiv(program, GL_LINK_STATUS, &success);

	if (!success)
	{
		printProgramLog(program);
		throw Exception(Exception::MKXPError,
	                    "GLSL: An error occured while linking program '%s' (vertex '%s', fragment '%s')",
	                    programName, vertName, fragName);
	}
}

void Shader::initFromFile(const char *_vertFile, const char *_fragFile,
                          const char *programName)
{
	std::string vertContents, fragContents;
	readFile(_vertFile, vertContents);
	readFile(_fragFile, fragContents);

	init((const unsigned char*) vertContents.c_str(), vertContents.size(),
	     (const unsigned char*) fragContents.c_str(), fragContents.size(),
	     _vertFile, _fragFile, programName);
}

void Shader::setVec2Uniform(GLint location, const Vec2 &vec)
{
    gl.Uniform2f(location, vec.x, vec.y);
}

void Shader::setVec4Uniform(GLint location, const Vec4 &vec)
{
	gl.Uniform4f(location, vec.x, vec.y, vec.z, vec.w);
}

void Shader::setTexUniform(GLint location, unsigned unitIndex, TEX::ID texture)
{
	GLenum texUnit = GL_TEXTURE0 + unitIndex;

	gl.ActiveTexture(texUnit);
	gl.BindTexture(GL_TEXTURE_2D, texture.gl);
#ifndef MKXPZ_PGL
	gl.Uniform1i(location, unitIndex);
#endif
	gl.ActiveTexture(GL_TEXTURE0);
}

void ShaderBase::GLProjMat::apply(const Vec2i &value)
{
	/* glOrtho replacement */
	const float a = 2.f / value.x;
	const float b = 2.f / value.y;
	const float c = -2.f;

	GLfloat mat[16] =
	{
		 a,  0,  0,  0,
		 0,  b,  0,  0,
		 0,  0,  c,  0,
		-1, -1, -1,  1
	};

#ifdef MKXPZ_PGL
	if (pgl_mat != NULL) {
		std::memcpy(pgl_mat, mat, sizeof *pgl_mat);
	}
#else
	gl.UniformMatrix4fv(u_mat, 1, GL_FALSE, mat);
#endif
}

void ShaderBase::init()
{
	GET_U(texSizeInv);
	GET_U(translation);

	projMat.u_mat = gl.GetUniformLocation(program, "projMat");
}

void ShaderBase::applyViewportProj()
{
	// High-res: scale the matrix if we're rendering to the PingPong framebuffer.
	const IntRect &vp = glState.viewport.get();
#ifndef MKXPZ_RETRO
	if (shState->config().enableHires && shState->graphics().isPingPongFramebufferActive() && framebufferScalingAllowed()) {
		projMat.set(Vec2i(shState->graphics().width(), shState->graphics().height()));
	}
	else {
#endif // MKXPZ_RETRO
#ifdef MKXPZ_PGL
		projMat.pgl_mat = pglProjMat();
#endif
		projMat.set(Vec2i(vp.w, vp.h));
#ifndef MKXPZ_RETRO
	}
#endif // MKXPZ_RETRO
}

bool ShaderBase::framebufferScalingAllowed()
{
	return true;
}

void ShaderBase::setTexSize(const Vec2i &value)
{
#ifdef MKXPZ_PGL
	pgl_vec2 *pgl_vec = pglTexSizeInv();
	if (pgl_vec != NULL) {
		pgl_vec->x = 1.f / value.x;
		pgl_vec->y = 1.f / value.y;
	}
#else
	gl.Uniform2f(u_texSizeInv, 1.f / value.x, 1.f / value.y);
#endif
}

void ShaderBase::setTranslation(const Vec2i &value)
{
#ifdef MKXPZ_PGL
	pgl_vec2 *pgl_vec = pglTranslation();
	if (pgl_vec != NULL) {
		pgl_vec->x = value.x;
		pgl_vec->y = value.y;
	}
#else
	gl.Uniform2f(u_translation, value.x, value.y);
#endif
}

pgl_mat4 *ShaderBase::pglProjMat()
{
	return NULL;
}

pgl_vec2 *ShaderBase::pglTexSizeInv()
{
	return NULL;
}

pgl_vec2 *ShaderBase::pglTranslation()
{
	return NULL;
}


FlatColorShader::FlatColorShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpFlatColorVS, mkxpFlatColorFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
#else
	INIT_SHADER(minimal, flatColor, FlatColorShader);

	ShaderBase::init();

	GET_U(color);
#endif
}

void FlatColorShader::setColor(const Vec4 &value)
{
#ifdef MKXPZ_PGL
	uniforms.color.x = value.x;
	uniforms.color.y = value.y;
	uniforms.color.z = value.z;
	uniforms.color.w = value.w;
#else
	setVec4Uniform(u_color, value);
#endif
}


SimpleShader::SimpleShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpSimpleVS, mkxpSimpleFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.texture = 0;
#else
	INIT_SHADER(simple, simple, SimpleShader);

	ShaderBase::init();

	GET_U(texOffsetX);
#endif
}

void SimpleShader::setTexOffsetX(int value)
{
#ifdef MKXPZ_PGL
	uniforms.translation.x = value;
#else
	gl.Uniform1f(u_texOffsetX, value);
#endif
}


SimpleColorShader::SimpleColorShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpSimpleColorVS, mkxpSimpleColorFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
#else
	INIT_SHADER(simpleColor, simpleColor, SimpleColorShader);

	ShaderBase::init();
#endif
}


SimpleAlphaShader::SimpleAlphaShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpSimpleAlphaVS, mkxpSimpleAlphaFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.texture = 0;
#else
	INIT_SHADER(simpleColor, simpleAlpha, SimpleAlphaShader);

	ShaderBase::init();
#endif
}


SimpleSpriteShader::SimpleSpriteShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpSimpleColorVS, mkxpSimpleColorFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.texture = 0;
#else
	INIT_SHADER(sprite, simple, SimpleSpriteShader);

	ShaderBase::init();

	GET_U(spriteMat);
#endif
}

void SimpleSpriteShader::setSpriteMat(const float value[16])
{
#ifdef MKXPZ_PGL
	std::memcpy(&uniforms.spriteMat, value, sizeof uniforms.spriteMat);
#else
	gl.UniformMatrix4fv(u_spriteMat, 1, GL_FALSE, value);
#endif
}

BicubicSpriteShader::BicubicSpriteShader()
{
	INIT_SHADER(sprite, bicubic, BicubicSpriteShader);

	ShaderBase::init();

	GET_U(spriteMat);
	GET_U(sourceSize);
	GET_U(bc);
}

void BicubicSpriteShader::setSharpness(int sharpness)
{
	gl.Uniform2f(u_bc, 1.f - sharpness * 0.01f, sharpness * 0.005f);
}

Lanczos3SpriteShader::Lanczos3SpriteShader()
{
	INIT_SHADER(sprite, lanczos3, Lanczos3SpriteShader);

	ShaderBase::init();

	GET_U(spriteMat);
	GET_U(sourceSize);
}

void Lanczos3SpriteShader::setTexSize(const Vec2i &value)
{
	ShaderBase::setTexSize(value);
	gl.Uniform2f(u_sourceSize, (float)value.x, (float)value.y);
}

#ifdef MKXPZ_SSL
XbrzSpriteShader::XbrzSpriteShader()
{
	INIT_SHADER(sprite, xbrz, XbrzSpriteShader);

	ShaderBase::init();

	GET_U(spriteMat);
	GET_U(sourceSize);
	GET_U(targetScale);
}

void XbrzSpriteShader::setTargetScale(const Vec2 &value)
{
	gl.Uniform2f(u_targetScale, value.x, value.y);
}
#endif

AlphaSpriteShader::AlphaSpriteShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpAlphaSpriteVS, mkxpAlphaSpriteFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.texture = 0;
#else
	INIT_SHADER(sprite, simpleAlphaUni, AlphaSpriteShader);

	ShaderBase::init();

	GET_U(spriteMat);
	GET_U(alpha);
#endif
}

void AlphaSpriteShader::setSpriteMat(const float value[16])
{
#ifdef MKXPZ_PGL
	std::memcpy(&uniforms.spriteMat, value, sizeof uniforms.spriteMat);
#else
	gl.UniformMatrix4fv(u_spriteMat, 1, GL_FALSE, value);
#endif
}

void AlphaSpriteShader::setAlpha(float value)
{
#ifdef MKXPZ_PGL
	uniforms.alpha = value;
#else
	gl.Uniform1f(u_alpha, value);
#endif
}


TransShader::TransShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpTransVS, mkxpTransFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.currentScene = 1;
	uniforms.frozenScene = 2;
	uniforms.transMap = 3;
#else
	INIT_SHADER(simple, trans, TransShader);

	ShaderBase::init();

	GET_U(currentScene);
	GET_U(frozenScene);
	GET_U(transMap);
	GET_U(prog);
	GET_U(vague);
#endif
}

void TransShader::setCurrentScene(TEX::ID tex)
{
	setTexUniform(u_currentScene, 1, tex);
}

void TransShader::setFrozenScene(TEX::ID tex)
{
	setTexUniform(u_frozenScene, 2, tex);
}

void TransShader::setTransMap(TEX::ID tex)
{
	setTexUniform(u_transMap, 3, tex);
}

void TransShader::setProg(float value)
{
#ifdef MKXPZ_PGL
	uniforms.prog = value;
#else
	gl.Uniform1f(u_prog, value);
#endif
}

void TransShader::setVague(float value)
{
#ifdef MKXPZ_PGL
	uniforms.vague = value;
#else
	gl.Uniform1f(u_vague, value);
#endif
}


SimpleTransShader::SimpleTransShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpTransSimpleVS, mkxpTransSimpleFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.currentScene = 1;
	uniforms.frozenScene = 2;
#else
	INIT_SHADER(simple, transSimple, SimpleTransShader);

	ShaderBase::init();

	GET_U(currentScene);
	GET_U(frozenScene);
	GET_U(prog);
#endif
}

void SimpleTransShader::setCurrentScene(TEX::ID tex)
{
	setTexUniform(u_currentScene, 1, tex);
}

void SimpleTransShader::setFrozenScene(TEX::ID tex)
{
	setTexUniform(u_frozenScene, 2, tex);
}

void SimpleTransShader::setProg(float value)
{
#ifdef MKXPZ_PGL
	uniforms.prog = value;
#else
	gl.Uniform1f(u_prog, value);
#endif
}


SpriteShader::SpriteShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpSpriteVS, mkxpSpriteFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.texture = 0;
	uniforms.pattern = 1;
#else
	INIT_SHADER(sprite, sprite, SpriteShader);

	ShaderBase::init();

	GET_U(spriteMat);
	GET_U(tone);
	GET_U(color);
	GET_U(opacity);
	GET_U(bushDepth);
	GET_U(bushOpacity);
    GET_U(pattern);
    GET_U(patternBlendType);
    GET_U(patternTile);
    GET_U(renderPattern);
    GET_U(patternSizeInv);
    GET_U(patternOpacity);
    GET_U(patternScroll);
    GET_U(patternZoom);
    GET_U(invert);
#endif
}

void SpriteShader::setSpriteMat(const float value[16])
{
#ifdef MKXPZ_PGL
	std::memcpy(&uniforms.spriteMat, value, sizeof uniforms.spriteMat);
#else
	gl.UniformMatrix4fv(u_spriteMat, 1, GL_FALSE, value);
#endif
}

void SpriteShader::setTone(const Vec4 &tone)
{
#ifdef MKXPZ_PGL
	uniforms.tone.x = tone.x;
	uniforms.tone.y = tone.y;
	uniforms.tone.z = tone.z;
	uniforms.tone.w = tone.w;
#else
	setVec4Uniform(u_tone, tone);
#endif
}

void SpriteShader::setColor(const Vec4 &color)
{
#ifdef MKXPZ_PGL
	uniforms.color.x = color.x;
	uniforms.color.y = color.y;
	uniforms.color.z = color.z;
	uniforms.color.w = color.w;
#else
	setVec4Uniform(u_color, color);
#endif
}

void SpriteShader::setOpacity(float value)
{
#ifdef MKXPZ_PGL
	uniforms.opacity = value;
#else
	gl.Uniform1f(u_opacity, value);
#endif
}

void SpriteShader::setBushDepth(float value)
{
#ifdef MKXPZ_PGL
	uniforms.bushDepth = value;
#else
	gl.Uniform1f(u_bushDepth, value);
#endif
}

void SpriteShader::setBushOpacity(float value)
{
#ifdef MKXPZ_PGL
	uniforms.bushOpacity = value;
#else
	gl.Uniform1f(u_bushOpacity, value);
#endif
}

void SpriteShader::setPattern(const TEX::ID pattern, const Vec2 &dimensions)
{
    setTexUniform(u_pattern, 1, pattern);
#ifdef MKXPZ_PGL
	uniforms.patternSizeInv.x = 1.f / dimensions.x;
	uniforms.patternSizeInv.y = 1.f / dimensions.y;
#else
    gl.Uniform2f(u_patternSizeInv, 1.f / dimensions.x, 1.f / dimensions.y);
#endif
}

void SpriteShader::setPatternBlendType(int blendType)
{
#ifdef MKXPZ_PGL
	uniforms.patternBlendType = blendType;
#else
    gl.Uniform1i(u_patternBlendType, blendType);
#endif
}

void SpriteShader::setPatternTile(bool value)
{
#ifdef MKXPZ_PGL
	uniforms.patternTile = value;
#else
    gl.Uniform1i(u_patternTile, value);
#endif
}

void SpriteShader::setShouldRenderPattern(bool value)
{
#ifdef MKXPZ_PGL
	uniforms.renderPattern = value;
#else
    gl.Uniform1i(u_renderPattern, value);
#endif
}

void SpriteShader::setPatternOpacity(float value)
{
#ifdef MKXPZ_PGL
	uniforms.patternOpacity = value;
#else
    gl.Uniform1f(u_patternOpacity, value);
#endif
}

void SpriteShader::setPatternScroll(const Vec2 &scroll)
{
#ifdef MKXPZ_PGL
	uniforms.patternScroll.x = scroll.x;
	uniforms.patternScroll.y = scroll.y;
#else
    setVec2Uniform(u_patternScroll, scroll);
#endif
}

void SpriteShader::setPatternZoom(const Vec2 &zoom)
{
#ifdef MKXPZ_PGL
	uniforms.patternZoom.x = zoom.x;
	uniforms.patternZoom.y = zoom.y;
#else
    setVec2Uniform(u_patternZoom, zoom);
#endif
}

void SpriteShader::setInvert(bool value)
{
#ifdef MKXPZ_PGL
	uniforms.invert = value;
#else
    gl.Uniform1i(u_invert, value);
#endif
}


PlaneShader::PlaneShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpPlaneVS, mkxpPlaneFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.texture = 0;
#else
	INIT_SHADER(simple, plane, PlaneShader);

	ShaderBase::init();

	GET_U(tone);
	GET_U(color);
	GET_U(flash);
	GET_U(opacity);
#endif
}

void PlaneShader::setTone(const Vec4 &tone)
{
#ifdef MKXPZ_PGL
	uniforms.tone.x = tone.x;
	uniforms.tone.y = tone.y;
	uniforms.tone.z = tone.z;
	uniforms.tone.w = tone.w;
#else
	setVec4Uniform(u_tone, tone);
#endif
}

void PlaneShader::setColor(const Vec4 &color)
{
#ifdef MKXPZ_PGL
	uniforms.color.x = color.x;
	uniforms.color.y = color.y;
	uniforms.color.z = color.z;
	uniforms.color.w = color.w;
#else
	setVec4Uniform(u_color, color);
#endif
}

void PlaneShader::setFlash(const Vec4 &flash)
{
#ifdef MKXPZ_PGL
	uniforms.flash.x = flash.x;
	uniforms.flash.y = flash.y;
	uniforms.flash.z = flash.z;
	uniforms.flash.w = flash.w;
#else
	setVec4Uniform(u_flash, flash);
#endif
}

void PlaneShader::setOpacity(float value)
{
#ifdef MKXPZ_PGL
	uniforms.opacity = value;
#else
	gl.Uniform1f(u_opacity, value);
#endif
}


GrayShader::GrayShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpGrayVS, mkxpGrayFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.texture = 0;
#else
	INIT_SHADER(simple, gray, GrayShader);

	ShaderBase::init();

	GET_U(gray);
#endif
}

bool GrayShader::framebufferScalingAllowed()
{
	// This shader is used with input textures that have already had a
	// framebuffer scale applied. So we don't want to double-apply it.
	return false;
}

void GrayShader::setGray(float value)
{
#ifdef MKXPZ_PGL
	uniforms.gray = value;
#else
	gl.Uniform1f(u_gray, value);
#endif
}


TilemapShader::TilemapShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpTilemapVS, mkxpTilemapFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.texture = 0;
#else
	INIT_SHADER(tilemap, tilemap, TilemapShader);

	ShaderBase::init();

	GET_U(tone);
	GET_U(color);
	GET_U(opacity);

	GET_U(aniIndex);
	GET_U(atFrames);
#endif
}

void TilemapShader::setTone(const Vec4 &tone)
{
#ifdef MKXPZ_PGL
	uniforms.tone.x = tone.x;
	uniforms.tone.y = tone.y;
	uniforms.tone.z = tone.z;
	uniforms.tone.w = tone.w;
#else
	setVec4Uniform(u_tone, tone);
#endif
}

void TilemapShader::setColor(const Vec4 &color)
{
#ifdef MKXPZ_PGL
	uniforms.color.x = color.x;
	uniforms.color.y = color.y;
	uniforms.color.z = color.z;
	uniforms.color.w = color.w;
#else
	setVec4Uniform(u_color, color);
#endif
}

void TilemapShader::setOpacity(float value)
{
#ifdef MKXPZ_PGL
	uniforms.opacity = value;
#else
	gl.Uniform1f(u_opacity, value);
#endif
}

void TilemapShader::setAniIndex(int value)
{
#ifdef MKXPZ_PGL
	uniforms.aniIndex = value;
#else
	gl.Uniform1i(u_aniIndex, value);
#endif
}

void TilemapShader::setATFrames(int values[7])
{
#ifdef MKXPZ_PGL
	std::memcpy(&uniforms.atFrames, values, sizeof uniforms.atFrames);
#else
	gl.Uniform1iv(u_atFrames, 7, values);
#endif
}



FlashMapShader::FlashMapShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpFlashMapVS, mkxpFlashMapFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
#else
	INIT_SHADER(simpleColor, flashMap, FlashMapShader);

	ShaderBase::init();

	GET_U(alpha);
#endif
}

void FlashMapShader::setAlpha(float value)
{
#ifdef MKXPZ_PGL
	uniforms.alpha = value;
#else
	gl.Uniform1f(u_alpha, value);
#endif
}


HueShader::HueShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpHueVS, mkxpHueFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.texture = 0;
#else
	INIT_SHADER(simple, hue, HueShader);

	ShaderBase::init();

	GET_U(hueAdjust);
#endif
}

void HueShader::setHueAdjust(float value)
{
#ifdef MKXPZ_PGL
	uniforms.hueAdjust = value;
#else
	gl.Uniform1f(u_hueAdjust, value);
#endif
}


SimpleMatrixShader::SimpleMatrixShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpSimpleMatrixVS, mkxpSimpleMatrixFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.texture = 0;
#else
	INIT_SHADER(simpleMatrix, simpleAlpha, SimpleMatrixShader);

	ShaderBase::init();

	GET_U(matrix);
#endif
}

void SimpleMatrixShader::setMatrix(const float value[16])
{
#ifdef MKXPZ_PGL
	std::memcpy(&uniforms.matrix, value, sizeof uniforms.matrix);
#else
	gl.UniformMatrix4fv(u_matrix, 1, GL_FALSE, value);
#endif
}


BlurShader::HPass::HPass()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpBlurHVS, mkxpBlurHFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.texture = 0;
#else
	INIT_SHADER(blurH, blur, BlurShader::HPass);

	ShaderBase::init();
#endif
}

BlurShader::VPass::VPass()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpBlurVVS, mkxpBlurVFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.texture = 0;
#else
	INIT_SHADER(blurV, blur, BlurShader::VPass);

	ShaderBase::init();
#endif
}


TilemapVXShader::TilemapVXShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpTilemapVXVS, mkxpTilemapVXFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.texture = 0;
#else
	INIT_SHADER(tilemapvx, simple, TilemapVXShader);

	ShaderBase::init();

	GET_U(aniOffset);
#endif
}

void TilemapVXShader::setAniOffset(const Vec2 &value)
{
#ifdef MKXPZ_PGL
	uniforms.aniOffset.x = value.x;
	uniforms.aniOffset.y = value.y;
#else
	gl.Uniform2f(u_aniOffset, value.x, value.y);
#endif
}


BltShader::BltShader()
{
#ifdef MKXPZ_PGL
	program = pglCreateProgram(mkxpBitmapBlitVS, mkxpBitmapBlitFS, 4, (GLenum[]){PGL_SMOOTH4}, GL_FALSE);
	gl.UseProgram(program);
	pglSetUniform(&uniforms);
	uniforms.source = 0;
	uniforms.destination = 1;
#else
	INIT_SHADER(simple, bitmapBlit, BltShader);

	ShaderBase::init();

	GET_U(source);
	GET_U(destination);
	GET_U(subRect);
	GET_U(opacity);
#endif
}

void BltShader::setSource()
{
#ifdef MKXPZ_PGL
	uniforms.source = 0;
#else
	gl.Uniform1i(u_source, 0);
#endif
}

void BltShader::setDestination(const TEX::ID value)
{
	setTexUniform(u_destination, 1, value);
}

void BltShader::setSubRect(const FloatRect &value)
{
#ifdef MKXPZ_PGL
	uniforms.subRect.x = value.x;
	uniforms.subRect.y = value.y;
	uniforms.subRect.z = value.w;
	uniforms.subRect.w = value.h;
#else
	gl.Uniform4f(u_subRect, value.x, value.y, value.w, value.h);
#endif
}

void BltShader::setOpacity(float value)
{
#ifdef MKXPZ_PGL
	uniforms.opacity = value;
#else
	gl.Uniform1f(u_opacity, value);
#endif
}

BicubicShader::BicubicShader()
{
	INIT_SHADER(simple, bicubic, BicubicShader);

	ShaderBase::init();

	GET_U(texOffsetX);
	GET_U(sourceSize);
	GET_U(bc);
}

void BicubicShader::setSharpness(int sharpness)
{
	gl.Uniform2f(u_bc, 1.f - sharpness * 0.01f, sharpness * 0.005f);
}

Lanczos3Shader::Lanczos3Shader()
{
	INIT_SHADER(simple, lanczos3, Lanczos3Shader);

	ShaderBase::init();

	GET_U(texOffsetX);
	GET_U(sourceSize);
}

void Lanczos3Shader::setTexSize(const Vec2i &value)
{
	ShaderBase::setTexSize(value);
	gl.Uniform2f(u_sourceSize, (float)value.x, (float)value.y);
}

#ifdef MKXPZ_SSL
XbrzShader::XbrzShader()
{
	INIT_SHADER(simple, xbrz, XbrzShader);

	ShaderBase::init();

	GET_U(texOffsetX);
	GET_U(sourceSize);
	GET_U(targetScale);
}

void XbrzShader::setTargetScale(const Vec2 &value)
{
	gl.Uniform2f(u_targetScale, value.x, value.y);
}
#endif
