all:
.SILENT:
.SECONDARY:
.PREFIXES=
PRECMD=echo "  $@" ; mkdir -p $(@D) ;

#-------------------------- Per-host configuration. -----------------------------------

ifeq ($(USER),andy)

WRAPPERS:=pulse ray
RAYLIB_SDK:=../thirdparty/raylib-5.5_linux_amd64
RAYLIB_WEB_SDK:=../thirdparty/raylib-5.5_webassembly
CC:=gcc -c -MMD -O3 -Isrc -Werror -Wimplicit -I$(RAYLIB_SDK)/include
LD:=gcc
LDPOST:=
LDPOST_pulse:=-lpulse-simple
LDPOST_ray:=$(RAYLIB_SDK)/lib/libraylib.a -lm
AR:=ar
LIBSTATIC:=out/libtfss.a

else

# When you land here, copy the "andy" block above, use your name, and season to taste.
$(error Unknown user. Please edit Makefile with your configuration block.)

endif

#--------------------------- The rest should be automatic. ------------------------------

CFILES:=$(shell find src -name '*.c')
OFILES:=$(patsubst src/%.c,mid/%.o,$(CFILES))
-include $(OFILES:.o=.d)
mid/%.o:src/%.c;$(PRECMD) $(CC) -o$@ $<

ifneq (,$(LIBSTATIC))
  all:$(LIBSTATIC)
  LIBSTATIC_OFILES:=$(filter mid/tfss/%,$(OFILES))
  $(LIBSTATIC):$(LIBSTATIC_OFILES);$(PRECMD) $(AR) rc $@ $^
endif

define WRAPPER_RULES
  OFILES_$1:=$$(filter mid/$1/%,$(OFILES))
  EXE_$1:=out/$1
  all:$$(EXE_$1)
  $$(EXE_$1):$$(OFILES_$1) $(LIBSTATIC);$$(PRECMD) $(LD) -o$$@ $$(OFILES_$1) $(LDPOST_$1) $(LIBSTATIC)
endef
$(foreach W,$(WRAPPERS),$(eval $(call WRAPPER_RULES,$W)))

FIRST_WRAPPER:=$(strip $(firstword $(WRAPPERS)))
ifeq (,$(FIRST_WRAPPER))
  run:;echo "No wrappers configured." ; exit 1
else
  run:$(EXE_$(FIRST_WRAPPER));$(EXE_$(FIRST_WRAPPER)) src/data/eternal_torment.mid --repeat
endif

ifneq (,$(RAYLIB_WEB_SDK))
  WASMCFILES:=$(filter src/ray/%.c src/tfss/%.c,$(CFILES))
  WASM_EXE:=out/tfss.html
  WASM_LIBA:=$(RAYLIB_WEB_SDK)/lib/libraylib.a
  $(WASM_EXE):$(WASMCFILES);$(PRECMD) emcc -o$@ $^ -Os -Wall $(WASM_LIBA) -Isrc -I$(RAYLIB_WEB_SDK)/include -L$(RAYLIB_WEB_SDK)/lib -lraylib -s USE_GLFW=3 --shell-file src/minshell.html -DPLATFORM_WEB
  web:$(WASM_EXE)
  all:$(WASM_EXE)
endif

clean:;rm -rf mid out
