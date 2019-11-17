CC = clang
CFLAGS = -std=c89 -W -Wall -Wextra -O0 -g -flto
CXX = clang++
CXXFLAGS = -std=c++17 -W -Wall -Wextra -O0 -g -flto -iquote external/CEmu/core
AR = llvm-ar

TEST_CFLAGS = -Oz
TEST_ITERATIONS = 10
CREDUCE_TIMEOUT = 10

ifeq ($(OS),Windows_NT)
	EXE = .exe
	RM = del /f 2>nul
	CHMOD_X =
	FASMG_TARGET = source/windows
	FASMG_BOOTSTRAP = fasmg.exe
else
	EXE =
	RM = rm -fr
	CHMOD_X = chmod +x "$1"
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		FASMG_TARGET = source/linux
		FASMG_BOOTSTRAP = fasmg
	endif
	ifeq ($(UNAME_S),Darwin)
		FASMG_TARGET = source/macos/x64
		FASMG_BOOTSTRAP = $(FASMG_TARGET)/fasmg
	endif
endif

RUNEZ80 = runez80$(EXE)
FASMG = external/fasmg-ez80/bin/fasmg$(EXE)

CSMITH = csmith
CSMITH_FLAGS = --no-hash-value-printf
GIT = git
PATCH = patch
UNZIP = unzip
WGET = wget

CEMUCORE = external/CEmu/core/libcemucore.a

check: rm-test.c check-one

rm-test.c:
	$(RM) test.c

check-one: test.c libcall.asm $(RUNEZ80)
	INCLUDE=external/fasmg-ez80 FASMG="$(FASMG) linker_script" CFLAGS="$(TEST_CFLAGS) $<" RUNEZ80=./$(RUNEZ80) ./runez80.sh
	$(RM) creduce
	mkdir -p creduce
	cp test.c creduce
	cd creduce && INCLUDE=$(CURDIR)\;$(CURDIR)/external/fasmg-ez80 FASMG="$(CURDIR)/$(FASMG) $(CURDIR)/linker_script" CFLAGS="$(TEST_CFLAGS) -iquote $(CURDIR) $<" RUNEZ80=$(CURDIR)/$(RUNEZ80) creduce --timeout $(CREDUCE_TIMEOUT) $(CURDIR)/runez80.sh $<

test.c:
	$(CSMITH) $(CSMITH_FLAGS) -o $@
	cp $@ $@.orig

$(RUNEZ80): runez80.cpp $(CEMUCORE)
	$(CXX) $(CXXFLAGS) $^ -o $@

libcall.asm: $(FASMG) libcall.gen
	$^ $@

$(CEMUCORE):
	@$(GIT) submodule update --init -- $(dir $(@D))
	$(MAKE) -C $(@D) CC="$(CC)" CFLAGS="$(CFLAGS)" AR="$(AR)" $(@F)

$(FASMG): external/fasmg-ez80/fasmg
	$</$(FASMG_BOOTSTRAP) $</$(FASMG_TARGET)/fasmg.asm $@
	$(call CHMOD_X,$@)

external/fasmg-ez80/fasmg: external/fasmg-ez80/fasmg.zip
	$(UNZIP) -o $< -d $@
	$(call CHMOD_X,$@/$(FASMG_BOOTSTRAP))
	$(PATCH) -p0 -d $@ <$@.patch

external/fasmg-ez80/fasmg.zip:
	@$(GIT) submodule update --init -- $(@D)
	$(WGET) https://flatassembler.net/fasmg.zip --output-document=$@

clean:
	$(MAKE) -C $(dir $(CEMUCORE)) clean
	$(RM) libcall.asm runz80 native.* test.c ez80.* $(RUNEZ80) $(FASMG)

distclean: clean
	$(RM) $(addprefix external/fasmg-ez80/, fasmg fasmg.zip)
	$(GIT) submodule deinit --all --force

.INTERMEDIATE: $(addpreifx external/fasmg-ez80/, fasmg fasmg.zip)
.PHONY: check check-one clean distclean
