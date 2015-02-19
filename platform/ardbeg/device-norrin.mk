# NVIDIA Tegra5 "Ardbeg" development system
#
# Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.
#
# ------------------------------------------------
# specific items for norrin
# ------------------------------------------------

ifeq ($(wildcard vendor/nvidia/tegra/core-private),vendor/nvidia/tegra/core-private)
    NVFLASH_FILES_PATH := vendor/nvidia/tegra/bootloader/nvbootloader/odm-partner/ardbeg
else
    NVFLASH_FILES_PATH := vendor/nvidia/tegra/odm/ardbeg
endif

ifneq ($(APPEND_DTB_TO_KERNEL),true)
ifneq ($(BUILD_NV_CRASHCOUNTER),true)
PRODUCT_COPY_FILES += \
    $(NVFLASH_FILES_PATH)/nvflash/norrin_android_fastboot_nvtboot_dtb_emmc_full.cfg:norrin_flash.cfg
endif
endif

NVFLASH_FILES_PATH :=

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/ueventd.ardbeg.rc:root/ueventd.norrin.rc \
    $(LOCAL_PATH)/init.norrin.rc:root/init.norrin.rc \
    $(LOCAL_PATH)/fstab.norrin:root/fstab.norrin
