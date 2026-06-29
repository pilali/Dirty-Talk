#!/usr/bin/make -f
# Top-level Makefile for Dirty Talk
# ---------------------------------

include dpf/Makefile.base.mk

all: plugins

plugins:
	$(MAKE) all -C plugins/DirtyTalk

# build the LV2 ttl generator (used to auto-generate the LV2 .ttl files)
gen: plugins dpf/utils/lv2_ttl_generator$(APP_EXT)
	@$(CURDIR)/dpf/utils/generate-ttl.sh

dpf/utils/lv2_ttl_generator$(APP_EXT):
	$(MAKE) -C dpf/utils/lv2-ttl-generator

clean:
	$(MAKE) clean -C plugins/DirtyTalk
	rm -rf bin build

.PHONY: all plugins gen clean
