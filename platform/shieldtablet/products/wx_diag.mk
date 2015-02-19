# NVIDIA Tegra5 "Ardbeg" development system
#
# Copyright (c) 2013-2014, NVIDIA Corporation.  All rights reserved.
#
# AndroidProducts.mk is included before BoardConfig.mk, variable essential at
# start of build and used in here should always be intialized in this file

$(call inherit-product, $(SRC_TARGET_DIR)/product/generic.mk)

include $(LOCAL_PATH)/wx_base.mk

## Thse are default settings, it gets changed as per sku manifest properties
PRODUCT_NAME := wx_diag
PRODUCT_MODEL := wx_diag

## Value of PRODUCT_NAME is mangeled before it can be
## used because of call to inherits, store their values to
## use later in this file below
_product_name := $(strip $(PRODUCT_NAME))

# Tegranote diagsuite app path
ifneq ($(wildcard vendor/nvidia/tegranote/apps/diagsuite),)
NV_DIAGSUITE_PATH := vendor/nvidia/tegranote/apps/diagsuite
else
NV_DIAGSUITE_PATH := vendor/nvidia/tegra/apps/diagsuite
endif

## Icera modem integration
$(call inherit-product-if-exists, $(LOCAL_PATH)/../../../common/icera/icera-modules.mk)
$(call inherit-product-if-exists, $(LOCAL_PATH)/../../../common/icera/firmware/nvidia-e1729-tn8l/fw-cpy-nvidia-e1729-tn8l-voice-prod.mk)
PRODUCT_COPY_FILES += \
        $(call add-to-product-copy-files-if-exists, $(LOCAL_PATH)/../../../drivers/comms/init.icera.rc:root/init.icera.rc) \
        $(call add-to-product-copy-files-if-exists, vendor/nvidia/tegra/icera/ril/icera-util/ril_atc.usb.config:system/etc/ril_atc.config)

## copy gpsconfig.xml with logged enabled in diag sku
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/gpsconfig-mo-diag.xml:system/etc/gps/gpsconfig.xml

## SKU specific overrides
include frameworks/native/build/phone-xhdpi-2048-dalvik-heap.mk

PRODUCT_PACKAGE_OVERLAYS += $(LOCAL_PATH)/../../../product/phone/overlay-phone

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.hardware.camera.autofocus.xml:system/etc/permissions/android.hardware.camera.autofocus.xml

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.tn8diag.rc:root/init.tn8diag.rc \
    $(LOCAL_PATH)/../../../common/init.cal_fct.rc:root/init.cal.rc \
    $(NV_DIAGSUITE_PATH)/bin/stress_test.sh:root/bin/stress_test.sh \
    $(NV_DIAGSUITE_PATH)/bin/CPU/power_utils.sh:data/mfgtest/CPU/power_utils.sh \
    $(NV_DIAGSUITE_PATH)/bin/CPU/roth_dvfs.sh:data/mfgtest/CPU/roth_dvfs.sh \
    $(NV_DIAGSUITE_PATH)/src/Tn8Diag/external/CPU/game_testing.sh:data/mfgtest/CPU/game_testing.sh \
    $(NV_DIAGSUITE_PATH)/bin/thermal/burnCortexA15_linux:data/mfgtest/thermal/burnCortexA15_linux \
    $(NV_DIAGSUITE_PATH)/bin/thermal/EDPVirus_linux:data/mfgtest/thermal/EDPVirus_linux \
    $(NV_DIAGSUITE_PATH)/bin/thermal/run.sh:data/mfgtest/thermal/run.sh \
    $(NV_DIAGSUITE_PATH)/bin/thermal/thermal_test.py:data/mfgtest/thermal/thermal_test.py \
    $(NV_DIAGSUITE_PATH)/bin/GPU/gl30.xml:data/media/Android/data/com.glbenchmark.glbenchmark30/cache/selected_tests.xml \
    $(NV_DIAGSUITE_PATH)/src/Tn8Diag/external/GPU/game_testing.sh:data/mfgtest/GPU/game_testing.sh \
    $(NV_DIAGSUITE_PATH)/bin/GPU/busybox:root/bin/busybox \
    $(NV_DIAGSUITE_PATH)/bin/thorMon/thorMon.py:root/bin/thorMon.py \
    $(NV_DIAGSUITE_PATH)/bin/thorMon/runThorMon.sh:root/bin/runThorMon.sh \
    $(NV_DIAGSUITE_PATH)/bin/flash/loki_mac_writer.sh:loki_mac_writer.sh \
    $(NV_DIAGSUITE_PATH)/bin/flash/main.sh:main.sh \
    $(NV_DIAGSUITE_PATH)/bin/flash/pullLogFiles.sh:pullLogFiles.sh \
    $(NV_DIAGSUITE_PATH)/bin/flash/flash.sh:flash.sh \
    $(NV_DIAGSUITE_PATH)/bin/modem/atcmd-itf-arm:data/bin/atcmd-itf-arm

## nvidia apps for this sku
$(call inherit-product-if-exists, $(_product_private_path)/$(_product_name).mk)

## 3rd-party apps for this sku
$(call inherit-product-if-exists, 3rdparty/applications/prebuilt/common/$(_product_name).mk)

## Clean local variables
_product_name :=
_product_private_path :=
