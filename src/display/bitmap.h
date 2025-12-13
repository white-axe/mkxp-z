/*
** bitmap.h
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

#ifndef BITMAP_H
#define BITMAP_H

#ifdef MKXPZ_RETRO
#  include <ft2build.h>
#  include FT_FREETYPE_H
#  include "wasm-types.h"
#endif // MKXPZ_RETRO

#include "disposable.h"
#include "etc-internal.h"
#include "etc.h"

#include "gl-util.h"

#include <string>
#include <vector>

#include "sigslot/signal.hpp"

class Font;
class ShaderBase;
struct SDL_Surface;

struct BitmapFrame
{
    TEXFBO gl;
#ifdef MKXPZ_RETRO
    std::vector<std::vector<uint32_t>> diff;
    std::string path;
    int originalFrameIndex;
#endif // MKXPZ_RETRO
};

struct BitmapPrivate;
// FIXME make this class use proper RGSS classes again
class Bitmap : public Disposable
{
	friend struct BitmapPrivate;
	friend struct PlanePrivate;
	friend class Sprite;
	friend struct SpritePrivate;
	friend struct TilemapPrivate;
	friend struct TilemapVXPrivate;
	friend class Window;
	friend struct WindowPrivate;
	friend class WindowVX;
	friend struct WindowVXPrivate;

public:
	Bitmap(Exception &exception, const char *filename, bool useDiff = true);
	Bitmap(Exception &exception, int width = 1, int height = 1, bool isHires = false, bool useDiff = true);
	Bitmap(Exception &exception, void *pixeldata, int width, int height, bool useDiff = true);
	Bitmap(Exception &exception, TEXFBO &other, bool useDiff = true);
	Bitmap(Exception &exception, SDL_Surface *imgSurf, SDL_Surface *imgSurfHires, bool forceMega = false, bool useDiff = true);

	/* Clone constructor */
    
    // frame is -2 for "any and all", -1 for "current", anything else for a specific frame
	Bitmap(Exception &exception, const Bitmap &other, int frame = -2, bool useDiff = true);
	~Bitmap();

	void initFromFilename(Exception &exception, const char *filename, bool useDiff = true);
	void initFromDimensions(Exception &exception, int width = 1, int height = 1, bool isHires = false, bool useDiff = true);
	void initFromSurface(Exception &exception, SDL_Surface *imgSurf, Bitmap *hiresBitmap, bool forceMega = false, bool useDiff = true);

	int getWidth(Exception &exception)  const;
	int getHeight(Exception &exception) const;
	bool getHasHires(Exception &exception) const;
	void setHiresRaw(Exception &exception, Bitmap *hires);
	DECL_ATTR(Hires, Bitmap*)
	void setLoresRaw(Exception &exception, Bitmap *lores);
	void setLores(Exception &exception, Bitmap *lores);
	bool getIsMega(Exception &exception) const;
	bool getIsAnimated(Exception &exception) const;
	IntRect getRect(Exception &exception) const;

	void blt(Exception &exception,
	         int x, int y,
	         const Bitmap &source, const IntRect &rect,
	         int opacity = 255);

	void stretchBlt(Exception &exception,
	                IntRect destRect,
	                const Bitmap &source, IntRect sourceRect,
	                int opacity = 255, bool smooth = false);

	void fillRect(Exception &exception,
	              int x, int y,
	              int width, int height,
	              const Vec4 &color);
	void fillRect(Exception &exception, const IntRect &rect, const Vec4 &color);

	void gradientFillRect(Exception &exception,
	                      int x, int y,
	                      int width, int height,
	                      const Vec4 &color1, const Vec4 &color2,
	                      bool vertical = false);
	void gradientFillRect(Exception &exception,
	                      const IntRect &rect,
	                      const Vec4 &color1, const Vec4 &color2,
	                      bool vertical = false);

	void clearRect(Exception &exception,
	               int x, int y,
	               int width, int height);
	void clearRect(Exception &exception,
	               const IntRect &rect);

	void blur(Exception &exception);
	void radialBlur(Exception &exception, int angle, int divisions);

	void clear(Exception &exception);

	Color getPixel(Exception &exception, int x, int y) const;
	void setPixel(Exception &exception, int x, int y, const Color &color);
    
    bool getRaw(Exception &exception, void *output, int output_size);
    void replaceRaw(Exception &exception, void *pixel_data, int size);
    void saveToFile(Exception &exception, const char *filename);

	void hueChange(Exception &exception, int hue);

	enum TextAlign
	{
		Left = 0,
		Center = 1,
		Right = 2
	};

	void drawText(Exception &exception,
	              int x, int y,
	              int width, int height,
	              const char *str, int align = Left);

	void drawText(Exception &exception,
	              const IntRect &rect,
	              const char *str, int align = Left);

	IntRect textSize(Exception &exception, const char *str);

	Font &getFont(Exception &exception) const;
	void setFont(Font &value);

	/* Sets initial reference without copying by value,
	 * use at construction */
	void setInitFont(Font *value);

	/* <internal> */
	TEXFBO &getGLTypes() const;
    SDL_Surface *surface() const;
	SDL_Surface *megaSurface() const;
	void ensureNonMega(Exception &exception) const;
    void ensureNonAnimated(Exception &exception) const;
    void ensureAnimated(Exception &exception) const;
    
    // Animation functions
    void stop(Exception &exception);
    void play(Exception &exception);
    bool isPlaying(Exception &exception) const;
    bool getPlaying(Exception &exception) const;
    void setPlaying(Exception &exception, bool playing);
    void gotoAndStop(Exception &exception, int frame);
    void gotoAndPlay(Exception &exception, int frame);
    int numFrames(Exception &exception) const;
    int currentFrameI(Exception &exception) const;
    
    int addFrame(Exception &exception, Bitmap &source, int position = -1);
    void removeFrame(Exception &exception, int position = -1);
    
    void nextFrame(Exception &exception);
    void previousFrame(Exception &exception);
    std::vector<BitmapFrame> &getFrames() const;
    
    void setAnimationFPS(Exception &exception, float FPS);
    float getAnimationFPS(Exception &exception) const;
    
    void setLooping(Exception &exception, bool loop);
    bool getLooping(Exception &exception) const;

    void ensureNotPlaying(Exception &exception) const;
    // ----------
    
	/* Binds the backing texture and sets the correct
	 * texture size uniform in shader */
	void bindTex(ShaderBase &shader, bool substituteLoresSize = true);

	/* Adds 'rect' to tainted area */
	void taintArea(const IntRect &rect);

	sigslot::signal<> modified;
#ifdef MKXPZ_RETRO
	const uint64_t id; // Globally unique nonzero ID for this bitmap, for change detection during save state deserialization
	bool deserModified;
	bool deserSizeChanged;
#endif // MKXPZ_RETRO

	static int maxSize();

    void assumeRubyGC(bool value = true);

#ifdef MKXPZ_RETRO
	bool sandbox_serialize_without_hires(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
	bool sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
	bool sandbox_deserialize_without_hires(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
	bool sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size);
	void sandbox_deserialize_begin(bool is_new);
	void sandbox_deserialize_end(bool is_sandbox_object);
	void sandbox_reinit();
#endif // MKXPZ_RETRO

private:
	int width()  const;
	int height() const;
	bool hasHires() const;
	bool isMega() const;
	bool isAnimated() const;
	IntRect rect() const;
	float animationFPS() const;
	bool looping() const;

	void releaseResources();
	sigslot::connection loresDispCon;
	const char *klassName() const { return "bitmap"; }

	BitmapPrivate *p;

	void loresDisposal();

#ifdef MKXPZ_RETRO
	SDL_Surface *drawTextInner(FT_Face font, const char *str, SDL_Color &c, size_t outline);
	bool sandbox_serialize_pixels(void *&data, mkxp_sandbox::wasm_size_t &max_size, const std::vector<std::vector<uint32_t>> &diff) const;
	bool sandbox_deserialize_pixels_check_need_reload(const void *&data, mkxp_sandbox::wasm_size_t &max_size, const std::vector<std::vector<uint32_t>> &diff, bool &need_reload, bool &need_reload_if_path_not_empty, bool modify_data_and_max_size) const;
	bool sandbox_deserialize_pixels(const void *&data, mkxp_sandbox::wasm_size_t &max_size, std::vector<std::vector<uint32_t>> &diff, mkxp_sandbox::wasm_size_t frame_number = 0);
#endif // MKXPZ_RETRO
};

#endif // BITMAP_H
