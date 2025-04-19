/*
** font.cpp
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

#include "font.h"

#include "sharedstate.h"
#include "filesystem.h"
#include "exception.h"
#include "boost-hash.h"
#include "util.h"
#include "config.h"

#include "debugwriter.h"

#include <string>
#include <utility>
#include <algorithm>
#include <cctype>

#ifdef MKXPZ_BUILD_XCODE
#include "filesystem/filesystem.h"
#endif

#ifndef MKXPZ_RETRO
#  include <SDL_ttf.h>
#endif // MKXPZ_RETRO

#ifndef MKXPZ_BUILD_XCODE
#ifndef MKXPZ_CJK_FONT
#include "liberation.ttf.xxd"
#else
#include "wqymicrohei.ttf.xxd"
#endif


#ifndef MKXPZ_CJK_FONT
#define BUNDLED_FONT liberation
#else
#define BUNDLED_FONT wqymicrohei
#endif

#define BUNDLED_FONT_D(f) mkxp_assets_## f ##_ttf
#define BUNDLED_FONT_L(f) sizeof mkxp_assets_## f ##_ttf

// Go fuck yourself CPP
#define BNDL_F_D(f) BUNDLED_FONT_D(f)
#define BNDL_F_L(f) BUNDLED_FONT_L(f)

#endif

#ifndef MKXPZ_RETRO
static SDL_RWops *openBundledFont()
{
#ifndef MKXPZ_BUILD_XCODE
    return SDL_RWFromConstMem(BNDL_F_D(BUNDLED_FONT), BNDL_F_L(BUNDLED_FONT));
#else
    return SDL_RWFromFile(mkxp_fs::getPathForAsset("Fonts/liberation", "ttf").c_str(), "rb");
#endif
}
#endif // MKXPZ_RETRO



typedef std::pair<std::string, int> FontKey;

struct FontSet
{
	/* 'Regular' style */
	std::string regular;

	/* Any other styles (used in case no 'Regular' exists) */
	std::string other;
};

struct SharedFontStatePrivate
{
#ifdef MKXPZ_RETRO
	FT_Library library;
#endif // MKXPZ_RETRO

	/* Maps: font family name, To: substituted family name,
	 * as specified via configuration file / arguments */
	BoostHash<std::string, std::string> subs;

	/* Maps: font family name, To: set of physical
	 * font filenames located in "Fonts/" */
	BoostHash<std::string, FontSet> sets;

	/* Pool of already opened fonts; once opened, they are reused
	 * and never closed until the termination of the program */
#ifdef MKXPZ_RETRO
	BoostHash<FontKey, FT_Face> pool;
#else
	BoostHash<FontKey, TTF_Font*> pool;
#endif // MKXPZ_RETRO
    
    /* Internal default font family that is used anytime an
     * empty/invalid family is requested */
    std::string defaultFamily;

#ifdef MKXPZ_RETRO
	FT_Error ftOpenFile(std::shared_ptr<struct FileSystem::File> ops, FT_Face &font)
	{
		FT_StreamRec ft_stream = {
			.base = NULL,
			.size = (unsigned long)-1,
			.pos = 0,
			.descriptor = {.pointer = new std::shared_ptr<struct FileSystem::File>(ops)},
			.pathname = {.pointer = NULL},
			.read = [](FT_Stream stream, unsigned long offset, unsigned char *buffer, unsigned long count) {
				if (!PHYSFS_seek(((std::shared_ptr<struct FileSystem::File> *)stream->descriptor.pointer)->get()->get(), offset))
					return (unsigned long)(count == 0);
				if (count == 0)
					return 0UL;
				PHYSFS_uint64 n = PHYSFS_readBytes(((std::shared_ptr<struct FileSystem::File> *)stream->descriptor.pointer)->get()->get(), buffer, count);
				return n < 0 ? 0UL : (unsigned long)n;
			},
			.close = [](FT_Stream stream) {
				delete (std::shared_ptr<struct FileSystem::File> *)stream->descriptor.pointer;
			},
		};
	
		const FT_Open_Args ft_open_args = {
			.flags = FT_OPEN_STREAM,
			.memory_base = NULL,
			.memory_size = 0,
			.pathname = NULL,
			.stream = &ft_stream,
			.driver = NULL,
			.num_params = 0,
			.params = NULL,
		};
	
		return FT_Open_Face(library, &ft_open_args, 0, &font);
	}
#endif // MKXPZ_RETRO
};

SharedFontState::SharedFontState(const Config &conf)
{
	p = new SharedFontStatePrivate;

#ifdef MKXPZ_RETRO
	FT_Error ft_error = FT_Init_FreeType(&p->library);
	if (ft_error)
	{
		mkxp_retro::log_printf(RETRO_LOG_ERROR, "FreeType error %d\n", ft_error);
		std::abort();
	}
#endif // MKXPZ_RETRO

#ifndef MKXPZ_RETRO // TODO
	/* Parse font substitutions */
	for (size_t i = 0; i < conf.fontSubs.size(); ++i)
	{
		const std::string &raw = conf.fontSubs[i];
		size_t sepPos = raw.find_first_of('>');

		if (sepPos == std::string::npos)
			continue;

		std::string from = raw.substr(0, sepPos);
		std::string to   = raw.substr(sepPos+1);

		p->subs.insert(from, to);
	}
#endif // MKXPZ_RETRO
}

SharedFontState::~SharedFontState()
{
#ifdef MKXPZ_RETRO
	BoostHash<FontKey, FT_Face>::const_iterator iter;
#else
	BoostHash<FontKey, TTF_Font*>::const_iterator iter;
#endif // MKXPZ_RETRO
	for (iter = p->pool.cbegin(); iter != p->pool.cend(); ++iter)
#ifdef MKXPZ_RETRO
		FT_Done_Face(iter->second);
#else
		TTF_CloseFont(iter->second);
#endif // MKXPZ_RETRO

#ifdef MKXPZ_RETRO
	FT_Done_FreeType(p->library);
#endif // MKXPZ_RETRO

	delete p;
}

void SharedFontState::initFontSetCB(
#ifdef MKXPZ_RETRO
                                    std::shared_ptr<struct FileSystem::File> ops,
#else
                                    SDL_RWops &ops,
#endif // MKXPZ_RETRO
                                    const std::string &filename)
{
#ifdef MKXPZ_RETRO
	FT_Face font;
	if (p->ftOpenFile(ops, font))
		return;

	std::string family(font->family_name);
	std::string style(font->style_name);
#else
	TTF_Font *font = TTF_OpenFontRW(&ops, 0, 0);

	if (!font)
		return;

	std::string family = TTF_FontFaceFamilyName(font);
	std::string style = TTF_FontFaceStyleName(font);
#endif // MKXPZ_RETRO

	std::transform(family.begin(), family.end(), family.begin(),
		[](unsigned char c){ return std::tolower(c); });

#ifdef MKXPZ_RETRO
	FT_Done_Face(font);
#else
	TTF_CloseFont(font);
#endif // MKXPZ_RETRO

	FontSet &set = p->sets[family];

	if (style == "Regular")
		set.regular = filename;
	else
		set.other = filename;
}

#ifdef MKXPZ_RETRO
FT_Face
#else
_TTF_Font *
#endif // MKXPZ_RETRO
SharedFontState::getFont(std::string family,
                                    int size)
{
	std::transform(family.begin(), family.end(), family.begin(),
		[](unsigned char c){ return std::tolower(c); });

	if (family.empty())
		family = p->defaultFamily;

	/* Check for substitutions */
	if (p->subs.contains(family))
		family = p->subs[family];

	/* Find out if the font asset exists */
	const FontSet &req = p->sets[family];

	if (req.regular.empty() && req.other.empty())
	{
		/* Doesn't exist; use built-in font */
		family = "";
	}

	FontKey key(family, size);

#ifdef MKXPZ_RETRO
	FT_Face font = p->pool.value(key);
#else
	TTF_Font *font = p->pool.value(key);
#endif // MKXPZ_RETRO

	if (font)
		return font;

	/* Not in pool; open new handle */
#ifdef MKXPZ_RETRO
	if (family.empty())
	{
		if (FT_New_Memory_Face(p->library, BNDL_F_D(BUNDLED_FONT), BNDL_F_L(BUNDLED_FONT), 0, &font))
			throw Exception(Exception::SDLError, "failed to load font");
	}
	else
	{
		/* Use 'other' path as alternative in case
		 * we have no 'regular' styled font asset */
		const char *path = !req.regular.empty()
		                 ? req.regular.c_str() : req.other.c_str();

		if (p->ftOpenFile(std::shared_ptr<struct FileSystem::File>(new struct FileSystem::File(*mkxp_retro::fs, path, FileSystem::OpenMode::Read)), font))
			throw Exception(Exception::SDLError, "failed to load font");
	}

	// FIXME 0.9 is guesswork at this point
	FT_Set_Char_Size(font, 0, (int)(size * 0.90f) * 64, 0, 0);
#else
	SDL_RWops *ops;

	if (family.empty())
	{
		/* Built-in font */
		ops = openBundledFont();
	}
	else
	{
		/* Use 'other' path as alternative in case
		 * we have no 'regular' styled font asset */
		const char *path = !req.regular.empty()
		                 ? req.regular.c_str() : req.other.c_str();

		ops = SDL_AllocRW();
		shState->fileSystem().openReadRaw(*ops, path, true);
	}

	// FIXME 0.9 is guesswork at this point
//	float gamma = (96.0/45.0)*(5.0/14.0)*(size-5);
//	font = TTF_OpenFontRW(ops, 1, gamma /** .90*/);
	font = TTF_OpenFontRW(ops, 1, size* 0.90f);

	if (!font)
		throw Exception(Exception::SDLError, "%s", SDL_GetError());
#endif // MKXPZ_RETRO

	p->pool.insert(key, font);

	return font;
}

bool SharedFontState::fontPresent(std::string family) const
{
	std::transform(family.begin(), family.end(), family.begin(),
		[](unsigned char c){ return std::tolower(c); });

	/* Check for substitutions */
	if (p->subs.contains(family))
		family = p->subs[family];

	const FontSet &set = p->sets[family];

	return !(set.regular.empty() && set.other.empty());
}

#ifdef MKXPZ_RETRO
FT_Library SharedFontState::getLibrary() const noexcept
{
	return p->library;
}
#else
_TTF_Font *SharedFontState::openBundled(int size)
{
	SDL_RWops *ops = openBundledFont();

	return TTF_OpenFontRW(ops, 1, size);
}
#endif // MKXPZ_RETRO

void SharedFontState::setDefaultFontFamily(const std::string &family) {
    p->defaultFamily = family;
}

static void pickExistingFontName(const std::vector<std::string> &names,
                          std::string &out,
                          const SharedFontState &sfs)
{
	/* Note: In RMXP, a names array with no existing entry
	 * results in no text being drawn at all (same for "" and []);
	 * we can't replicate this in mkxp due to the default substitute. */

	for (size_t i = 0; i < names.size(); ++i)
	{
		if (sfs.fontPresent(names[i]))
		{
			out = names[i];
			return;
		}
		else
		{
			if (i == 0)
			{
				Debug() << "Primary font not found:" << names[i];
			}
			else
			{
				Debug() << "Fallback font not found:" << names[i];
			}
		}
	}

	out = "";
}


struct FontPrivate
{
	std::string name;
	int size;
	bool bold;
	bool italic;
	bool outline;
	bool shadow;
	Color *color;
	Color *outColor;

	Color colorTmp;
	Color outColorTmp;

	static std::string defaultName;
	static int defaultSize;
	static bool defaultBold;
	static bool defaultItalic;
	static bool defaultOutline;
	static bool defaultShadow;
	static Color *defaultColor;
	static Color *defaultOutColor;

	static Color defaultColorTmp;
	static Color defaultOutColorTmp;

	static std::vector<std::string> initialDefaultNames;

	/* The actual font is opened as late as possible
	 * (when it is queried by a Bitmap), prior it is
	 * set to null */
#ifdef MKXPZ_RETRO
	FT_Face sdlFont;
#else
	TTF_Font *sdlFont;
#endif // MKXPZ_RETRO
    
    bool isSolid;

	FontPrivate(int size)
	    : size(size),
	      bold(defaultBold),
	      italic(defaultItalic),
	      outline(defaultOutline),
	      shadow(defaultShadow),
	      color(&colorTmp),
	      outColor(&outColorTmp),
	      colorTmp(*defaultColor),
	      outColorTmp(*defaultOutColor),
	      sdlFont(0),
          isSolid(false)
	{}

	FontPrivate(const FontPrivate &other)
	    : name(other.name),
	      size(other.size),
	      bold(other.bold),
	      italic(other.italic),
	      outline(other.outline),
	      shadow(other.shadow),
	      color(&colorTmp),
	      outColor(&outColorTmp),
	      colorTmp(*other.color),
	      outColorTmp(*other.outColor),
	      sdlFont(other.sdlFont),
          isSolid(false)
	{}

	void operator=(const FontPrivate &o)
	{
		 name     =  o.name;
		 size     =  o.size;
		 bold     =  o.bold;
		 italic   =  o.italic;
		 outline  =  o.outline;
		 shadow   =  o.shadow;
		*color    = *o.color;
		*outColor = *o.outColor;

		sdlFont = 0;
        isSolid = o.isSolid;
	}
};

std::string FontPrivate::defaultName     = "Arial";
int         FontPrivate::defaultSize     = 22;
bool        FontPrivate::defaultBold     = false;
bool        FontPrivate::defaultItalic   = false;
bool        FontPrivate::defaultOutline  = false; /* Inited at runtime */
bool        FontPrivate::defaultShadow   = false; /* Inited at runtime */
Color      *FontPrivate::defaultColor    = &FontPrivate::defaultColorTmp;
Color      *FontPrivate::defaultOutColor = &FontPrivate::defaultOutColorTmp;

Color FontPrivate::defaultColorTmp(255, 255, 255, 255);
Color FontPrivate::defaultOutColorTmp(0, 0, 0, 128);

std::vector<std::string> FontPrivate::initialDefaultNames;

bool Font::isSolid() const {
    return p->isSolid;
}

bool Font::doesExist(const char *name)
{
	if (!name)
		return false;

	return shState->fontState().fontPresent(name);
}

Font::Font(const std::vector<std::string> *names,
           int size)
{
	p = new FontPrivate(size ? size : FontPrivate::defaultSize);

	if (names)
		setName(*names);
	else
		p->name = FontPrivate::defaultName;
}

Font::Font(const Font &other)
{
	p = new FontPrivate(*other.p);
}

Font::~Font()
{
	delete p;
}

const Font &Font::operator=(const Font &o)
{
	*p = *o.p;

	return o;
}

void Font::setName(const std::vector<std::string> &names)
{
	pickExistingFontName(names, p->name, shState->fontState());
#ifdef MKXPZ_RETRO // TODO
	p->isSolid = false;
#else
	p->isSolid = strcmp(p->name.c_str(), "") && shState->config().fontIsSolid(p->name.c_str());
#endif // MKXPZ_RETRO
	p->sdlFont = 0;
}

void Font::setSize(int value, bool checkIllegal)
{
	if (p->size == value)
		return;

	/* Catch illegal values (according to RMXP) */
	if (value < 6 || value > 96) {
		if (checkIllegal) {
			throw Exception(Exception::ArgumentError, "%s", "bad value for size");
		}
	}

	p->size = value;
	p->sdlFont = 0;
}

static void guardDisposed() {}

DEF_ATTR_RD_SIMPLE(Font, Size, int, p->size)

DEF_ATTR_SIMPLE(Font, Bold,     bool,    p->bold)
DEF_ATTR_SIMPLE(Font, Italic,   bool,    p->italic)
DEF_ATTR_SIMPLE(Font, Shadow,   bool,    p->shadow)
DEF_ATTR_SIMPLE(Font, Outline,  bool,    p->outline)
DEF_ATTR_SIMPLE(Font, Color,    Color&, *p->color)
DEF_ATTR_SIMPLE(Font, OutColor, Color&, *p->outColor)

DEF_ATTR_SIMPLE_STATIC(Font, DefaultSize,     int,     FontPrivate::defaultSize)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultBold,     bool,    FontPrivate::defaultBold)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultItalic,   bool,    FontPrivate::defaultItalic)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultShadow,   bool,    FontPrivate::defaultShadow)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultOutline,  bool,    FontPrivate::defaultOutline)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultColor,    Color&, *FontPrivate::defaultColor)
DEF_ATTR_SIMPLE_STATIC(Font, DefaultOutColor, Color&, *FontPrivate::defaultOutColor)

void Font::setDefaultName(const std::vector<std::string> &names,
                          const SharedFontState &sfs)
{
	pickExistingFontName(names, FontPrivate::defaultName, sfs);
}

const std::vector<std::string> &Font::getInitialDefaultNames()
{
	return FontPrivate::initialDefaultNames;
}

void Font::initDynAttribs()
{
	p->color = new Color(p->colorTmp);

	if (rgssVer >= 3)
		p->outColor = new Color(p->outColorTmp);;
}

void Font::initDefaultDynAttribs()
{
	if (FontPrivate::defaultColor != &FontPrivate::defaultColorTmp)
	{
		delete FontPrivate::defaultColor;
		FontPrivate::defaultColor = &FontPrivate::defaultColorTmp;
	}
	FontPrivate::defaultColor = new Color(FontPrivate::defaultColorTmp);

	if (FontPrivate::defaultOutColor != &FontPrivate::defaultOutColorTmp)
	{
		delete FontPrivate::defaultOutColor;
		FontPrivate::defaultOutColor = &FontPrivate::defaultOutColorTmp;
	}
	if (rgssVer >= 3)
		FontPrivate::defaultOutColor = new Color(FontPrivate::defaultOutColorTmp);
}

void Font::initDefaults(const SharedFontState &sfs)
{
	std::vector<std::string> &names = FontPrivate::initialDefaultNames;
	names.clear();

	switch (rgssVer)
	{
	case 1 :
		// FIXME: Japanese version has "MS PGothic" instead
		names.push_back("Arial");
		break;

	case 2 :
		names.push_back("UmePlus Gothic");
		names.push_back("MS Gothic");
		names.push_back("Courier New");
		break;

	default:
	case 3 :
		names.push_back("VL Gothic");
	}

	setDefaultName(names, sfs);

	FontPrivate::defaultOutline = (rgssVer >= 3 ? true : false);
	FontPrivate::defaultShadow  = (rgssVer == 2 ? true : false);
}

#ifdef MKXPZ_RETRO
FT_Face
#else
_TTF_Font *
#endif // MKXPZ_RETRO
Font::getSdlFont()
{
	if (!p->sdlFont)
		p->sdlFont = shState->fontState().getFont(p->name.c_str(),
		                                          p->size);

#ifndef MKXPZ_RETRO // TODO
	int style = TTF_STYLE_NORMAL;

	if (p->bold)
		style |= TTF_STYLE_BOLD;

	if (p->italic)
		style |= TTF_STYLE_ITALIC;

	TTF_SetFontStyle(p->sdlFont, style);
#endif // MKXPZ_RETRO

	return p->sdlFont;
}
