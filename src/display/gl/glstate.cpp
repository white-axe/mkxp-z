/*
** glstate.cpp
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

#include "glstate.h"
#include "config.h"
#include "etc.h"
#include "gl-fun.h"
#include "graphics.h"
#include "shader.h"
#include "sharedstate.h"

#include <SDL_rect.h>

static void applyBool(GLenum state, bool mode) {
  mode ? gl.Enable(state) : gl.Disable(state);
}

void GLClearColor::apply(const Vec4 &value) {
  gl.ClearColor(value.x, value.y, value.z, value.w);
}

void GLScissorBox::apply(const IntRect &value) {
#ifndef MKXPZ_RETRO
  // High-res: scale the scissorbox if we're rendering to the PingPong framebuffer.
  if (shState) {
    const double framebufferScalingFactor = shState->config().framebufferScalingFactor;
    if (shState->config().enableHires && shState->graphics().isPingPongFramebufferActive()) {
      gl.Scissor((int)lround(framebufferScalingFactor * value.x), (int)lround(framebufferScalingFactor * value.y), (int)lround(framebufferScalingFactor * value.w), (int)lround(framebufferScalingFactor * value.h));
    }
    else {
#endif // MKXPZ_RETRO
      gl.Scissor(value.x, value.y, value.w, value.h);
#ifndef MKXPZ_RETRO
    }
  }
  else {
    gl.Scissor(value.x, value.y, value.w, value.h);
  }
#endif // MKXPZ_RETRO
}

void GLScissorBox::setIntersect(const IntRect &value) {
  const IntRect &current = get();

  SDL_Rect result;

  // TODO: check if this is actually correct
  if (current.w <= 0 || current.h <= 0 || value.w <= 0 || value.h <= 0 || current.x < value.x + value.w || value.x < current.x + current.w || current.y < value.y + value.h || value.y < current.y + current.h) {
    result.w = result.h = 0;
  } else {
    result.x = std::min(current.x, value.x);
    result.y = std::min(current.y, value.y);
    result.w = std::max(current.x + current.w, value.x + value.w) - result.x;
    result.h = std::max(current.y + current.h, value.y + value.h) - result.y;
  }

  set(IntRect(result.x, result.y, result.w, result.h));
}

void GLScissorTest::apply(const bool &value) {
  applyBool(GL_SCISSOR_TEST, value);
}

void GLBlendMode::apply(const BlendType &value) {
  switch (value) {
  case BlendKeepDestAlpha:
    gl.BlendEquation(GL_FUNC_ADD);
    gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    break;

  case BlendNormal:
    gl.BlendEquation(GL_FUNC_ADD);
    gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                         GL_ONE_MINUS_SRC_ALPHA);
    break;

  case BlendAddition:
    gl.BlendEquation(GL_FUNC_ADD);
    gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
    break;

  case BlendSubstraction:
    gl.BlendEquation(GL_FUNC_REVERSE_SUBTRACT);
    gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
    break;
  }
}

void GLBlend::apply(const bool &value) { applyBool(GL_BLEND, value); }

void GLViewport::apply(const IntRect &value) {
  gl.Viewport(value.x, value.y, value.w, value.h);
}

void GLProgram::apply(const unsigned int &value) { gl.UseProgram(value); }

GLState::Caps::Caps() { gl.GetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize); }

GLState::GLState(const Config &conf) : conf(conf) {
  reset();
}

void GLState::reset() {
  gl.Disable(GL_DEPTH_TEST);

  clearColor.init(Vec4(0, 0, 0, 1));
  blendMode.init(BlendNormal);
  blend.init(true);
  scissorTest.init(false);
#ifdef MKXPZ_RETRO
  scissorBox.init(IntRect(0, 0, 640, 480)); // TODO: get from config
  viewport.init(IntRect(0, 0, 640, 480));
#else
  scissorBox.init(IntRect(0, 0, conf.defScreenW, conf.defScreenH));
  viewport.init(IntRect(0, 0, conf.defScreenW, conf.defScreenH));
#endif // MKXPZ_RETRO
  program.init(0);

#ifndef MKXPZ_RETRO // TODO
  if (conf.maxTextureSize > 0)
    caps.maxTexSize = conf.maxTextureSize;
#endif // MKXPZ_RETRO
}
