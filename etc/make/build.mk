SRCFILES:=$(shell find src -type f)

CC+=$(patsubst %,-DBB_USE_%=1,$(DRIVERS_ENABLE))
DRIVERS_AVAILABLE:=$(notdir $(wildcard src/driver/types/*))
DRIVERS_IGNORE:=$(filter-out $(DRIVERS_ENABLE),$(DRIVERS_AVAILABLE))
IGNORE_PATTERN:=$(addprefix src/driver/types/,$(addsuffix /%,$(DRIVERS_IGNORE)))
SRCFILES:=$(filter-out $(IGNORE_PATTERN),$(SRCFILES))

CFILES:=$(filter %.c,$(SRCFILES))
OFILES_ALL:=$(patsubst src/%.c,$(MIDDIR)/%.o,$(CFILES))
OFILES_BBA:=$(filter $(MIDDIR)/bba/%,$(OFILES_ALL))
OFILES_DRIVER:=$(filter $(MIDDIR)/driver/%,$(OFILES_ALL))
OFILES_DEMO:=$(filter $(MIDDIR)/demo/%,$(OFILES_ALL))
OFILES_CLI:=$(filter $(MIDDIR)/cli/%,$(OFILES_ALL))
OFILES_SHARE:=$(filter $(MIDDIR)/share/%,$(OFILES_ALL))

-include $(OFILES_ALL:.o=.d)

$(MIDDIR)/%.o:src/%.c;$(PRECMD) $(CC) -o $@ $<

ifneq (,$(strip $(LIB_BBA)))
  all:$(LIB_BBA)
  $(LIB_BBA):$(OFILES_BBA);$(PRECMD) $(AR) $@ $^
endif

ifneq (,$(strip $(LIB_BBDRIVER)))
  all:$(LIB_BBDRIVER)
  $(LIB_BBDRIVER):$(OFILES_DRIVER);$(PRECMD) $(AR) $@ $^
endif

ifneq (,$(strip $(LIB_BEEPBOT)))
  all:$(LIB_BEEPBOT)
  $(LIB_BEEPBOT):$(OFILES_BBA) $(OFILES_DRIVER) $(OFILES_SHARE);$(PRECMD) $(AR) $@ $^
endif

ifneq (,$(strip $(EXE_DEMO)))
  all:$(EXE_DEMO)
  $(EXE_DEMO):$(OFILES_BBA) $(OFILES_DRIVER) $(OFILES_SHARE) $(OFILES_DEMO);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)
endif

ifneq (,$(strip $(EXE_CLI)))
  all:$(EXE_CLI)
  $(EXE_CLI):$(OFILES_BBA) $(OFILES_DRIVER) $(OFILES_SHARE) $(OFILES_CLI);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)
endif

