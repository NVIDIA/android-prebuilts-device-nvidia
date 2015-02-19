# NVIDIA Tegra5 "Ardbeg" development system
#
# Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
#
# AndroidProducts.mk is included before BoardConfig.mk, variable essential at
# start of build and used in here should always be intialized in this file

## All essential packages
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic.mk)

include $(LOCAL_PATH)/wx_base.mk

## Thse are default settings, it gets changed as per sku manifest properties
PRODUCT_NAME := wx_un_mo
PRODUCT_MODEL := wx_un_mo

## Value of PRODUCT_NAME is mangeled before it can be
## used because of call to inherits, store their values to
## use later in this file below
_product_name := $(strip $(PRODUCT_NAME))

## Icera modem integration
$(call inherit-product-if-exists, $(LOCAL_PATH)/../../../common/icera/icera-modules.mk)
$(call inherit-product-if-exists, $(LOCAL_PATH)/../../../common/icera/firmware/nvidia-e1729-tn8l/fw-cpy-nvidia-e1729-tn8l-voice-prod.mk)

PRODUCT_COPY_FILES += \
        $(call add-to-product-copy-files-if-exists, $(LOCAL_PATH)/../../../drivers/comms/init.icera.rc:root/init.icera.rc) \
        $(call add-to-product-copy-files-if-exists, vendor/nvidia/tegra/icera/ril/icera-util/ril_atc.usb.config:system/etc/ril_atc.config)

PRODUCT_PROPERTY_OVERRIDES += ro.modem.vc=1 \
                        ril.icera-config-args=notifier:ON,datastall:ON,lwaactivate

## SKU specific overrides
include frameworks/native/build/phone-xhdpi-2048-dalvik-heap.mk

PRODUCT_PACKAGE_OVERLAYS += $(LOCAL_PATH)/../../../product/phone/overlay-phone

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml

## GPS configuration
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/gpsconfig-ardbeg.xml:system/etc/gps/gpsconfig.xml

## enable cell broadcast receiver for mms
PRODUCT_PACKAGES += CellBroadcastReceiver

## enable Wifi Access Point monitor (needed for two-step SAR backoff)
PRODUCT_PACKAGES += icera-config

## common settings for all sku except for diag
include $(LOCAL_PATH)/wx_common_user.mk

## nvidia apps for this sku
$(call inherit-product-if-exists, $(_product_private_path)/$(_product_name).mk)

## Clean local variables
_product_name :=
_product_private_path :=
