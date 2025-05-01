# Configure flags are based on the ones from https://github.com/ruby/ruby.wasm

RUBY_VERSION ?= v3_3_7
WABT_VERSION ?= 1.0.37
LIBYAML_VERSION ?= 0.2.5
ZLIB_VERSION ?= 1.3.1
PICOSHA2_VERSION ?= 27fcf6979298949e8a462e16d09a0351c18fcaf2
TARGET ?= wasm32-wasip1
WASI_SDK ?= /opt/wasi-sdk
WASM_OPT ?= wasm-opt
CTAGS ?= ctags
AUTORECONF ?= autoreconf
CURL ?= curl
GIT ?= git
ZIP ?= zip
SED ?= sed
GREP ?= grep
CC ?= cc
CXX ?= c++
LD ?= ld
AR ?= ar
RANLIB ?= ranlib
CFLAGS ?=
CXXFLAGS ?=
LDFLAGS ?=
WASI_CFLAGS ?= -Oz -DNDEBUG
WASI_CXXFLAGS ?= -Oz -DNDEBUG
WASI_LDFLAGS ?=
WASM_OPT_FLAGS ?= -Oz

BUILD_PREFIX := ${PWD}/build
OUTDIR := $(BUILD_PREFIX)/libretro-stage1
LIBDIR := $(BUILD_PREFIX)/lib
DOWNLOADS := $(BUILD_PREFIX)/downloads
RUBY := $(LIBDIR)/bin/ruby
WASM2C := $(LIBDIR)/bin/wasm2c
CLONE := $(GIT) clone -q --depth 1
GITHUB := https://github.com
WASI_CC := $(WASI_SDK)/bin/clang
WASI_CXX := $(WASI_SDK)/bin/clang++
WASI_LD := $(WASI_SDK)/bin/clang
WASI_AR := $(WASI_SDK)/bin/llvm-ar
WASI_RANLIB := $(WASI_SDK)/bin/llvm-ranlib
NATIVE_TOOLCHAIN := CC=$(CC) CXX=$(CXX) LD=$(LD) AR=$(AR) RANLIB=$(RANLIB)
WASI_TOOLCHAIN := CC=$(WASI_CC) CXX=$(WASI_CXX) LD=$(WASI_LD) AR=$(WASI_AR) RANLIB=$(WASI_RANLIB)

all default: ruby-dist ruby-bindings

ruby-dist: $(OUTDIR)/mkxp-retro-dist.zip $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby.h $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby-impl.h $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_0.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_1.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_2.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_3.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_4.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_5.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_6.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_7.c

ruby-bindings: $(OUTDIR)/mkxp-sandbox-bindgen.cpp $(OUTDIR)/mkxp-sandbox-bindgen.h

clean: clean-ruby-dist clean-ruby-bindings
	rm -rf $(LIBDIR)/*
	rm -rf $(DOWNLOADS)/*

clean-ruby-dist:
	rm -f $(OUTDIR)/mkxp-retro-dist.zip
	rm -rf $(OUTDIR)/mkxp-retro-ruby

clean-ruby-bindings:
	rm -f $(OUTDIR)/mkxp-sandbox-bindgen.cpp
	rm -f $(OUTDIR)/mkxp-sandbox-bindgen.h
	rm -f $(LIBDIR)/tags.c
	rm -f $(LIBDIR)/tags

# Cross Ruby (targets WASI)

$(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby.h $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby-impl.h $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_0.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_1.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_2.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_3.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_4.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_5.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_6.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_7.c &: $(WASM2C) $(LIBDIR)/mkxp-retro-dist/bin/ruby
	mkdir -p $(OUTDIR)/mkxp-retro-ruby
	$(WASM2C) $(LIBDIR)/mkxp-retro-dist/bin/ruby -n ruby --num-outputs=8 -o $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby.c
	# Find all the function types declared using `FUNC_TYPE_DECL_EXTERN_T` and redeclare them as macros to fix the "initializer element is not constant" compilation error that occurs when using GCC versions earlier than 8.1.0
	rm -f $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby-func-types.h
	for func_type in $$($(GREP) -r 'FUNC_TYPE_DECL_EXTERN_T *([^()]*)' $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby-impl.h | $(GREP) -v '# *define' | $(SED) -e 's/FUNC_TYPE_DECL_EXTERN_T *( *\(.*\) *).*/\1/'); do \
		echo "#define $$func_type \"$$(od -N 32 -x /dev/urandom | cut -b 9- | tr -d ' \n' | $(SED) -e 's/.\{2\}/\\x&/g')\"" >> $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby-func-types.h; \
	done
	$(SED) -i 's/ *# *define * FUNC_TYPE_DECL_EXTERN_T *([^()]).*/#define FUNC_TYPE_DECL_EXTERN_T(x)/g' $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby-impl.h
	$(SED) -i 's/ *# *define * FUNC_TYPE_EXTERN_T *([^()]).*/#define FUNC_TYPE_EXTERN_T(x) const char _mkxp_unused_##x[]/g' $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby-impl.h
	$(SED) -i 's/ *# *define * FUNC_TYPE_T *([^()]).*/#define FUNC_TYPE_T(x) static const char _mkxp_unused_##x[]/g' $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby-impl.h
	$(SED) -i 's/__has_builtin *([^()]*)/0/g' $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby-impl.h
	$(SED) -i 's/#error "Missing implementation of __builtin_add_overflow or _addcarry_u64"/return __builtin_add_overflow(a, b, resptr);/g' $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby-impl.h

$(OUTDIR)/mkxp-retro-dist.zip: $(LIBDIR)/mkxp-retro-dist/bin/ruby $(RUBY)
	rm -rf $(LIBDIR)/_mkxp-retro-dist
	cp -r $(LIBDIR)/mkxp-retro-dist $(LIBDIR)/_mkxp-retro-dist
	rm $(LIBDIR)/_mkxp-retro-dist/bin/ruby
	rm $(LIBDIR)/_mkxp-retro-dist/lib/libruby-static.a
	rm -r $(LIBDIR)/_mkxp-retro-dist/include
	rm -r $(LIBDIR)/_mkxp-retro-dist/share
	rm -r $(LIBDIR)/_mkxp-retro-dist/lib/ruby/gems
	rm -r $(LIBDIR)/_mkxp-retro-dist/lib/ruby/$(shell $(RUBY) -e 'puts RUBY_VERSION.split(".")[..1].join(".")').0/bundler
	rm -r $(LIBDIR)/_mkxp-retro-dist/lib/ruby/$(shell $(RUBY) -e 'puts RUBY_VERSION.split(".")[..1].join(".")').0/rdoc
	rm -r $(LIBDIR)/_mkxp-retro-dist/lib/ruby/$(shell $(RUBY) -e 'puts RUBY_VERSION.split(".")[..1].join(".")').0/rubygems
	echo '# This is a Ruby script that does nothing. We make Ruby load this file on startup just because Ruby needs to load a script on startup for some reason.' > $(LIBDIR)/_mkxp-retro-dist/bin/mkxp-z
	rm -f $(OUTDIR)/mkxp-retro-dist.zip
	cd $(LIBDIR)/_mkxp-retro-dist && $(ZIP) -r $(OUTDIR)/mkxp-retro-dist.zip *
	rm -r $(LIBDIR)/_mkxp-retro-dist

$(LIBDIR)/mkxp-retro-dist/bin/ruby: $(DOWNLOADS)/crossruby/Makefile ruby-bindings.h
	cd $(DOWNLOADS)/crossruby && $(MAKE) install DESTDIR=$(LIBDIR)

$(OUTDIR)/mkxp-sandbox-bindgen.cpp $(OUTDIR)/mkxp-sandbox-bindgen.h &: sandbox-bindgen.rb $(LIBDIR)/tags $(RUBY)
	mkdir -p $(OUTDIR)
	cd $(LIBDIR) && $(RUBY) ${PWD}/sandbox-bindgen.rb
	mv $(LIBDIR)/mkxp-sandbox-bindgen.h $(OUTDIR)
	mv $(LIBDIR)/mkxp-sandbox-bindgen.cpp $(OUTDIR)

$(LIBDIR)/tags: $(LIBDIR)/tags.c
	$(CTAGS) --fields=kS --kinds-c=epx -o $(LIBDIR)/tags $(LIBDIR)/tags.c

$(LIBDIR)/tags.c: $(DOWNLOADS)/crossruby/.ext/include/$(TARGET)/ruby/config.h
	mkdir -p $(LIBDIR)
	echo '#include <ruby.h>' | $(WASI_CC) -E -I$(DOWNLOADS)/crossruby/include -I$(DOWNLOADS)/crossruby/.ext/include/$(TARGET) -o $(LIBDIR)/tags.c -

$(DOWNLOADS)/crossruby/Makefile $(DOWNLOADS)/crossruby/.ext/include/$(TARGET)/ruby/config.h &: $(DOWNLOADS)/crossruby/configure $(RUBY) $(LIBDIR)/usr/local/lib/libyaml.a $(LIBDIR)/usr/local/lib/libz.a
	cd $(DOWNLOADS)/crossruby && ./configure \
		--prefix=/mkxp-retro-dist \
		--host $(TARGET) \
		--build $(shell $(RUBY) -e 'puts(RbConfig::CONFIG["host"])') \
		--with-static-linked-ext \
		--with-ext=bigdecimal,cgi/escape,continuation,coverage,date,digest/bubblebabble,digest,digest/md5,digest/rmd160,digest/sha1,digest/sha2,etc,fcntl,fiber,json,json/generator,json/parser,nkf,objspace,pathname,psych,racc/cparse,rbconfig/sizeof,ripper,stringio,strscan,monitor,zlib \
		--with-libyaml-dir=$(LIBDIR)/usr/local \
		--with-zlib-dir=$(LIBDIR)/usr/local \
		--with-baseruby=$(RUBY) \
		WASMOPT=$(WASM_OPT) \
		WASI_SDK_PATH=$(WASI_SDK) \
		$(WASI_TOOLCHAIN) \
		LDFLAGS='-Wl,--export-dynamic,--stack-first,-z,stack-size=2097152 $(WASI_LDFLAGS)' \
		XCFLAGS='-DWASM_SETJMP_STACK_BUFFER_SIZE=24576 -DWASM_FIBER_STACK_BUFFER_SIZE=24576 -DWASM_SCAN_STACK_BUFFER_SIZE=24576' \
		cppflags='$(WASI_CXXFLAGS)' \
		optflags='$(WASI_CFLAGS) -fno-fast-math' \
		wasmoptflags='$(WASM_OPT_FLAGS) --pass-arg=asyncify-ignore-imports' \
		$(shell $(RUBY) -e 'puts "ac_cv_func_dlopen=no" if RUBY_VERSION.split(".")[..1].join(".").to_f < 3.3') \
		--disable-rubygems \
		--disable-install-doc

$(DOWNLOADS)/crossruby/configure: $(DOWNLOADS)/crossruby/configure.ac $(RUBY)
	cd $(DOWNLOADS)/crossruby && $(RUBY) tool/downloader.rb -d tool -e gnu config.guess config.sub
	cd $(DOWNLOADS)/crossruby && $(AUTORECONF) -i

$(DOWNLOADS)/crossruby/configure.ac:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/ruby/ruby $(DOWNLOADS)/crossruby -b $(RUBY_VERSION)
	cd $(DOWNLOADS)/crossruby && $(GIT) apply ${PWD}/ruby-stack-pointer.patch
	cd $(DOWNLOADS)/crossruby && $(GIT) apply ${PWD}/ruby-jump-buffer.patch
	cd $(DOWNLOADS)/crossruby && $(GIT) apply ${PWD}/ruby-compat.patch
	echo '#include "${PWD}/ruby-bindings.h"' >> $(DOWNLOADS)/crossruby/gc.c

# Base Ruby (targets the build machine)

$(RUBY): $(DOWNLOADS)/baseruby/Makefile
	cd $(DOWNLOADS)/baseruby && $(MAKE) install
	if ! $(RUBY) -e 'exit 1 if RUBY_VERSION.split(".")[..1].join(".").to_f < 3.2'; then echo -e '\e[91m[ERROR] Ruby version must be at least 3.2 because earlier versions do not have WebAssembly support\e[0m'; rm $(RUBY); exit 1; fi;

$(DOWNLOADS)/baseruby/Makefile: $(DOWNLOADS)/baseruby/configure
	cd $(DOWNLOADS)/baseruby && ./configure --prefix=$(LIBDIR) $(NATIVE_TOOLCHAIN) LDFLAGS='$(LDFLAGS)' XCFLAGS='$(CFLAGS)' cppflags='$(CXXFLAGS)' --disable-install-doc

$(DOWNLOADS)/baseruby/configure: $(DOWNLOADS)/baseruby/configure.ac
	cd $(DOWNLOADS)/baseruby && $(AUTORECONF) -i

$(DOWNLOADS)/baseruby/configure.ac:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/ruby/ruby $(DOWNLOADS)/baseruby -b $(RUBY_VERSION)

# wasm2c

WASM2C_SRC := \
	$(DOWNLOADS)/wabt/src/apply-names.cc \
	$(DOWNLOADS)/wabt/src/binary-reader-ir.cc \
	$(DOWNLOADS)/wabt/src/binary-reader-logging.cc \
	$(DOWNLOADS)/wabt/src/binary-reader.cc \
	$(DOWNLOADS)/wabt/src/binary-writer-spec.cc \
	$(DOWNLOADS)/wabt/src/binary-writer.cc \
	$(DOWNLOADS)/wabt/src/binary.cc \
	$(DOWNLOADS)/wabt/src/binding-hash.cc \
	$(DOWNLOADS)/wabt/src/color.cc \
	$(DOWNLOADS)/wabt/src/common.cc \
	$(DOWNLOADS)/wabt/src/config.cc \
	$(DOWNLOADS)/wabt/src/decompiler.cc \
	$(DOWNLOADS)/wabt/src/error-formatter.cc \
	$(DOWNLOADS)/wabt/src/expr-visitor.cc \
	$(DOWNLOADS)/wabt/src/feature.cc \
	$(DOWNLOADS)/wabt/src/filenames.cc \
	$(DOWNLOADS)/wabt/src/generate-names.cc \
	$(DOWNLOADS)/wabt/src/ir-util.cc \
	$(DOWNLOADS)/wabt/src/ir.cc \
	$(DOWNLOADS)/wabt/src/leb128.cc \
	$(DOWNLOADS)/wabt/src/lexer-source-line-finder.cc \
	$(DOWNLOADS)/wabt/src/lexer-source.cc \
	$(DOWNLOADS)/wabt/src/literal.cc \
	$(DOWNLOADS)/wabt/src/opcode-code-table.c \
	$(DOWNLOADS)/wabt/src/opcode.cc \
	$(DOWNLOADS)/wabt/src/option-parser.cc \
	$(DOWNLOADS)/wabt/src/resolve-names.cc \
	$(DOWNLOADS)/wabt/src/sha256.cc \
	$(DOWNLOADS)/wabt/src/shared-validator.cc \
	$(DOWNLOADS)/wabt/src/stream.cc \
	$(DOWNLOADS)/wabt/src/token.cc \
	$(DOWNLOADS)/wabt/src/tracing.cc \
	$(DOWNLOADS)/wabt/src/type-checker.cc \
	$(DOWNLOADS)/wabt/src/utf8.cc \
	$(DOWNLOADS)/wabt/src/validator.cc \
	$(DOWNLOADS)/wabt/src/wast-lexer.cc \
	$(DOWNLOADS)/wabt/src/wast-parser.cc \
	$(DOWNLOADS)/wabt/src/wat-writer.cc \
	$(DOWNLOADS)/wabt/src/c-writer.cc \
	$(DOWNLOADS)/wabt/src/prebuilt/wasm2c_header_top.cc \
	$(DOWNLOADS)/wabt/src/prebuilt/wasm2c_header_bottom.cc \
	$(DOWNLOADS)/wabt/src/prebuilt/wasm2c_source_includes.cc \
	$(DOWNLOADS)/wabt/src/prebuilt/wasm2c_source_declarations.cc \
	$(DOWNLOADS)/wabt/src/prebuilt/wasm2c_simd_source_declarations.cc \
	$(DOWNLOADS)/wabt/src/prebuilt/wasm2c_atomicops_source_declarations.cc \
	$(DOWNLOADS)/wabt/src/interp/binary-reader-interp.cc \
	$(DOWNLOADS)/wabt/src/interp/interp.cc \
	$(DOWNLOADS)/wabt/src/interp/interp-util.cc \
	$(DOWNLOADS)/wabt/src/interp/istream.cc \
	$(DOWNLOADS)/wabt/src/tools/wasm2c.cc

$(WASM2C): $(addsuffix .o,$(WASM2C_SRC))
	mkdir -p $(LIBDIR)/bin
	$(CXX) $(LDFLAGS) -o $@ $^

$(DOWNLOADS)/wabt/src/%.c.o: $(DOWNLOADS)/wabt/src/%.c $(DOWNLOADS)/wabt/include/wabt/config.h $(DOWNLOADS)/picosha2/picosha2.h
	$(CC) $(CFLAGS) -I$(DOWNLOADS)/wabt/include -I$(DOWNLOADS)/picosha2 -c -o $@ $<

$(DOWNLOADS)/wabt/src/%.cc.o: $(DOWNLOADS)/wabt/src/%.cc $(DOWNLOADS)/wabt/include/wabt/config.h $(DOWNLOADS)/picosha2/picosha2.h
	$(CXX) $(CXXFLAGS) -I$(DOWNLOADS)/wabt/include -I$(DOWNLOADS)/picosha2 -c -o $@ $<

$(DOWNLOADS)/wabt/include/wabt/config.h: $(DOWNLOADS)/wabt/src/config.h.in
	$(SED) \
		-e 's/#cmakedefine WABT_VERSION_STRING "@WABT_VERSION_STRING@"/#define WABT_VERSION_STRING "$(WABT_VERSION) (mkxp-z internal use)"/' \
		-e 's/#cmakedefine WABT_DEBUG @WABT_DEBUG@/#define WABT_DEBUG 0/' \
		-e 's/#cmakedefine01 HAVE_SNPRINTF/#define HAVE_SNPRINTF 1/' \
		-e 's/#cmakedefine01 HAVE_STRCASECMP/#define HAVE_STRCASECMP 1/' \
		-e 's/#cmakedefine01 COMPILER_IS_GNU/#define COMPILER_IS_GNU 1/' \
		-e 's/#cmakedefine01 \(\w*\)/#define \1 0/' \
		$< > $@

$(WASM2C_SRC) $(DOWNLOADS)/wabt/src/config.h.in &:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/WebAssembly/wabt $(DOWNLOADS)/wabt -b $(WABT_VERSION)

$(DOWNLOADS)/picosha2/picosha2.h:
	mkdir -p $(DOWNLOADS)/picosha2
	$(CURL) -L -o $@ https://raw.githubusercontent.com/okdshin/PicoSHA2/$(PICOSHA2_VERSION)/picosha2.h

# libyaml

$(LIBDIR)/usr/local/lib/libyaml.a: $(DOWNLOADS)/libyaml/Makefile
	cd $(DOWNLOADS)/libyaml && $(MAKE) install DESTDIR=$(LIBDIR)

$(DOWNLOADS)/libyaml/Makefile: $(DOWNLOADS)/libyaml/configure
	cd $(DOWNLOADS)/libyaml && ./configure --host $(TARGET) $(WASI_TOOLCHAIN) CFLAGS='$(WASI_CFLAGS)' CXXFLAGS='$(WASI_CXXFLAGS)' LDFLAGS='$(WASI_LDFLAGS)'

$(DOWNLOADS)/libyaml/configure: $(DOWNLOADS)/libyaml/configure.ac
	cd $(DOWNLOADS)/libyaml && $(AUTORECONF) -i

$(DOWNLOADS)/libyaml/configure.ac:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/yaml/libyaml $(DOWNLOADS)/libyaml -b $(LIBYAML_VERSION)

# zlib

$(LIBDIR)/usr/local/lib/libz.a: $(DOWNLOADS)/zlib/Makefile
	cd $(DOWNLOADS)/zlib && $(MAKE) install DESTDIR=$(LIBDIR)

$(DOWNLOADS)/zlib/Makefile: $(DOWNLOADS)/zlib/configure
	cd $(DOWNLOADS)/zlib && CHOST=linux $(WASI_TOOLCHAIN) CFLAGS='$(WASI_CFLAGS)' CXXFLAGS='$(WASI_CXXFLAGS)' LDFLAGS='$(WASI_LDFLAGS)' ./configure --static

$(DOWNLOADS)/zlib/configure:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/madler/zlib $(DOWNLOADS)/zlib -b v$(ZLIB_VERSION)
