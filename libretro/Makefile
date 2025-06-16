# Configure flags are based on the ones from https://github.com/ruby/ruby.wasm

RUBY_VERSION ?= 3.3.8
WABT_VERSION ?= 1.0.37
LIBYAML_VERSION ?= 0.2.5
ZLIB_VERSION ?= 1.3.1
PICOSHA2_VERSION ?= 27fcf6979298949e8a462e16d09a0351c18fcaf2
TARGET ?= wasm32-wasip1
WASI_SDK ?= /opt/wasi-sdk
WASM_OPT ?= wasm-opt
CTAGS ?= ctags
RUBY ?= ruby
AUTORECONF ?= autoreconf
CURL ?= curl
GIT ?= git
ZIP ?= zip
SED ?= sed
GREP ?= grep
NATIVE_CC ?= cc
NATIVE_CXX ?= c++
NATIVE_LD ?= ld
NATIVE_AR ?= ar
NATIVE_RANLIB ?= ranlib
NATIVE_CFLAGS ?=
NATIVE_CXXFLAGS ?=
NATIVE_LDFLAGS ?=
WASI_CFLAGS ?= -Oz -DNDEBUG
WASI_CXXFLAGS ?= -Oz -DNDEBUG
WASI_LDFLAGS ?=
WASM_OPT_FLAGS ?= -Oz
ZLIB_PATH ?=
LIBYAML_PATH ?=
OPENSSL_PATH ?=

BUILD_PREFIX := ${PWD}/build
OUTDIR := $(BUILD_PREFIX)/libretro-stage1
LIBDIR := $(BUILD_PREFIX)/lib
DOWNLOADS := $(BUILD_PREFIX)/downloads
WASM2C := $(LIBDIR)/bin/wasm2c
INDEXER := $(LIBDIR)/bin/indexer
CLONE := $(GIT) clone -q --depth 1
GITHUB := https://github.com
WASI_CC := $(WASI_SDK)/bin/clang
WASI_CXX := $(WASI_SDK)/bin/clang++
WASI_LD := $(WASI_SDK)/bin/clang
WASI_AR := $(WASI_SDK)/bin/llvm-ar
WASI_RANLIB := $(WASI_SDK)/bin/llvm-ranlib
NATIVE_TOOLCHAIN := CC=$(NATIVE_CC) CXX=$(NATIVE_CXX) LD=$(NATIVE_LD) AR=$(NATIVE_AR) RANLIB=$(NATIVE_RANLIB)
WASI_TOOLCHAIN := CC=$(WASI_CC) CXX=$(WASI_CXX) LD=$(WASI_LD) AR=$(WASI_AR) RANLIB=$(WASI_RANLIB)

ifeq ($(ZLIB_PATH),)
	ZLIB_PATH_FLAG =
else
	ZLIB_PATH_FLAG = --with-zlib-dir=$(ZLIB_PATH)
endif

ifeq ($(LIBYAML_PATH),)
	LIBYAML_PATH_FLAG =
else
	LIBYAML_PATH_FLAG = --with-libyaml-dir=$(LIBYAML_PATH)
endif

ifeq ($(OPENSSL_PATH),)
	OPENSSL_PATH_FLAG =
else
	OPENSSL_PATH_FLAG = --with-openssl-dir=$(OPENSSL_PATH)
endif

all default: ruby-dist ruby-bindings

ruby-dist: $(OUTDIR)/dist.zip $(OUTDIR)/ruby/mkxp-sandbox-ruby.h $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h $(OUTDIR)/ruby/mkxp-sandbox-ruby-func-types.h $(OUTDIR)/ruby/mkxp-sandbox-ruby-indices.h $(OUTDIR)/ruby/mkxp-sandbox-ruby_0.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_1.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_2.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_3.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_4.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_5.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_6.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_7.c

ruby-bindings: $(OUTDIR)/sandbox-bindgen/mkxp-sandbox-bindgen.cpp $(OUTDIR)/sandbox-bindgen/mkxp-sandbox-bindgen.h

clean: clean-ruby-dist clean-ruby-bindings
	rm -rf $(LIBDIR)/*
	rm -rf $(DOWNLOADS)/*

clean-ruby-dist:
	rm -f $(OUTDIR)/dist.zip
	rm -rf $(OUTDIR)/ruby

clean-ruby-bindings:
	rm -rf $(OUTDIR)/sandbox-bindgen
	rm -f $(LIBDIR)/tags.c
	rm -f $(LIBDIR)/tags

# Ruby (targets WASI)

$(OUTDIR)/ruby/mkxp-sandbox-ruby.h $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h $(OUTDIR)/ruby/mkxp-sandbox-ruby-func-types.h $(OUTDIR)/ruby/mkxp-sandbox-ruby_0.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_1.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_2.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_3.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_4.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_5.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_6.c $(OUTDIR)/ruby/mkxp-sandbox-ruby_7.c &: $(WASM2C) $(LIBDIR)/Dist/bin/ruby
	mkdir -p $(OUTDIR)/ruby
	$(WASM2C) $(LIBDIR)/Dist/bin/ruby -n ruby --num-outputs=8 -o $(OUTDIR)/ruby/mkxp-sandbox-ruby.c
	# Find all the function types declared using `FUNC_TYPE_DECL_EXTERN_T` and redeclare them as macros to fix the "initializer element is not constant" compilation error that occurs when using GCC versions earlier than 8.1.0
	rm -f $(OUTDIR)/ruby/mkxp-sandbox-ruby-func-types.h
	for func_type in $$($(GREP) -r 'FUNC_TYPE_DECL_EXTERN_T *([^()]*)' $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h | $(GREP) -v '# *define' | $(SED) -e 's/FUNC_TYPE_DECL_EXTERN_T *( *\(.*\) *).*/\1/'); do \
		echo "#define $$func_type \"$$(od -N 32 -x /dev/urandom | cut -b 9- | tr -d ' \n' | $(SED) -e 's/.\{2\}/\\x&/g')\"" >> $(OUTDIR)/ruby/mkxp-sandbox-ruby-func-types.h; \
	done
	$(SED) -i 's/ *# *define * FUNC_TYPE_DECL_EXTERN_T *([^()]).*/#define FUNC_TYPE_DECL_EXTERN_T(x)/g' $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h
	$(SED) -i 's/ *# *define * FUNC_TYPE_EXTERN_T *([^()]).*/#define FUNC_TYPE_EXTERN_T(x) const char _mkxp_unused_##x[]/g' $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h
	$(SED) -i 's/ *# *define * FUNC_TYPE_T *([^()]).*/#define FUNC_TYPE_T(x) static const char _mkxp_unused_##x[]/g' $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h
	$(SED) -i 's/__has_builtin *([^()]*)/0/g' $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h
	$(SED) -i 's/#error "Missing implementation of __builtin_add_overflow or _addcarry_u64"/return __builtin_add_overflow(a, b, resptr);/g' $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h

$(OUTDIR)/ruby/mkxp-sandbox-ruby-indices.h: $(INDEXER) $(LIBDIR)/Dist/bin/ruby
	mkdir -p $(OUTDIR)/ruby
	$(INDEXER) $(LIBDIR)/Dist/bin/ruby > $(OUTDIR)/ruby/mkxp-sandbox-ruby-indices.h

$(OUTDIR)/dist.zip: $(LIBDIR)/Dist/bin/ruby $(DOWNLOADS)/ruby/BSDL $(DOWNLOADS)/ruby/COPYING $(DOWNLOADS)/ruby/COPYING.ja
	rm -rf $(LIBDIR)/_Dist
	cp -r $(LIBDIR)/Dist $(LIBDIR)/_Dist
	rm $(LIBDIR)/_Dist/bin/ruby
	rm $(LIBDIR)/_Dist/lib/libruby-static.a
	rm -r $(LIBDIR)/_Dist/include
	rm -r $(LIBDIR)/_Dist/share
	rm -r $(LIBDIR)/_Dist/lib/ruby/$(shell $(RUBY) -e 'puts "$(RUBY_VERSION)".split(".")[..1].join(".")').0/bundler
	rm -r $(LIBDIR)/_Dist/lib/ruby/$(shell $(RUBY) -e 'puts "$(RUBY_VERSION)".split(".")[..1].join(".")').0/rdoc
	rm -r $(LIBDIR)/_Dist/lib/ruby/$(shell $(RUBY) -e 'puts "$(RUBY_VERSION)".split(".")[..1].join(".")').0/rubygems
	echo '# This is a Ruby script that does nothing. We make Ruby load this file on startup just because Ruby needs to load a script on startup for some reason.' > $(LIBDIR)/_Dist/bin/mkxp-z
	cp $(DOWNLOADS)/ruby/BSDL $(LIBDIR)/_Dist/lib/ruby
	cp $(DOWNLOADS)/ruby/COPYING $(LIBDIR)/_Dist/lib/ruby
	cp $(DOWNLOADS)/ruby/COPYING.ja $(LIBDIR)/_Dist/lib/ruby
	rm -f $(OUTDIR)/dist.zip
	cd $(LIBDIR)/_Dist && $(ZIP) -r $(OUTDIR)/dist.zip *
	rm -r $(LIBDIR)/_Dist

$(LIBDIR)/Dist/bin/ruby: $(DOWNLOADS)/ruby/Makefile ruby-bindings.h
	cd $(DOWNLOADS)/ruby && $(MAKE) install DESTDIR=$(LIBDIR)

$(OUTDIR)/sandbox-bindgen/mkxp-sandbox-bindgen.cpp $(OUTDIR)/sandbox-bindgen/mkxp-sandbox-bindgen.h &: sandbox-bindgen.rb $(LIBDIR)/tags
	mkdir -p $(OUTDIR)/sandbox-bindgen
	cd $(LIBDIR) && $(RUBY) ${PWD}/sandbox-bindgen.rb
	mv $(LIBDIR)/mkxp-sandbox-bindgen.h $(OUTDIR)/sandbox-bindgen
	mv $(LIBDIR)/mkxp-sandbox-bindgen.cpp $(OUTDIR)/sandbox-bindgen

$(LIBDIR)/tags: $(LIBDIR)/tags.c
	$(CTAGS) --fields=kS --kinds-c=epx -o $(LIBDIR)/tags $(LIBDIR)/tags.c

$(LIBDIR)/tags.c: $(DOWNLOADS)/ruby/.ext/include/$(TARGET)/ruby/config.h
	mkdir -p $(LIBDIR)
	echo '#include <ruby.h>' | $(WASI_CC) -E -I$(DOWNLOADS)/ruby/include -I$(DOWNLOADS)/ruby/.ext/include/$(TARGET) -o $(LIBDIR)/tags.c -

$(DOWNLOADS)/ruby/Makefile $(DOWNLOADS)/ruby/.ext/include/$(TARGET)/ruby/config.h $(DOWNLOADS)/ruby/BSDL $(DOWNLOADS)/ruby/COPYING $(DOWNLOADS)/ruby/COPYING.ja &: $(DOWNLOADS)/ruby/configure $(LIBDIR)/usr/local/lib/libyaml.a $(LIBDIR)/usr/local/lib/libz.a
	cd $(DOWNLOADS)/ruby && ./configure \
		--prefix=/Dist \
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
		$(shell $(RUBY) -e 'puts "ac_cv_func_dlopen=no" if "$(RUBY_VERSION)".split(".")[..1].join(".").to_f < 3.3') \
		--disable-rubygems \
		--disable-install-doc

$(DOWNLOADS)/ruby/configure: $(DOWNLOADS)/ruby/configure.ac
	cd $(DOWNLOADS)/ruby && $(RUBY) tool/downloader.rb -d tool -e gnu config.guess config.sub
	cd $(DOWNLOADS)/ruby && $(AUTORECONF) -i

$(DOWNLOADS)/ruby/configure.ac:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/ruby/ruby $(DOWNLOADS)/ruby -b v$(subst .,_,$(RUBY_VERSION))
	cd $(DOWNLOADS)/ruby && $(GIT) apply ${PWD}/ruby-stack-pointer.patch
	cd $(DOWNLOADS)/ruby && $(GIT) apply ${PWD}/ruby-jump-buffer.patch
	cd $(DOWNLOADS)/ruby && $(GIT) apply ${PWD}/ruby-compat.patch
	echo '#include "${PWD}/ruby-bindings.h"' >> $(DOWNLOADS)/ruby/gc.c

# WABT (targets the build machine)

WABT_SRC := \
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

WASM2C_SRC := $(WABT_SRC) $(DOWNLOADS)/wabt/src/tools/wasm2c.cc

$(WASM2C): $(addsuffix .o,$(WASM2C_SRC))
	mkdir -p $(LIBDIR)/bin
	$(NATIVE_CXX) $(NATIVE_LDFLAGS) -o $@ $^

$(INDEXER): $(addsuffix .o,$(WABT_SRC)) $(DOWNLOADS)/wabt/src/indexer.o
	mkdir -p $(LIBDIR)/bin
	$(NATIVE_CXX) $(NATIVE_LDFLAGS) -o $@ $^

$(DOWNLOADS)/wabt/src/%.c.o: $(DOWNLOADS)/wabt/src/%.c $(DOWNLOADS)/wabt/include/wabt/config.h $(DOWNLOADS)/picosha2/picosha2.h
	$(NATIVE_CC) $(NATIVE_CFLAGS) -I$(DOWNLOADS)/wabt/include -I$(DOWNLOADS)/picosha2 -c -o $@ $<

$(DOWNLOADS)/wabt/src/%.cc.o: $(DOWNLOADS)/wabt/src/%.cc $(DOWNLOADS)/wabt/include/wabt/config.h $(DOWNLOADS)/picosha2/picosha2.h
	$(NATIVE_CXX) $(NATIVE_CXXFLAGS) -I$(DOWNLOADS)/wabt/include -I$(DOWNLOADS)/picosha2 -c -o $@ $<

$(DOWNLOADS)/wabt/src/indexer.o: indexer.cpp $(DOWNLOADS)/wabt/include/wabt/config.h
	$(NATIVE_CXX) $(NATIVE_CXXFLAGS) -I$(DOWNLOADS)/wabt/include -c -o $@ $<

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

# libyaml (targets WASI)

$(LIBDIR)/usr/local/lib/libyaml.a: $(DOWNLOADS)/libyaml/Makefile
	cd $(DOWNLOADS)/libyaml && $(MAKE) install DESTDIR=$(LIBDIR)

$(DOWNLOADS)/libyaml/Makefile: $(DOWNLOADS)/libyaml/configure
	cd $(DOWNLOADS)/libyaml && ./configure --host $(TARGET) $(WASI_TOOLCHAIN) CFLAGS='$(WASI_CFLAGS)' CXXFLAGS='$(WASI_CXXFLAGS)' LDFLAGS='$(WASI_LDFLAGS)'

$(DOWNLOADS)/libyaml/configure: $(DOWNLOADS)/libyaml/configure.ac
	cd $(DOWNLOADS)/libyaml && $(AUTORECONF) -i

$(DOWNLOADS)/libyaml/configure.ac:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/yaml/libyaml $(DOWNLOADS)/libyaml -b $(LIBYAML_VERSION)

# zlib (targets WASI)

$(LIBDIR)/usr/local/lib/libz.a: $(DOWNLOADS)/zlib/Makefile
	cd $(DOWNLOADS)/zlib && $(MAKE) install DESTDIR=$(LIBDIR)

$(DOWNLOADS)/zlib/Makefile: $(DOWNLOADS)/zlib/configure
	cd $(DOWNLOADS)/zlib && CHOST=linux $(WASI_TOOLCHAIN) CFLAGS='$(WASI_CFLAGS)' CXXFLAGS='$(WASI_CXXFLAGS)' LDFLAGS='$(WASI_LDFLAGS)' ./configure --static

$(DOWNLOADS)/zlib/configure:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/madler/zlib $(DOWNLOADS)/zlib -b v$(ZLIB_VERSION)
