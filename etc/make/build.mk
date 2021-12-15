#------------------------------------------------
# Discover C files, digest into O files.

SRCFILES:=$(shell find src -type f)

CC+=$(patsubst %,-DBB_USE_%=1,$(DRIVERS_ENABLE))
DRIVERS_AVAILABLE:=$(notdir $(wildcard src/driver/types/*))
DRIVERS_IGNORE:=$(filter-out $(DRIVERS_ENABLE),$(DRIVERS_AVAILABLE))
IGNORE_PATTERN:=$(addprefix src/driver/types/,$(addsuffix /%,$(DRIVERS_IGNORE)))
SRCFILES:=$(filter-out $(IGNORE_PATTERN),$(SRCFILES))

CFILES:=$(filter %.c,$(SRCFILES))
OFILES_ALL:=$(patsubst src/%.c,$(MIDDIR)/%.o,$(CFILES))
OFILES_BBA:=$(filter $(MIDDIR)/bba/%,$(OFILES_ALL))
OFILES_BBB:=$(filter $(MIDDIR)/bbb/%,$(OFILES_ALL))
OFILES_DRIVER:=$(filter $(MIDDIR)/driver/%,$(OFILES_ALL))
OFILES_DEMO:=$(filter $(MIDDIR)/demo/%,$(OFILES_ALL))
OFILES_CLI:=$(filter $(MIDDIR)/cli/%,$(OFILES_ALL))
OFILES_SHARE:=$(filter $(MIDDIR)/share/%,$(OFILES_ALL))
OFILES_ENCHILADA:=$(OFILES_BBA) $(OFILES_BBB) $(OFILES_DRIVER) $(OFILES_SHARE)

-include $(OFILES_ALL:.o=.d)

$(MIDDIR)/%.o:src/%.c;$(PRECMD) $(CC) -o $@ $<

#--------------------------------------------------
# Generate demo data.

DEMO_DATA_INPUTS:=$(filter src/demo/data/%,$(SRCFILES))
DEMO_BBBAR_SOURCES:=$(filter src/demo/data/bbbar-%,$(DEMO_DATA_INPUTS))
DEMO_BBBAR_NAMES:=$(sort $(foreach F,$(DEMO_BBBAR_SOURCES), \
  $(firstword $(subst /, ,$(patsubst src/demo/data/%,%,$F))) \
))
DEMO_DATA_INPUTS:=$(filter-out src/demo/data/bbbar-%,$(DEMO_DATA_INPUTS))
DEMO_DATA_OUTPUTS:=$(patsubst src/demo/data/%.mid,$(MIDDIR)/demo/data/%.bba, \
  $(DEMO_DATA_INPUTS) \
) $(patsubst %,$(MIDDIR)/demo/data/%.bbbar,$(DEMO_BBBAR_NAMES))

ifneq (,$(strip $(EXE_CLI)))
  all:$(DEMO_DATA_OUTPUTS)
  $(MIDDIR)/%.bba:src/%.mid $(EXE_CLI);$(PRECMD) $(EXE_CLI) mid2bba -o$@ $<
  # This rebuilds *every* bbbar whenever *one* of them changes. I think not a big problem.
  $(MIDDIR)/%.bbbar:$(DEMO_BBBAR_SOURCES) $(EXE_CLI);$(PRECMD) $(EXE_CLI) bbbar -c $@ src/$*
else
  $(MIDDIR)/%.bba:src/%.mid;$(PRECMD) echo "$@:WARNING: EXE_CLI unset, producing dummy song" ; touch $@
  $(MIDDIR)/%.bbbar:;$(PRECMD) echo "$@:WARNING: EXE_CLI unset, producing dummy archive" ; echo -en "\0\xbb\xbaR" >$@
endif

#---------------------------------------------------
# Archive or link the main outputs.

# BBA is for highly restrictive embedded systems -- We ship it alone, no extra utilities or whatnot.
ifneq (,$(strip $(LIB_BBA)))
  all:$(LIB_BBA)
  $(LIB_BBA):$(OFILES_BBA);$(PRECMD) $(AR) $@ $^
endif

# Other single-API libs get the drivers and shared bits too.
ifneq (,$(strip $(LIB_BBB)))
  all:$(LIB_BBB)
  $(LIB_BBB):$(OFILES_BBB) $(OFILES_DRIVER) $(OFILES_SHARE);$(PRECMD) $(AR) $@ $^
endif

ifneq (,$(strip $(LIB_BBDRIVER)))
  all:$(LIB_BBDRIVER)
  $(LIB_BBDRIVER):$(OFILES_DRIVER);$(PRECMD) $(AR) $@ $^
endif

# The special 'libbeepbot' contains all the APIs, drivers, and utility code.
ifneq (,$(strip $(LIB_BEEPBOT)))
  all:$(LIB_BEEPBOT)
  $(LIB_BEEPBOT):$(OFILES_ENCHILADA);$(PRECMD) $(AR) $@ $^
endif

ifneq (,$(strip $(EXE_DEMO)))
  all:$(EXE_DEMO)
  $(EXE_DEMO):$(OFILES_ENCHILADA) $(OFILES_DEMO);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)
endif

ifneq (,$(strip $(EXE_CLI)))
  all:$(EXE_CLI)
  $(EXE_CLI):$(OFILES_ENCHILADA) $(OFILES_CLI);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)
endif

