ifndef BB_CONFIG
  UNAMES:=$(shell uname -s)
  ifeq ($(UNAMES),Linux)
    BB_CONFIG:=linux-default
  else
    $(error Unsupported platform '$(UNAMES)' -- TODO)
  endif
endif

export BB_CONFIG

MIDDIR:=mid/$(BB_CONFIG)
OUTDIR:=out/$(BB_CONFIG)

LIB_BBA:=$(OUTDIR)/libbba.a
LIB_BBB:=$(OUTDIR)/libbbb.a
LIB_BBDRIVER:=$(OUTDIR)/libbbdriver.a
LIB_BEEPBOT:=$(OUTDIR)/libbeepbot.a
EXE_DEMO:=$(OUTDIR)/demo
EXE_CLI:=$(OUTDIR)/beepbot

include etc/config/$(BB_CONFIG).mk

CC+=-DBB_MIDDIR='"$(MIDDIR)"' -DBB_OUTDIR='"$(OUTDIR)"'
