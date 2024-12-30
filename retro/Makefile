# Configure flags are based on the ones from https://github.com/ruby/ruby.wasm

RUBY_VERSION ?= 3.3

CC ?= gcc
CXX ?= g++
WASI_SDK ?= /opt/wasi-sdk
WASM_OPT ?= wasm-opt
WASM2C ?= wasm2c

WASI_CC := $(WASI_SDK)/bin/clang
WASI_CXX := $(WASI_SDK)/bin/clang++
WASI_LD := $(WASI_SDK)/bin/clang
WASI_AR := $(WASI_SDK)/bin/llvm-ar
WASI_RANLIB := $(WASI_SDK)/bin/llvm-ranlib
WASI_TOOLCHAIN := CC=$(WASI_CC) CXX=$(WASI_CXX) LD=$(WASI_LD) AR=$(WASI_AR) RANLIB=$(WASI_RANLIB)

NPROC := $(shell nproc)
BUILD_PREFIX := ${PWD}/build
OUTDIR := $(BUILD_PREFIX)/retro-phase1
LIBDIR := $(BUILD_PREFIX)/lib
DOWNLOADS := $(BUILD_PREFIX)/downloads
CURL := curl -s
GIT := git
CLONE := $(GIT) clone -q --depth 1
GITHUB := https://github.com

default: $(OUTDIR)/mkxp-retro-dist.zip.c $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby.h $(OUTDIR)/wasm2c/wasm-rt-impl.c $(OUTDIR)/wasm2c/wasm-rt-mem-impl.c $(OUTDIR)/libretro.h

$(OUTDIR)/libretro.h:
	mkdir -p $(OUTDIR); \
	$(CURL) -Lo $(OUTDIR)/libretro.h https://raw.githubusercontent.com/libretro/libretro-common/80d7a25029b9c60f3c71cda49108f4545897d195/include/libretro.h # 2024-12-22

$(OUTDIR)/wasm2c/wasm-rt-impl.c $(OUTDIR)/wasm2c/wasm-rt-mem-impl.c:
	mkdir -p $(DOWNLOADS); \
	$(CLONE) $(GITHUB)/WebAssembly/wabt $(DOWNLOADS)/wabt -b 1.0.36; \
	cp -r $(DOWNLOADS)/wabt/wasm2c $(OUTDIR)

# Ruby

$(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby.h: $(LIBDIR)/ruby.wasm
	mkdir -p $(OUTDIR)/mkxp-retro-ruby; \
	$(WASM2C) $(LIBDIR)/ruby.wasm -n ruby --num-outputs=8 -o $(OUTDIR)/mkxp-retro-ruby/mkxp-retro-ruby.c

$(LIBDIR)/ruby.wasm $(OUTDIR)/mkxp-retro-dist.zip.c: $(DOWNLOADS)/ruby/Makefile
	mkdir -p $(OUTDIR); \
	cd $(DOWNLOADS)/ruby; \
	make -j$(NPROC); \
	make install DESTDIR=$(OUTDIR); \
	mv $(OUTDIR)/mkxp-retro-dist/bin/ruby $(LIBDIR)/ruby.wasm; \
	cd $(OUTDIR)/mkxp-retro-dist; \
	zip -r $(OUTDIR)/mkxp-retro-dist.zip *; \
	rm -r $(OUTDIR)/mkxp-retro-dist; \
	cd $(OUTDIR); \
	xxd -i mkxp-retro-dist.zip mkxp-retro-dist.zip.c; \
	rm mkxp-retro-dist.zip

$(DOWNLOADS)/ruby/Makefile: $(DOWNLOADS)/ruby/configure $(LIBDIR)/bin/ruby $(LIBDIR)/usr/local/lib/libyaml.a $(LIBDIR)/usr/local/lib/libz.a $(LIBDIR)/usr/local/lib/libssl.a
	cd $(DOWNLOADS)/ruby; \
	./configure \
	--prefix=/mkxp-retro-dist \
	--host wasm32-wasip1 \
	--build $(shell $(LIBDIR)/bin/ruby -e 'puts(RbConfig::CONFIG["host"])') \
	--with-static-linked-ext \
	--with-ext=bigdecimal,cgi/escape,continuation,coverage,date,dbm,digest/bubblebabble,digest,digest/md5,digest/rmd160,digest/sha1,digest/sha2,etc,fcntl,fiber,gdbm,json,json/generator,json/parser,nkf,objspace,pathname,psych,racc/cparse,rbconfig/sizeof,ripper,stringio,strscan,monitor,zlib,openssl \
	--with-libyaml-dir=$(LIBDIR)/usr/local \
	--with-zlib-dir=$(LIBDIR)/usr/local \
	--with-openssl-dir=$(LIBDIR)/usr/local \
	--with-baseruby=$(LIBDIR)/bin/ruby \
	WASMOPT=$(WASM_OPT) \
	WASI_SDK_PATH=$(WASI_SDK) \
	$(WASI_TOOLCHAIN) \
	LDFLAGS='-Wl,--export-dynamic,--stack-first,-z,stack-size=16777216' \
	XCFLAGS='-DWASM_SETJMP_STACK_BUFFER_SIZE=24576 -DWASM_FIBER_STACK_BUFFER_SIZE=24576 -DWASM_SCAN_STACK_BUFFER_SIZE=24576' \
	debugflags=-g \
	cppflags= \
	wasmoptflags='-O3 -g --pass-arg=asyncify-ignore-imports' \
	$(shell $(LIBDIR)/bin/ruby -e 'puts "ac_cv_func_dlopen=no" if RUBY_VERSION.start_with? "3.2."') \
	--disable-install-doc

$(DOWNLOADS)/ruby/configure: $(DOWNLOADS)/baseruby/configure
	cp -r $(DOWNLOADS)/baseruby $(DOWNLOADS)/ruby; \
	echo '#include "${PWD}/extra-ruby-bindings.h"' >> $(DOWNLOADS)/ruby/main.c

$(LIBDIR)/bin/ruby: $(DOWNLOADS)/baseruby/Makefile
	cd $(DOWNLOADS)/baseruby; \
	make -j$(NPROC); \
	make install

$(DOWNLOADS)/baseruby/Makefile: $(DOWNLOADS)/baseruby/configure
	cd $(DOWNLOADS)/baseruby; \
	./configure --prefix=$(LIBDIR) --disable-install-doc

$(DOWNLOADS)/baseruby/configure: $(DOWNLOADS)/baseruby/configure.ac
	cd $(DOWNLOADS)/baseruby; \
	ruby tool/downloader.rb -d tool -e gnu config.guess config.sub ; \
	autoreconf -i

$(DOWNLOADS)/baseruby/configure.ac:
	if ! ruby -e 'exit 1 if $(RUBY_VERSION) < 3.2'; then echo -e '\e[91m[ERROR] Ruby version must be at least 3.2 because earlier versions do not have WebAssembly support\e[0m'; exit 1; fi; \
	mkdir -p $(DOWNLOADS); \
	$(CLONE) $(GITHUB)/ruby/ruby $(DOWNLOADS)/baseruby -b ruby_$(shell echo $(RUBY_VERSION) | sed -e 's/\./_/g')

# libyaml

$(LIBDIR)/usr/local/lib/libyaml.a: $(DOWNLOADS)/libyaml/Makefile
	cd $(DOWNLOADS)/libyaml; \
	make -j$(NPROC); \
	make install DESTDIR=$(LIBDIR)

$(DOWNLOADS)/libyaml/Makefile: $(DOWNLOADS)/libyaml/configure
	cd $(DOWNLOADS)/libyaml; \
	./configure --host wasm32-wasip1 $(WASI_TOOLCHAIN)

$(DOWNLOADS)/libyaml/configure: $(DOWNLOADS)/libyaml/configure.ac
	cd $(DOWNLOADS)/libyaml; \
	autoreconf -i

$(DOWNLOADS)/libyaml/configure.ac:
	mkdir -p $(DOWNLOADS); \
	$(CLONE) $(GITHUB)/yaml/libyaml $(DOWNLOADS)/libyaml -b 0.2.5

# zlib

$(LIBDIR)/usr/local/lib/libz.a: $(DOWNLOADS)/zlib/Makefile
	cd $(DOWNLOADS)/zlib; \
	make -j$(NPROC); \
	make install DESTDIR=$(LIBDIR)

$(DOWNLOADS)/zlib/Makefile: $(DOWNLOADS)/zlib/configure
	cd $(DOWNLOADS)/zlib; \
	env CHOST=linux $(WASI_TOOLCHAIN) ./configure --static

$(DOWNLOADS)/zlib/configure:
	mkdir -p $(DOWNLOADS); \
	$(CLONE) $(GITHUB)/madler/zlib $(DOWNLOADS)/zlib -b v1.3.1

# OpenSSL

$(LIBDIR)/usr/local/lib/libssl.a: $(DOWNLOADS)/openssl/Makefile
	cd $(DOWNLOADS)/openssl; \
	make -j$(NPROC); \
	make install_dev DESTDIR=$(LIBDIR)

$(DOWNLOADS)/openssl/Makefile: $(DOWNLOADS)/openssl/Configure
	cd $(DOWNLOADS)/openssl; \
	./Configure \
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
	mkdir -p $(DOWNLOADS); \
	$(CLONE) $(GITHUB)/openssl/openssl $(DOWNLOADS)/openssl -b openssl-3.2.0
