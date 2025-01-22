/*
** sharedstate.cpp
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

#include "sharedstate.h"

#include "scene.h"
#include "util.h"
#ifndef MKXPZ_RETRO
#include "filesystem.h"
#endif // MKXPZ_RETRO
#include "graphics.h"
#ifndef MKXPZ_RETRO
#include "input.h"
#include "audio.h"
#endif // MKXPZ_RETRO
#include "glstate.h"
#ifndef MKXPZ_RETRO
#include "shader.h"
#include "texpool.h"
#endif // MKXPZ_RETRO
#include "font.h"
#ifndef MKXPZ_RETRO
#include "eventthread.h"
#include "gl-util.h"
#include "global-ibo.h"
#include "quad.h"
#endif // MKXPZ_RETRO
#include "binding.h"
#include "exception.h"
#ifndef MKXPZ_RETRO
#include "sharedmidistate.h"
#endif // MKXPZ_RETRO

#include <unistd.h>
#include <stdio.h>
#include <string>
#include <chrono>

SharedState *SharedState::instance = 0;
int SharedState::rgssVersion = 0;
static GlobalIBO *_globalIBO = 0;

static const char *gameArchExt()
{
	if (rgssVer == 1)
		return ".rgssad";
	else if (rgssVer == 2)
		return ".rgss2a";
	else if (rgssVer == 3)
		return ".rgss3a";

	assert(!"unreachable");
	return 0;
}

struct SharedStatePrivate
{
	void *bindingData;
#ifndef MKXPZ_RETRO
	SDL_Window *sdlWindow;
#endif // MKXPZ_RETRO
	Scene *screen;

#ifndef MKXPZ_RETRO
	FileSystem fileSystem;

	EventThread &eThread;
	RGSSThreadData &rtData;
	Config &config;

	SharedMidiState midiState;

	Graphics graphics;
	Input input;
	Audio audio;

	GLState _glState;

	ShaderSet shaders;

	TexPool texPool;

	SharedFontState fontState;
	Font *defaultFont;

	TEX::ID globalTex;
#endif // MKXPZ_RETRO
	int globalTexW, globalTexH;
	bool globalTexDirty;

#ifndef MKXPZ_RETRO
	TEXFBO gpTexFBO;

	TEXFBO atlasTex;

	Quad gpQuad;
#endif // MKXPZ_RETRO

	unsigned int stampCounter;
    
    std::chrono::time_point<std::chrono::steady_clock> startupTime;

	SharedStatePrivate(RGSSThreadData *threadData)
	    : bindingData(0),
#ifndef MKXPZ_RETRO
	      sdlWindow(threadData->window),
	      fileSystem(threadData->argv0, threadData->config.allowSymlinks),
	      eThread(*threadData->ethread),
	      rtData(*threadData),
	      config(threadData->config),
	      midiState(threadData->config),
	      graphics(threadData),
	      input(*threadData),
	      audio(*threadData),
	      _glState(threadData->config),
	      fontState(threadData->config),
#endif // MKXPZ_RETRO
	      stampCounter(0)
	{}
	
	void init(RGSSThreadData *threadData)
	{
        
        startupTime = std::chrono::steady_clock::now();
        
#ifndef MKXPZ_RETRO
		/* Shaders have been compiled in ShaderSet's constructor */
		if (gl.ReleaseShaderCompiler)
			gl.ReleaseShaderCompiler();

		std::string archPath = config.execName + gameArchExt();

		for (size_t i = 0; i < config.patches.size(); ++i)
			fileSystem.addPath(config.patches[i].c_str());

		/* Check if a game archive exists */
		FILE *tmp = fopen(archPath.c_str(), "rb");
		if (tmp)
		{
			fileSystem.addPath(archPath.c_str());
			fclose(tmp);
		}

		fileSystem.addPath(".");

		for (size_t i = 0; i < config.rtps.size(); ++i)
			fileSystem.addPath(config.rtps[i].c_str());

		if (config.pathCache)
			fileSystem.createPathCache();

		fileSystem.initFontSets(fontState);

		globalTexW = 128;
		globalTexH = 64;

		globalTex = TEX::gen();
		TEX::bind(globalTex);
		TEX::setRepeat(false);
		TEX::setSmooth(false);
		TEX::allocEmpty(globalTexW, globalTexH);
		globalTexDirty = false;

		TEXFBO::init(gpTexFBO);
		/* Reuse starting values */
		TEXFBO::allocEmpty(gpTexFBO, globalTexW, globalTexH);
		TEXFBO::linkFBO(gpTexFBO);

		/* RGSS3 games will call setup_midi, so there's
		 * no need to do it on startup */
		if (rgssVer <= 2)
			midiState.initIfNeeded(threadData->config);
#endif // MKXPZ_RETRO
	}

	~SharedStatePrivate()
	{
#ifndef MKXPZ_RETRO
		TEX::del(globalTex);
		TEXFBO::fini(gpTexFBO);
		TEXFBO::fini(atlasTex);
#endif // MKXPZ_RETRO
	}
};

void SharedState::initInstance(RGSSThreadData *threadData)
{
	/* This section is tricky because of dependencies:
	 * SharedState depends on GlobalIBO existing,
	 * Font depends on SharedState existing */

#ifndef MKXPZ_RETRO
	rgssVersion = threadData->config.rgssVersion;
    
	_globalIBO = new GlobalIBO();
	_globalIBO->ensureSize(1);

	SharedState::instance = 0;
	Font *defaultFont = 0;
#endif // MKXPZ_RETRO

	try
	{
		SharedState::instance = new SharedState(threadData);
#ifndef MKXPZ_RETRO
		Font::initDefaults(instance->p->fontState);
		defaultFont = new Font();
#endif // MKXPZ_RETRO
	}
	catch (const Exception &exc)
	{
#ifndef MKXPZ_RETRO
		delete _globalIBO;
#endif // MKXPZ_RETRO
		delete SharedState::instance;
#ifndef MKXPZ_RETRO
		delete defaultFont;
#endif // MKXPZ_RETRO

		throw exc;
	}

#ifndef MKXPZ_RETRO
	SharedState::instance->p->defaultFont = defaultFont;
#endif // MKXPZ_RETRO
}

void SharedState::finiInstance()
{
#ifndef MKXPZ_RETRO
	delete SharedState::instance->p->defaultFont;
#endif // MKXPZ_RETRO

	delete SharedState::instance;

#ifndef MKXPZ_RETRO
	delete _globalIBO;
#endif // MKXPZ_RETRO
}

void SharedState::setScreen(Scene &screen)
{
	p->screen = &screen;
}

#define GSATT(type, lower) \
	type SharedState :: lower() const \
	{ \
		return p->lower; \
	}

GSATT(void*, bindingData)
#ifndef MKXPZ_RETRO
GSATT(SDL_Window*, sdlWindow)
#endif // MKXPZ_RETRO
GSATT(Scene*, screen)
#ifndef MKXPZ_RETRO
GSATT(FileSystem&, fileSystem)
GSATT(EventThread&, eThread)
GSATT(RGSSThreadData&, rtData)
GSATT(Config&, config)
GSATT(Graphics&, graphics)
GSATT(Input&, input)
GSATT(Audio&, audio)
GSATT(GLState&, _glState)
GSATT(ShaderSet&, shaders)
GSATT(TexPool&, texPool)
GSATT(Quad&, gpQuad)
GSATT(SharedFontState&, fontState)
GSATT(SharedMidiState&, midiState)
#endif // MKXPZ_RETRO

void SharedState::setBindingData(void *data)
{
	p->bindingData = data;
}

void SharedState::ensureQuadIBO(size_t minSize)
{
#ifndef MKXPZ_RETRO
	_globalIBO->ensureSize(minSize);
#endif // MKXPZ_RETRO
}

GlobalIBO &SharedState::globalIBO()
{
	return *_globalIBO;
}

void SharedState::bindTex()
{
#ifndef MKXPZ_RETRO
	TEX::bind(p->globalTex);

	if (p->globalTexDirty)
	{
		TEX::allocEmpty(p->globalTexW, p->globalTexH);
		p->globalTexDirty = false;
	}
#endif // MKXPZ_RETRO
}

void SharedState::ensureTexSize(int minW, int minH, Vec2i &currentSizeOut)
{
	if (minW > p->globalTexW)
	{
		p->globalTexDirty = true;
		p->globalTexW = findNextPow2(minW);
	}

	if (minH > p->globalTexH)
	{
		p->globalTexDirty = true;
		p->globalTexH = findNextPow2(minH);
	}

	currentSizeOut = Vec2i(p->globalTexW, p->globalTexH);
}

#ifndef MKXPZ_RETRO
TEXFBO &SharedState::gpTexFBO(int minW, int minH)
{
	bool needResize = false;

	if (minW > p->gpTexFBO.width)
	{
		p->gpTexFBO.width = findNextPow2(minW);
		needResize = true;
	}

	if (minH > p->gpTexFBO.height)
	{
		p->gpTexFBO.height = findNextPow2(minH);
		needResize = true;
	}

	if (needResize)
	{
		TEX::bind(p->gpTexFBO.tex);
		TEX::allocEmpty(p->gpTexFBO.width, p->gpTexFBO.height);
	}

	return p->gpTexFBO;
}

void SharedState::requestAtlasTex(int w, int h, TEXFBO &out)
{
	TEXFBO tex;

	if (w == p->atlasTex.width && h == p->atlasTex.height)
	{
		tex = p->atlasTex;
		p->atlasTex = TEXFBO();
	}
	else
	{
		TEXFBO::init(tex);
		TEXFBO::allocEmpty(tex, w, h);
		TEXFBO::linkFBO(tex);
	}

	out = tex;
}

void SharedState::releaseAtlasTex(TEXFBO &tex)
{
	/* No point in caching an invalid object */
	if (tex.tex == TEX::ID(0))
		return;

	TEXFBO::fini(p->atlasTex);

	p->atlasTex = tex;
}
#endif // MKXPZ_RETRO

void SharedState::checkShutdown()
{
#ifndef MKXPZ_RETRO
	if (!p->rtData.rqTerm)
		return;

	p->rtData.rqTermAck.set();
	p->texPool.disable();
	scriptBinding->terminate();
#endif // MKXPZ_RETRO
}

void SharedState::checkReset()
{
#ifndef MKXPZ_RETRO
	if (!p->rtData.rqReset)
		return;

	p->rtData.rqReset.clear();
	scriptBinding->reset();
#endif // MKXPZ_RETRO
}

#ifndef MKXPZ_RETRO
Font &SharedState::defaultFont() const
{
	return *p->defaultFont;
}
#endif // MKXPZ_RETRO

double SharedState::runTime() {
    if (!p) return 0;
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - p->startupTime).count() / 1000.0 / 1000.0;
}

unsigned int SharedState::genTimeStamp()
{
	return p->stampCounter++;
}

SharedState::SharedState(RGSSThreadData *threadData)
{
	p = new SharedStatePrivate(threadData);
	SharedState::instance = this;
	try
	{
		p->init(threadData);
#ifdef MKXPZ_RETRO
		p->screen = new Scene(); // TODO: implement
#else
		p->screen = p->graphics.getScreen();
#endif // MKXPZ_RETRO
	}
	catch (const Exception &exc)
	{
#ifndef MKXPZ_RETRO
		// If the "error" was the user quitting the game before the path cache finished building,
		// then just return
		if (rtData().rqTerm)
			return;
#endif // MKXPZ_RETRO
		
		delete p;
		SharedState::instance = 0;
		
		throw exc;
	}
}

SharedState::~SharedState()
{
	delete p;
}
