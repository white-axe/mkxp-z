# Configure flags are based on the ones from https://github.com/ruby/ruby.wasm

RUBY_VERSION ?= 3.3
LIBRETRO_REF ?= 80d7a25029b9c60f3c71cda49108f4545897d195
LIBYAML_VERSION ?= 0.2.5
ZLIB_VERSION ?= 1.3.1
OPENSSL_VERSION ?= 3.2.0
WASM_RT_VERSION ?= 1.0.36
P7ZIP_VERSION ?= 17.06
TARGET ?= wasm32-wasip1
WASI_SDK ?= /opt/wasi-sdk
WASM_OPT ?= wasm-opt
WASM2C ?= wasm2c
CTAGS ?= ctags
AUTORECONF ?= autoreconf
CURL ?= curl
GIT ?= git
XXD ?= xxd
CC ?= cc
CXX ?= c++
LD ?= ld
AR ?= ar
RANLIB ?= ranlib
CFLAGS ?=
CXXFLAGS ?=
LDFLAGS ?=
WASI_CFLAGS ?= -O3
WASI_CXXFLAGS ?= -O3
WASI_LDFLAGS ?=

BUILD_PREFIX := ${PWD}/build
OUTDIR := $(BUILD_PREFIX)/retro-phase1
LIBDIR := $(BUILD_PREFIX)/lib
DOWNLOADS := $(BUILD_PREFIX)/downloads
RUBY := $(LIBDIR)/bin/ruby
P7ZIP := $(DOWNLOADS)/p7zip/bin/7z
CLONE := $(GIT) clone -q --depth 1
GITHUB := https://github.com
WASI_CC := $(WASI_SDK)/bin/clang
WASI_CXX := $(WASI_SDK)/bin/clang++
WASI_LD := $(WASI_SDK)/bin/clang
WASI_AR := $(WASI_SDK)/bin/llvm-ar
WASI_RANLIB := $(WASI_SDK)/bin/llvm-ranlib
NATIVE_TOOLCHAIN := CC=$(CC) CXX=$(CXX) LD=$(LD) AR=$(AR) RANLIB=$(RANLIB)
WASI_TOOLCHAIN := CC=$(WASI_CC) CXX=$(WASI_CXX) LD=$(WASI_LD) AR=$(WASI_AR) RANLIB=$(WASI_RANLIB)

all default: deps ruby-dist ruby-bindings

ruby-dist: $(OUTDIR)/mkxp-retro-dist.zip.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby.h $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby-impl.h $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_0.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_1.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_2.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_3.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_4.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_5.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_6.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_7.c

ruby-bindings: $(OUTDIR)/mkxp-sandbox-bindgen.cpp $(OUTDIR)/mkxp-sandbox-bindgen.h

deps: $(OUTDIR)/libretro.h $(OUTDIR)/wasm2c/wasm-rt.h $(OUTDIR)/wasm2c/wasm-rt-impl.c $(OUTDIR)/wasm2c/wasm-rt-mem-impl.c

clean: clean-ruby-dist clean-ruby-bindings clean-deps
	rm -rf $(LIBDIR)/*
	rm -rf $(DOWNLOADS)/*

clean-ruby-dist:
	rm -f $(OUTDIR)/mkxp-retro-dist.zip.c
	rm -rf $(LIBDIR)/mkxp-retro-ruby

clean-ruby-bindings:
	rm -f $(OUTDIR)/mkxp-sandbox-bindgen.cpp
	rm -f $(OUTDIR)/mkxp-sandbox-bindgen.h
	rm -f $(LIBDIR)/tags.c
	rm -f $(LIBDIR)/tags

clean-deps:
	rm -f $(OUTDIR)/libretro.h
	rm -rf $(OUTDIR)/wasm2c

$(OUTDIR)/libretro.h:
	mkdir -p $(OUTDIR)
	$(CURL) -s -L -o $(OUTDIR)/libretro.h https://raw.githubusercontent.com/libretro/libretro-common/$(LIBRETRO_REF)/include/libretro.h

$(OUTDIR)/wasm2c/wasm-rt.h $(OUTDIR)/wasm2c/wasm-rt-impl.c $(OUTDIR)/wasm2c/wasm-rt-mem-impl.c &:
	mkdir -p $(OUTDIR)
	$(CLONE) $(GITHUB)/WebAssembly/wabt $(DOWNLOADS)/wabt -b $(WASM_RT_VERSION)
	cp -r $(DOWNLOADS)/wabt/wasm2c $(OUTDIR)

# Cross Ruby (targets WASI)

$(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby.h $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby-impl.h $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_0.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_1.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_2.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_3.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_4.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_5.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_6.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby_7.c &: $(LIBDIR)/mkxp-retro-dist/bin/ruby
	mkdir -p $(OUTDIR)/mkxp-retro-ruby
	$(WASM2C) $(LIBDIR)/mkxp-retro-dist/bin/ruby -n ruby --num-outputs=8 -o $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby.c

$(OUTDIR)/mkxp-retro-dist.zip.c: $(LIBDIR)/mkxp-retro-dist.zip
	mkdir -p $(OUTDIR)
	cd $(LIBDIR) && $(XXD) -i mkxp-retro-dist.zip $(OUTDIR)/mkxp-retro-dist.zip.c

$(LIBDIR)/mkxp-retro-dist.zip: $(LIBDIR)/mkxp-retro-dist/bin/ruby $(P7ZIP)
	rm -rf $(LIBDIR)/_mkxp-retro-dist
	cp -r $(LIBDIR)/mkxp-retro-dist $(LIBDIR)/_mkxp-retro-dist
	rm $(LIBDIR)/_mkxp-retro-dist/bin/ruby
	rm $(LIBDIR)/_mkxp-retro-dist/lib/libruby-static.a
	rm -r $(LIBDIR)/_mkxp-retro-dist/include
	rm -r $(LIBDIR)/_mkxp-retro-dist/share
	rm -r $(LIBDIR)/_mkxp-retro-dist/lib/ruby/gems/$(RUBY_VERSION).0/cache/*
	echo '# This is a Ruby script that does nothing. We make Ruby load this file on startup just because Ruby needs to load a script on startup for some reason.' > $(LIBDIR)/_mkxp-retro-dist/bin/mkxp-z
	rm -f $(LIBDIR)/mkxp-retro-dist.zip
	cd $(LIBDIR)/_mkxp-retro-dist && $(P7ZIP) a -bb3 -mm=zstd -mx=19 $(LIBDIR)/mkxp-retro-dist.zip *
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

$(DOWNLOADS)/crossruby/Makefile $(DOWNLOADS)/crossruby/.ext/include/$(TARGET)/ruby/config.h &: $(DOWNLOADS)/crossruby/configure $(RUBY) $(LIBDIR)/usr/local/lib/libyaml.a $(LIBDIR)/usr/local/lib/libz.a $(LIBDIR)/usr/local/lib/libssl.a
	cd $(DOWNLOADS)/crossruby && ./configure \
		--prefix=/mkxp-retro-dist \
		--host $(TARGET) \
		--build $(shell $(RUBY) -e 'puts(RbConfig::CONFIG["host"])') \
		--with-static-linked-ext \
		--with-ext=bigdecimal,cgi/escape,continuation,coverage,date,dbm,digest/bubblebabble,digest,digest/md5,digest/rmd160,digest/sha1,digest/sha2,etc,fcntl,fiber,gdbm,json,json/generator,json/parser,nkf,objspace,pathname,psych,racc/cparse,rbconfig/sizeof,ripper,stringio,strscan,monitor,zlib,openssl \
		--with-libyaml-dir=$(LIBDIR)/usr/local \
		--with-zlib-dir=$(LIBDIR)/usr/local \
		--with-openssl-dir=$(LIBDIR)/usr/local \
		--with-baseruby=$(RUBY) \
		WASMOPT=$(WASM_OPT) \
		WASI_SDK_PATH=$(WASI_SDK) \
		$(WASI_TOOLCHAIN) \
		LDFLAGS='-Wl,--export-dynamic,--stack-first,-z,stack-size=16777216 $(WASI_LDFLAGS)' \
		XCFLAGS='-DWASM_SETJMP_STACK_BUFFER_SIZE=24576 -DWASM_FIBER_STACK_BUFFER_SIZE=24576 -DWASM_SCAN_STACK_BUFFER_SIZE=24576 $(WASI_CFLAGS)' \
		debugflags=-g \
		cppflags='$(WASI_CXXFLAGS)' \
		wasmoptflags='-O4 -g --pass-arg=asyncify-ignore-imports' \
		$(shell $(RUBY) -e 'puts "ac_cv_func_dlopen=no" if RUBY_VERSION.split(".")[..1].join(".").to_f < 3.3') \
		--disable-install-doc

$(DOWNLOADS)/crossruby/configure: $(DOWNLOADS)/crossruby/configure.ac $(RUBY)
	cd $(DOWNLOADS)/crossruby && $(RUBY) tool/downloader.rb -d tool -e gnu config.guess config.sub
	cd $(DOWNLOADS)/crossruby && $(AUTORECONF) -i

$(DOWNLOADS)/crossruby/configure.ac:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/ruby/ruby $(DOWNLOADS)/crossruby -b ruby_$(shell echo $(RUBY_VERSION) | sed -e 's/\./_/g')
	echo '#include "${PWD}/ruby-bindings.h"' >> $(DOWNLOADS)/crossruby/main.c

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
	$(CLONE) $(GITHUB)/ruby/ruby $(DOWNLOADS)/baseruby -b ruby_$(shell echo $(RUBY_VERSION) | sed -e 's/\./_/g')

# p7zip

$(P7ZIP): $(DOWNLOADS)/p7zip/makefile
	cd $(DOWNLOADS)/p7zip && $(MAKE) 7z $(NATIVE_TOOLCHAIN)

$(DOWNLOADS)/p7zip/makefile:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/p7zip-project/p7zip $(DOWNLOADS)/p7zip -b v$(P7ZIP_VERSION)

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

# OpenSSL

$(LIBDIR)/usr/local/lib/libssl.a: $(DOWNLOADS)/openssl/Makefile
	cd $(DOWNLOADS)/openssl && $(MAKE) install_dev DESTDIR=$(LIBDIR)

$(DOWNLOADS)/openssl/Makefile: $(DOWNLOADS)/openssl/Configure
	cd $(DOWNLOADS)/openssl && CFLAGS='$(WASI_CFLAGS)' CXXFLAGS='$(WASI_CXXFLAGS)' LDFLAGS='$(WASI_LDFLAGS)' ./Configure \
		gcc \
		-static \
		-no-asm \
		-no-threads \
		-no-afalgeng \
		-no-ui-console \
		-no-tests \
		-no-sock \
		-no-dgram \
		--libdir=lib \
		-Wl,--allow-undefined \
		-D_WASI_EMULATED_SIGNAL \
		-D_WASI_EMULATED_PROCESS_CLOCKS \
		-D_WASI_EMULATED_MMAN \
		-D_WASI_EMULATED_GETPID \
		-DNO_CHMOD \
		-DHAVE_FORK=0 \
		$(WASI_TOOLCHAIN)

$(DOWNLOADS)/openssl/Configure:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/openssl/openssl $(DOWNLOADS)/openssl -b openssl-$(OPENSSL_VERSION)
