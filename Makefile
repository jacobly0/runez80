ifeq ($(OS),Windows_NT)
	EXE = .exe
	RM = del /f 2>nul
	MV = ren
	CHMOD_X =
	FASMG_TARGET = source/windows
	FASMG_BOOTSTRAP = fasmg.exe
else
	EXE =
	RM = rm -fr
	MV = mv
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

FLAGS = -O3 -g0 -flto -DMULTITHREAD
FLAGSD = -O0 -g3 -flto -DMULTITHREAD
CC = clang
CFLAGS = -std=gnu11 -W -Wall -Wextra $(FLAGS)
CXX = clang++
CXXFLAGS = -std=c++17 -W -Wall -Wextra $(FLAGS) -iquote external/CEmu/core
CXXFLAGSD = -std=c++17 -W -Wall -Wextra $(FLAGSD) -iquote external/CEmu/core
AR = llvm-ar

FASMG = external/fasmg-ez80/bin/fasmg$(EXE)

TEST_CFLAGS = -Oz
TEST_ITERATIONS = 10
NATIVE_TIMEOUT = 10
RUNEZ80 = runez80$(EXE)
RUNEZ80D = runez80d$(EXE)

CVISE=cvise
CVISE_TIMEOUT = 30
CVISE_FLAGS = --n 8 --timeout $(CVISE_TIMEOUT) --renaming

CSMITH = csmith
CSMITH_FLAGS = --no-bitfields --no-argc --no-hash-value-printf

GIT = git
UNZIP = unzip
WGET = wget

CEMUCORE = external/CEmu/core/libcemucore.a

TEST_CFLAGS_ALL = $(TEST_CFLAGS) -I$(abspath $(lastword $(wildcard $(addprefix $(dir $(realpath $(shell which $(CSMITH))))/../,csmith-* runtime)))) -iquote $(CURDIR)

check: rm-test.c recheck

rm-test.c:
ifneq ($(wildcard test.c),)
	$(MV) test.c test.prev.c
endif

recheck: test.c libcall.asm $(RUNEZ80) $(RUNEZ80D)
	INCLUDE=external/fasmg-ez80 FASMG="$(FASMG) linker_script" CFLAGS="$(TEST_CFLAGS_ALL) $<" NATIVE_TIMEOUT=$(NATIVE_TIMEOUT) RUNEZ80=./$(RUNEZ80) time ./runez80.sh
	$(RM) cvise
	mkdir -p cvise
	ez80-clang -E $(TEST_CFLAGS_ALL) $< -o cvise/$<
	cd cvise && INCLUDE=$(CURDIR)\;$(CURDIR)/external/fasmg-ez80 FASMG="$(CURDIR)/$(FASMG) $(CURDIR)/linker_script" CFLAGS="$(TEST_CFLAGS_ALL) $<" NATIVE_TIMEOUT=$(NATIVE_TIMEOUT) RUNEZ80=$(CURDIR)/$(RUNEZ80) $(CVISE) $(CVISE_FLAGS) $(CURDIR)/runez80.sh $< || exit 0

test.c:
	$(CSMITH) $(CSMITH_FLAGS) -o $@
	cp $@ $@.orig

$(RUNEZ80): runez80.cpp $(CEMUCORE)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(RUNEZ80D): runez80.cpp $(CEMUCORE)
	$(CXX) $(CXXFLAGSD) $^ -o $@

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

external/fasmg-ez80/fasmg.zip:
	@$(GIT) submodule update --init -- $(@D)
	$(WGET) https://flatassembler.net/fasmg.zip --output-document=$@

clean:
ifneq ($(wildcard $(dir $(CEMUCORE))),)
	$(MAKE) -C $(dir $(CEMUCORE)) clean
endif
	$(RM) libcall.asm runz80 native.* test.c ez80.* $(RUNEZ80) $(FASMG)

distclean: clean
	$(RM) $(addprefix external/fasmg-ez80/, fasmg fasmg.zip)
	$(GIT) submodule deinit --all --force

.INTERMEDIATE: $(addpreifx external/fasmg-ez80/, fasmg fasmg.zip)
.PHONY: check recheck clean distclean
