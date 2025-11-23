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
#include "forced-assert.h"
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

#ifdef MKXPZ_RETRO
#  include "sandbox-serial-util.h"
#  include <boost/optional.hpp>
#else
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
	struct PoolEntry
	{
		FT_StreamRec *rec;
		FT_Open_Args *args;
		FT_Face font;
		PoolEntry() : rec(nullptr), args(nullptr), font(nullptr) {}
		PoolEntry(const struct PoolEntry &entry) = delete;
		PoolEntry(struct PoolEntry &&entry) noexcept : rec(std::exchange(entry.rec, nullptr)), args(std::exchange(entry.args, nullptr)), font(std::exchange(entry.font, nullptr)) {};
		struct PoolEntry &operator=(const struct PoolEntry &entry) = delete;
		struct PoolEntry &operator=(struct PoolEntry &&entry) noexcept {
			rec = std::exchange(entry.rec, nullptr);
			args = std::exchange(entry.args, nullptr);
			font = std::exchange(entry.font, nullptr);
			return *this;
		}
		~PoolEntry() {
			if (font != nullptr) {
				FT_Done_Face(font);
			}
			if (args != nullptr) {
				delete args;
			}
			if (rec != nullptr) {
				delete rec;
			}
		}
	};
	BoostHash<FontKey, PoolEntry> pool;
#else
	BoostHash<FontKey, TTF_Font*> pool;
#endif // MKXPZ_RETRO
    
    /* Internal default font family that is used anytime an
     * empty/invalid family is requested */
    std::string defaultFamily;

#ifdef MKXPZ_RETRO
	boost::optional<SharedFontStatePrivate::PoolEntry> ftOpenFile(std::shared_ptr<struct FileSystem::File> ops)
	{
		SharedFontStatePrivate::PoolEntry entry;

		entry.rec = new FT_StreamRec {
			nullptr,
			(unsigned long)-1,
			0,
			{},
			{},
			[](FT_Stream stream, unsigned long offset, unsigned char *buffer, unsigned long count) {
				if (!PHYSFS_seek(((std::shared_ptr<struct FileSystem::File> *)stream->descriptor.pointer)->get()->get_read(), offset))
					return (unsigned long)(count == 0);
				if (count == 0)
					return 0UL;
				PHYSFS_uint64 n = PHYSFS_readBytes(((std::shared_ptr<struct FileSystem::File> *)stream->descriptor.pointer)->get()->get_read(), buffer, count);
				return n < 0 ? 0UL : (unsigned long)n;
			},
			[](FT_Stream stream) {
				delete (std::shared_ptr<struct FileSystem::File> *)stream->descriptor.pointer;
			},
		};
		entry.rec->descriptor.pointer = new std::shared_ptr<struct FileSystem::File>(ops);
	
		entry.args = new FT_Open_Args {
			FT_OPEN_STREAM,
			nullptr,
			0,
			nullptr,
			entry.rec,
			nullptr,
			0,
			nullptr,
		};
	
		if (!FT_Open_Face(library, entry.args, 0, &entry.font)) {
			return entry;
		} else {
			return boost::none;
		}
	}

	SharedFontStatePrivate()
	{
		MKXPZ_FORCED_ASSERT(!FT_Init_FreeType(&library));
	}

	~SharedFontStatePrivate()
	{
		pool.clear();
		FT_Done_FreeType(library);
	}
#endif // MKXPZ_RETRO
};

SharedFontState::SharedFontState(const Config &conf)
{
	p = new SharedFontStatePrivate;

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
}

SharedFontState::~SharedFontState()
{
#ifndef MKXPZ_RETRO
	BoostHash<FontKey, TTF_Font*>::const_iterator iter;
	for (iter = p->pool.cbegin(); iter != p->pool.cend(); ++iter)
		TTF_CloseFont(iter->second);
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
	boost::optional<SharedFontStatePrivate::PoolEntry> entry = p->ftOpenFile(ops);
	if (!entry.has_value())
		return;

	std::string family(entry->font->family_name);
	std::string style(entry->font->style_name);
#else
	TTF_Font *font = TTF_OpenFontRW(&ops, 0, 0);

	if (!font)
		return;

	std::string family = TTF_FontFaceFamilyName(font);
	std::string style = TTF_FontFaceStyleName(font);
#endif // MKXPZ_RETRO

	std::transform(family.begin(), family.end(), family.begin(),
		[](unsigned char c){ return std::tolower(c); });

#ifndef MKXPZ_RETRO
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
SharedFontState::getFont(Exception &exception,
                                    std::string family,
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
	{
		const SharedFontStatePrivate::PoolEntry *entry = p->pool.value_ptr(key);
		if (entry != nullptr)
			return entry->font;
	}
	boost::optional<SharedFontStatePrivate::PoolEntry> entry;
#else
	TTF_Font *font = p->pool.value(key);
	if (font)
		return font;
#endif // MKXPZ_RETRO

	/* Not in pool; open new handle */
#ifdef MKXPZ_RETRO
	if (family.empty())
	{
		entry = SharedFontStatePrivate::PoolEntry();
		if (FT_New_Memory_Face(p->library, BNDL_F_D(BUNDLED_FONT), BNDL_F_L(BUNDLED_FONT), 0, &entry->font))
		{
			entry->font = nullptr;
			exception = Exception(Exception::SDLError, "failed to load font");
			return nullptr;
		}
	}
	else
	{
		/* Use 'other' path as alternative in case
		 * we have no 'regular' styled font asset */
		const char *path = !req.regular.empty()
		                 ? req.regular.c_str() : req.other.c_str();

		entry = p->ftOpenFile(std::shared_ptr<struct FileSystem::File>(new struct FileSystem::File(*mkxp_retro::fs, path)));
		if (!entry.has_value())
		{
			exception = Exception(Exception::SDLError, "failed to load font");
			return nullptr;
		}
	}

	// FIXME 0.9 is guesswork at this point
	FT_Set_Char_Size(entry->font, 0, (int)(size * 0.90f) * 64, 0, 0);
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
	{
		exception = Exception(Exception::SDLError, "%s", SDL_GetError());
		return nullptr;
	}
#endif // MKXPZ_RETRO

#ifdef MKXPZ_RETRO
	FT_Face font = entry->font;
	p->pool.insert(key, std::move(*entry));
	return font;
#else
	p->pool.insert(key, font);
	return font;
#endif // MKXPZ_RETRO
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
	p->isSolid = strcmp(p->name.c_str(), "") && shState->config().fontIsSolid(p->name.c_str());
	p->sdlFont = 0;
}

void Font::setSizeNoCheck(int value)
{
	if (p->size == value)
		return;

	p->size = value;
	p->sdlFont = 0;
}

void Font::setSizeCheck(Exception &exception, int value)
{
	if (p->size == value)
		return;

	/* Catch illegal values (according to RMXP) */
	if (value < 6 || value > 96) {
		exception = Exception(Exception::ArgumentError, "%s", "bad value for size");
		return;
	}

	p->size = value;
	p->sdlFont = 0;
}

DEF_ATTR_NOEXCEPT_RD_SIMPLE(Font, Size, int, p->size)

DEF_ATTR_NOEXCEPT_SIMPLE(Font, Bold,     bool,    p->bold)
DEF_ATTR_NOEXCEPT_SIMPLE(Font, Italic,   bool,    p->italic)
DEF_ATTR_NOEXCEPT_SIMPLE(Font, Shadow,   bool,    p->shadow)
DEF_ATTR_NOEXCEPT_SIMPLE(Font, Outline,  bool,    p->outline)
DEF_ATTR_NOEXCEPT_SIMPLE(Font, Color,    Color&, *p->color)
DEF_ATTR_NOEXCEPT_SIMPLE(Font, OutColor, Color&, *p->outColor)

DEF_ATTR_NOEXCEPT_SIMPLE_STATIC(Font, DefaultSize,     int,     FontPrivate::defaultSize)
DEF_ATTR_NOEXCEPT_SIMPLE_STATIC(Font, DefaultBold,     bool,    FontPrivate::defaultBold)
DEF_ATTR_NOEXCEPT_SIMPLE_STATIC(Font, DefaultItalic,   bool,    FontPrivate::defaultItalic)
DEF_ATTR_NOEXCEPT_SIMPLE_STATIC(Font, DefaultShadow,   bool,    FontPrivate::defaultShadow)
DEF_ATTR_NOEXCEPT_SIMPLE_STATIC(Font, DefaultOutline,  bool,    FontPrivate::defaultOutline)
DEF_ATTR_NOEXCEPT_SIMPLE_STATIC(Font, DefaultColor,    Color&, *FontPrivate::defaultColor)
DEF_ATTR_NOEXCEPT_SIMPLE_STATIC(Font, DefaultOutColor, Color&, *FontPrivate::defaultOutColor)

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
	FontPrivate::defaultColor = new Color(FontPrivate::defaultColorTmp);

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
Font::getSdlFont(Exception &exception)
{
	if (!p->sdlFont)
	{
		p->sdlFont = shState->fontState().getFont(exception, p->name.c_str(), p->size);
		if (exception.is_error())
			return nullptr;
	}

#ifndef MKXPZ_RETRO
	int style = TTF_STYLE_NORMAL;

	if (p->bold)
		style |= TTF_STYLE_BOLD;

	if (p->italic)
		style |= TTF_STYLE_ITALIC;

	TTF_SetFontStyle(p->sdlFont, style);
#endif // MKXPZ_RETRO

	return p->sdlFont;
}

#ifdef MKXPZ_RETRO
bool Font::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const
{
	if (!mkxp_sandbox::sandbox_serialize(p->bold, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->italic, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->outline, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->shadow, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize(p->color == &p->colorTmp ? nullptr : p->color, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->outColor == &p->outColorTmp ? nullptr : p->outColor, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_serialize((int32_t)p->size, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_serialize(p->name, data, max_size)) return false;

	return true;
}

bool Font::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size)
{
	if (!mkxp_sandbox::sandbox_deserialize(p->bold, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->italic, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->outline, data, max_size)) return false;
	if (!mkxp_sandbox::sandbox_deserialize(p->shadow, data, max_size)) return false;

	if (!mkxp_sandbox::sandbox_deserialize(p->color, data, max_size)) return false;
	if (p->color == nullptr) {
		p->color = &p->colorTmp;
	}
	if (!mkxp_sandbox::sandbox_deserialize(p->outColor, data, max_size)) return false;
	if (p->outColor == nullptr) {
		p->outColor = &p->outColorTmp;
	}

	// Invalidate the inner font object if either the name or size of this font is different from before
	if (p->sdlFont != nullptr) {
		int32_t size = p->size;
		if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->size, data, max_size)) return false;
		std::string name(p->name);
		if (!mkxp_sandbox::sandbox_deserialize(p->name, data, max_size)) return false;
		if (p->size != size || p->name != name) {
			p->sdlFont = nullptr;
		}
	} else {
		if (!mkxp_sandbox::sandbox_deserialize((int32_t &)p->size, data, max_size)) return false;
		if (!mkxp_sandbox::sandbox_deserialize(p->name, data, max_size)) return false;
	}

	return true;
}
#endif // MKXPZ_RETRO
