/*
** plane.cpp
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

#include "plane.h"

#include <cmath>

#include "sharedstate.h"
#include "bitmap.h"
#include "etc.h"
#include "util.h"

#include "gl-util.h"
#include "quad.h"
#include "quadarray.h"
#include "transform.h"
#include "etc-internal.h"
#include "shader.h"
#include "glstate.h"

#include "sigslot/signal.hpp"

#ifdef MKXPZ_RETRO
#  include "sandbox-serial-util.h"
#endif // MKXPZ_RETRO

#define GUARD_V(value, expression) do { expression; if (exception.is_error()) return value; } while (0)
#define GUARD(expression) GUARD_V(, expression)

static float fwrap(float value, float range)
{
	float res = std::fmod(value, range);
	return res < 0 ? res + range : res;
}

struct PlanePrivate
{
	Bitmap *bitmap;

	sigslot::connection bitmapDispCon;

	NormValue opacity;
	BlendType blendType;
	Color *color;
	Tone *tone;

	int ox, oy;
	float zoomX, zoomY;

	Scene::Geometry sceneGeo;

	bool quadSourceDirty;

	SimpleQuadArray qArray;

	EtcTemps tmp;

	sigslot::connection prepareCon;

	PlanePrivate()
	    : bitmap(0),
	      opacity(255),
	      blendType(BlendNormal),
	      color(&tmp.color),
	      tone(&tmp.tone),
	      ox(0), oy(0),
	      zoomX(1), zoomY(1),
	      quadSourceDirty(false)
	{
		prepareCon = shState->prepareDraw.connect
		        (&PlanePrivate::prepare, this);

		qArray.resize(1);
	}

	~PlanePrivate()
	{
		prepareCon.disconnect();
		
		bitmapDisposal();
	}

	void bitmapDisposal()
	{
		bitmap = 0;
		bitmapDispCon.disconnect();
	}

	void updateQuadSource()
	{
		if (gl.npot_repeat)
		{
			FloatRect srcRect;
			srcRect.x = (sceneGeo.orig.x + ox) / zoomX;
			srcRect.y = (sceneGeo.orig.y + oy) / zoomY;
			srcRect.w = sceneGeo.rect.w / zoomX;
			srcRect.h = sceneGeo.rect.h / zoomY;

			Quad::setTexRect(&qArray.vertices[0], srcRect);
			qArray.commit();

			return;
		}

		if (nullOrDisposed(bitmap))
			return;

		/* Scaled (zoomed) bitmap dimensions */
		float sw = bitmap->width()  * zoomX;
		float sh = bitmap->height() * zoomY;

		/* Plane offset wrapped by scaled bitmap dims */
		float wox = fwrap(ox, sw);
		float woy = fwrap(oy, sh);

		/* Viewport dimensions */
		int vpw = sceneGeo.rect.w;
		int vph = sceneGeo.rect.h;

		/* Amount the scaled bitmap is tiled (repeated) */
		size_t tilesX = ceil((vpw - sw + wox) / sw) + 1;
		size_t tilesY = ceil((vph - sh + woy) / sh) + 1;

		FloatRect tex = bitmap->rect();

		qArray.resize(tilesX * tilesY);

		for (size_t y = 0; y < tilesY; ++y)
			for (size_t x = 0; x < tilesX; ++x)
			{
				SVertex *vert = &qArray.vertices[(y*tilesX + x) * 4];
				FloatRect pos(x*sw - wox, y*sh - woy, sw, sh);

				Quad::setTexPosRect(vert, tex, pos);
			}

		qArray.commit();
	}

	void prepare()
	{
		if (nullOrDisposed(bitmap))
			return;
		
		if (quadSourceDirty)
		{
			updateQuadSource();
			quadSourceDirty = false;
		}
	}
};

static void disposePtr(void *ptr)
{
	((Plane *)ptr)->dispose();
}

Plane::Plane(Viewport *viewport)
    : ViewportElement(disposePtr, viewport)
{
	p = new PlanePrivate();

	onGeometryChange(scene->getGeometry());
}

DEF_ATTR_RD_SIMPLE(Plane, Bitmap,    Bitmap*, p->bitmap)
DEF_ATTR_RD_SIMPLE(Plane, OX,        int,     p->ox)
DEF_ATTR_RD_SIMPLE(Plane, OY,        int,     p->oy)
DEF_ATTR_RD_SIMPLE(Plane, ZoomX,     float,   p->zoomX)
DEF_ATTR_RD_SIMPLE(Plane, ZoomY,     float,   p->zoomY)
DEF_ATTR_RD_SIMPLE(Plane, BlendType, int,     p->blendType)

DEF_ATTR_SIMPLE(Plane, Opacity,   int,     p->opacity)
DEF_ATTR_SIMPLE(Plane, Color,     Color&, *p->color)
DEF_ATTR_SIMPLE(Plane, Tone,      Tone&,  *p->tone)

Plane::~Plane()
{
	dispose();
}

void Plane::setBitmap(Exception &exception, Bitmap *value)
{
	GUARD(guardDisposed(exception));

	p->bitmap = value;

	p->bitmapDispCon.disconnect();

	if (nullOrDisposed(value))
	{
		p->bitmap = 0;
		return;
	}

	p->bitmapDispCon = value->wasDisposed.connect(&PlanePrivate::bitmapDisposal, p);

	GUARD(value->ensureNonMega(exception));
}

void Plane::setOX(Exception &exception, int value)
{
	GUARD(guardDisposed(exception));

	if (p->ox == value)
	        return;

	p->ox = value;
	p->quadSourceDirty = true;
}

void Plane::setOY(Exception &exception, int value)
{
	GUARD(guardDisposed(exception));

	if (p->oy == value)
	        return;

	p->oy = value;
	p->quadSourceDirty = true;
}

void Plane::setZoomX(Exception &exception, float value)
{
	GUARD(guardDisposed(exception));

	if (p->zoomX == value)
	        return;

	p->zoomX = value;
	p->quadSourceDirty = true;
}

void Plane::setZoomY(Exception &exception, float value)
{
	GUARD(guardDisposed(exception));

	if (p->zoomY == value)
	        return;

	p->zoomY = value;
	p->quadSourceDirty = true;
}

void Plane::setBlendType(Exception &exception, int value)
{
	GUARD(guardDisposed(exception));

	switch (value)
	{
	default :
	case BlendNormal :
		p->blendType = BlendNormal;
		return;
	case BlendAddition :
		p->blendType = BlendAddition;
		return;
	case BlendSubstraction :
		p->blendType = BlendSubstraction;
		return;
	}
}

void Plane::initDynAttribs()
{
	p->color = new Color;
	p->tone = new Tone;
}

void Plane::draw(Exception &exception)
{
	if (nullOrDisposed(p->bitmap))
		return;

	if (!p->opacity)
		return;

	ShaderBase *base;

	if (p->color->hasEffect() || p->tone->hasEffect() || p->opacity != 255)
	{
		PlaneShader &shader = shState->shaders().plane;

		shader.bind();
		shader.applyViewportProj();
		shader.setTone(p->tone->norm);
		shader.setColor(p->color->norm);
		shader.setFlash(Vec4());
		shader.setOpacity(p->opacity.norm);

		base = &shader;
	}
	else
	{
		SimpleShader &shader = shState->shaders().simple;

		shader.bind();
		shader.applyViewportProj();
		shader.setTranslation(Vec2i());

		base = &shader;
	}

	glState.blendMode.pushSet(p->blendType);

	p->bitmap->bindTex(*base);

	if (gl.npot_repeat)
		TEX::setRepeat(true);

	p->qArray.draw();

	if (gl.npot_repeat)
		TEX::setRepeat(false);

	glState.blendMode.pop();
}

void Plane::onGeometryChange(const Scene::Geometry &geo)
{
	if (gl.npot_repeat)
		Quad::setPosRect(&p->qArray.vertices[0], FloatRect(geo.rect));

	p->sceneGeo = geo;
	p->quadSourceDirty = true;
}

void Plane::releaseResources()
{
	unlink();

	delete p;
}

#ifdef MKXPZ_RETRO
bool Plane::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(p->opacity, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->blendType, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize((int32_t)p->ox, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize((int32_t)p->oy, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->zoomX, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->zoomY, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->sceneGeo, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->qArray, data, max_size)) return false;
	if (!sandbox_serialize_viewport_element(data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->bitmap, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->color, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->tone, data, max_size)) return false;

	return true;
}

bool Plane::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	if (!mkxp_sandbox::sandbox_deserialize(p->opacity, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->blendType, data, max_size)) return false;
	{
		int32_t value = (int32_t)p->ox;
		if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->ox, data, max_size)) return false;
		if ((int32_t)p->ox != value) {
			p->quadSourceDirty = true;
		}
	}
	{
		int32_t value = (int32_t)p->oy;
		if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->oy, data, max_size)) return false;
		if ((int32_t)p->oy != value) {
			p->quadSourceDirty = true;
		}
	}
	{
		int32_t value = (int32_t)p->zoomX;
		if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->zoomX, data, max_size)) return false;
		if ((int32_t)p->zoomX != value) {
			p->quadSourceDirty = true;
		}
	}
	{
		int32_t value = (int32_t)p->zoomY;
		if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->zoomY, data, max_size)) return false;
		if ((int32_t)p->zoomY != value) {
			p->quadSourceDirty = true;
		}
	}
	{
		Scene::Geometry old_geo = p->sceneGeo;
		if (!mkxp_sandbox::sandbox_deserialize(p->sceneGeo, data, max_size)) return false;
		if (p->sceneGeo != old_geo) {
			p->quadSourceDirty = true;
		}
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->qArray, data, max_size)) return false;
	if (!sandbox_deserialize_viewport_element(data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->bitmap, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->color, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->tone, data, max_size)) return false;

	return true;
}

void Plane::sandbox_deserialize_begin()
{
	sandbox_deserialize_begin_viewport_element();

	if (isDisposed()) return;

	p->bitmapDispCon.disconnect();
}

void Plane::sandbox_deserialize_end()
{
	sandbox_deserialize_end_viewport_element();

	if (isDisposed()) return;
	if (p->bitmap != nullptr) {
		p->bitmapDispCon = p->bitmap->wasDisposed.connect(&PlanePrivate::bitmapDisposal, p);
		if (p->bitmap->isDisposed()) {
			p->bitmapDisposal();
		}
	}
}
#endif // MKXPZ_RETRO
