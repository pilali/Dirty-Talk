#!/usr/bin/make -f
# Makefile for Dirty Talk
# -----------------------

NAME = dirty_talk

# --------------------------------------------------------------
# Files to build

FILES_DSP = \
	DirtyTalkPlugin.cpp

FILES_UI = \
	DirtyTalkUI.cpp

# --------------------------------------------------------------
# Do some magic

include ../../dpf/Makefile.plugins.mk

# --------------------------------------------------------------
# Enable all possible plugin types

TARGETS += lv2_sep

ifeq ($(HAVE_OPENGL),true)
TARGETS += jack
TARGETS += vst2
TARGETS += vst3
TARGETS += clap
ifeq ($(MACOS),true)
TARGETS += au
endif
endif # HAVE_OPENGL

all: $(TARGETS)

# --------------------------------------------------------------
