all:
.SILENT:
.SECONDARY:
.PREFIXES=
PRECMD=echo "  $@" ; mkdir -p $(@D) ;

#-------------------------- Per-host configuration. -----------------------------------

ifeq ($(USER),andy)

WRAPPERS:=pulse
CC:=gcc -c -MMD -O3 -Isrc -Werror -Wimplicit
LD:=gcc
LDPOST:=
LDPOST_pulse:=-lpulse-simple
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

clean:;rm -rf mid out
