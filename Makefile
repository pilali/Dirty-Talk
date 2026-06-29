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

# --------------------------------------------------------------
# Consolidated MOD / LV2 device bundle (local mirror of
# mod-builder/dirty-talk.mk): a single DSP-only dirty_talk.so + generated
# .ttl, modgui.ttl at the bundle root and web resources under modgui/.
# Output: bin/dirty-talk.lv2/  (ready to drop on a mod-host/mod-ui device).
DIRTY_TALK_URI = https://mod.audio/plugins/dirty-talk

mod: dpf/utils/lv2_ttl_generator$(APP_EXT)
	rm -rf bin/dirty_talk.lv2 bin/dirty-talk.lv2
	$(MAKE) DIRTY_TALK_DSP_ONLY=true -C plugins/DirtyTalk lv2_dsp
	@$(CURDIR)/dpf/utils/generate-ttl.sh
	mkdir -p bin/dirty-talk.lv2/modgui
	cp bin/dirty_talk.lv2/dirty_talk_dsp.so  bin/dirty-talk.lv2/dirty_talk.so
	cp bin/dirty_talk.lv2/dirty_talk_dsp.ttl bin/dirty-talk.lv2/dirty_talk.ttl
	sed -e 's|dirty_talk_dsp\.so|dirty_talk.so|g' \
	    -e 's|dirty_talk_dsp\.ttl|dirty_talk.ttl|g' \
	    bin/dirty_talk.lv2/manifest.ttl > bin/dirty-talk.lv2/manifest.ttl
	printf '\n<%s>\n    rdfs:seeAlso <modgui.ttl> .\n' "$(DIRTY_TALK_URI)" \
	    >> bin/dirty-talk.lv2/manifest.ttl
	cp modgui/modgui.ttl bin/dirty-talk.lv2/modgui.ttl
	cp modgui/dirty-talk.html modgui/stylesheet.css \
	   modgui/screenshot.png modgui/thumbnail.png bin/dirty-talk.lv2/modgui/
	rm -rf bin/dirty_talk.lv2
	@echo "MOD bundle ready: bin/dirty-talk.lv2/"

clean:
	$(MAKE) clean -C plugins/DirtyTalk
	rm -rf bin build

.PHONY: all plugins gen mod clean
