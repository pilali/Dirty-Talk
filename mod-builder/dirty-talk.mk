################################################################################
#
# dirty-talk  --  mod-plugin-builder package
#
# Builds the LV2 flavour of Dirty Talk from the real source tree (DPF as a git
# submodule) and ships the classic HTML/CSS modgui.
#
# Drop this file (and a matching Config.in entry) into mod-plugin-builder under
# package/dirty-talk/ and add `dirty-talk` to your build set.
#
################################################################################

# Pin to a tag/commit of the Dirty Talk repository.
DIRTY_TALK_VERSION = main
DIRTY_TALK_SITE = https://github.com/pilali/Dirty-Talk.git
DIRTY_TALK_SITE_METHOD = git
DIRTY_TALK_GIT_SUBMODULES = YES
DIRTY_TALK_BUNDLES = dirty-talk.lv2

# Build a single DSP-only LV2 binary (no native ui:X11UI — the web modgui drives
# the UI on MOD), then generate the .ttl files with DPF's lv2_ttl_generator. The
# generator is built with the target toolchain and run through mod-plugin-builder's
# qemu EXE_WRAPPER (honoured by generate-ttl.sh). DIRTY_TALK_DSP_ONLY=true sets
# -DDISTRHO_PLUGIN_HAS_UI=0 inside the plugin Makefile so the cross-toolchain
# CXXFLAGS stay intact.
define DIRTY_TALK_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) \
		-C $(@D)/dpf/utils/lv2-ttl-generator
	$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) NOOPT=true \
		DIRTY_TALK_DSP_ONLY=true -C $(@D)/plugins/DirtyTalk lv2_dsp
	cd $(@D) && $(TARGET_MAKE_ENV) ./dpf/utils/generate-ttl.sh
endef

# Assemble the MOD bundle the way mod-host/mod-ui expect (cf. megalo.lv2):
#  - a single DSP binary, named plainly dirty_talk.so (drop DPF's _dsp suffix);
#  - manifest.ttl -> seeAlso the plugin .ttl AND the modgui .ttl;
#  - modgui.ttl at the bundle root, web resources under modgui/.
DIRTY_TALK_URI = https://mod.audio/plugins/dirty-talk
define DIRTY_TALK_INSTALL_TARGET_CMDS
	mkdir -p $($(PKG)_PKGDIR)/dirty-talk.lv2/modgui
	# DSP binary + generated descriptor, renamed to the plain bundle name.
	cp $(@D)/bin/dirty_talk.lv2/dirty_talk_dsp.so  $($(PKG)_PKGDIR)/dirty-talk.lv2/dirty_talk.so
	cp $(@D)/bin/dirty_talk.lv2/dirty_talk_dsp.ttl $($(PKG)_PKGDIR)/dirty-talk.lv2/dirty_talk.ttl
	# manifest: rewrite the _dsp references, then add the modgui seeAlso.
	sed -e 's|dirty_talk_dsp\.so|dirty_talk.so|g' \
	    -e 's|dirty_talk_dsp\.ttl|dirty_talk.ttl|g' \
	    $(@D)/bin/dirty_talk.lv2/manifest.ttl > $($(PKG)_PKGDIR)/dirty-talk.lv2/manifest.ttl
	printf '\n<%s>\n    rdfs:seeAlso <modgui.ttl> .\n' "$(DIRTY_TALK_URI)" \
	    >> $($(PKG)_PKGDIR)/dirty-talk.lv2/manifest.ttl
	# modgui: .ttl at the bundle root, web resources under modgui/.
	cp $(@D)/modgui/modgui.ttl $($(PKG)_PKGDIR)/dirty-talk.lv2/modgui.ttl
	cp $(@D)/modgui/dirty-talk.html $(@D)/modgui/stylesheet.css \
	   $(@D)/modgui/screenshot.png $(@D)/modgui/thumbnail.png \
	   $($(PKG)_PKGDIR)/dirty-talk.lv2/modgui/
endef

$(eval $(generic-package))
