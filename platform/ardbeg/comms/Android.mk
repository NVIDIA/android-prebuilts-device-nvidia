#
# NFC symlink creator
#

SYMLINKS := $(TARGET_OUT)/etc/permissions/android.hardware.nfc.xml
$(SYMLINKS): NFC_SYMLINK := /data/nfc/nfc.xml
$(SYMLINKS): $(LOCAL_INSTALLED_MODULE) $(LOCAL_PATH)/Android.mk
	@echo "Symlink: $@ -> $(NFC_SYMLINK)"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf $(NFC_SYMLINK) $@
ALL_DEFAULT_INSTALLED_MODULES += $(SYMLINKS)

