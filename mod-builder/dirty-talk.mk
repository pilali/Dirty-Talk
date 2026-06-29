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

# Build the separate LV2 DSP+UI binaries, then generate the .ttl files with
# DPF's lv2_ttl_generator. The generator is built with the target toolchain and
# run through mod-plugin-builder's qemu EXE_WRAPPER (honoured by generate-ttl.sh).
define DIRTY_TALK_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) \
		-C $(@D)/dpf/utils/lv2-ttl-generator
	$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) NOOPT=true \
		-C $(@D)/plugins/DirtyTalk lv2_sep
	cd $(@D) && $(TARGET_MAKE_ENV) ./dpf/utils/generate-ttl.sh
endef

# Install the generated bundle, then add the modgui resources and merge the
# modgui triples into the plugin .ttl.
define DIRTY_TALK_INSTALL_TARGET_CMDS
	mkdir -p $($(PKG)_PKGDIR)/dirty-talk.lv2
	# the desktop X11 UI is kept in the bundle but ignored on MOD, which uses
	# the web modgui below; keeping it avoids dangling ui:ui references.
	cp -r $(@D)/bin/dirty_talk.lv2/. $($(PKG)_PKGDIR)/dirty-talk.lv2/
	cp -r $(@D)/modgui $($(PKG)_PKGDIR)/dirty-talk.lv2/modgui
	rm -f $($(PKG)_PKGDIR)/dirty-talk.lv2/modgui/modgui.ttl
	cat $(@D)/modgui/modgui.ttl >> $($(PKG)_PKGDIR)/dirty-talk.lv2/dirty_talk_dsp.ttl
endef

$(eval $(generic-package))
