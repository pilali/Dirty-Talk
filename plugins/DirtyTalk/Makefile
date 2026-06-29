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
# MOD / LV2 device build: build a single DSP-only binary with no native
# ui:X11UI (the web modgui drives the UI on MOD). Set DIRTY_TALK_DSP_ONLY=true
# and build the `lv2_dsp` target. Appending to BUILD_CXX_FLAGS keeps the
# cross-toolchain's own CXXFLAGS intact (don't override CXXFLAGS on the
# command line under mod-plugin-builder).

ifeq ($(DIRTY_TALK_DSP_ONLY),true)
BUILD_CXX_FLAGS += -DDISTRHO_PLUGIN_HAS_UI=0
endif

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
