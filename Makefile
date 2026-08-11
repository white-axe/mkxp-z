# This is a makefile wrapper that builds mkxp-z for the current machine using Meson.

.PHONY: all default clean clean-builddir clean-subprojects clean-subprojects-packagecache

BUILDDIR ?= build

# C compiler executable
CC ?=
# C compiler flags
CFLAGS ?=
# C linker ID, passed to the compiler using the -fuse-ld= compiler flag
CC_LD ?=
# C++ compiler executable
CXX ?=
# C++ compiler flags
CXXFLAGS ?=
# C++ linker ID, passed to the compiler using the -fuse-ld= compiler flag
CXX_LD ?=
# Objective-C compiler executable
OBJC ?=
# Objective-C compiler flags
OBJCFLAGS ?=
# Objective-C linker ID, passed to the compiler using the -fuse-ld= compiler flag
OBJC_LD ?=
# Objective-C++ compiler executable
OBJCXX ?=
# Objective-C++ compiler flags
OBJCXXFLAGS ?=
# Objective-C++ linker ID, passed to the compiler using the -fuse-ld= compiler flag
OBJCXX_LD ?=
# Preprocessor flags
CPPFLAGS ?=
# Linker flags
LDFLAGS ?=
# Archiver executable (for generating static libraries)
AR ?=
# Windows resource compiler executable
WINDRES ?=

all default: $(BUILDDIR)/build.ninja
	ninja -C $(BUILDDIR) -v

$(BUILDDIR)/build.ninja:
	CC='$(CC)' CFLAGS='$(CFLAGS)' CC_LD='$(CC_LD)' CXX='$(CXX)' CXXFLAGS='$(CXXFLAGS)' CXX_LD='$(CXX_LD)' OBJC='$(OBJC)' OBJCFLAGS='$(OBJCFLAGS)' OBJC_LD='$(OBJC_LD)' OBJCXX='$(OBJCXX)' OBJCXXFLAGS='$(OBJCXXFLAGS)' OBJCXX_LD='$(OBJCXX_LD)' CPPFLAGS='$(CPPFLAGS)' LDFLAGS='$(LDFLAGS)' AR='$(AR)' WINDRES='$(WINDRES)' meson setup $(BUILDDIR) --buildtype release -Db_lto=true

clean: clean-builddir clean-subprojects-packagecache

clean-builddir:
	rm -rf $(BUILDDIR)

clean-subprojects-packagecache: clean-subprojects
	rm -rf subprojects/packagecache

clean-subprojects:
	meson subprojects purge --confirm
