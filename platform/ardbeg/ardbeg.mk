# NVIDIA Tegra5 "Ardbeg" development system
#
# Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
#
# AndroidProducts.mk is included before BoardConfig.mk, variable essential at
# start of build and used in here should always be intialized in this file

## All essential packages
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)
$(call inherit-product, device/nvidia/platform/ardbeg/device.mk)
$(call inherit-product, device/nvidia/platform/ardbeg/device-norrin.mk)
$(call inherit-product-if-exists, frameworks/base/data/sounds/AudioPackage3.mk)

## Thse are default settings, it gets changed as per sku manifest properties
PRODUCT_NAME := ardbeg
PRODUCT_DEVICE := ardbeg
PRODUCT_MODEL := ardbeg
PRODUCT_MANUFACTURER := NVIDIA
PRODUCT_BRAND := nvidia

## Icera modem integration
$(call inherit-product-if-exists, $(LOCAL_PATH)/../../common/icera/icera-modules.mk)
$(call inherit-product-if-exists, $(LOCAL_PATH)/../../common/icera/firmware/nvidia-e1729-tn8l/fw-cpy-nvidia-e1729-tn8l-do.mk)
$(call inherit-product-if-exists, $(LOCAL_PATH)/../../common/icera/firmware/nvidia-e1729-tn8l/fw-cpy-nvidia-e1729-tn8l-voice.mk)
PRODUCT_COPY_FILES += \
        $(call add-to-product-copy-files-if-exists, $(LOCAL_PATH)/../../drivers/comms/init.icera.rc:root/init.icera.rc) \
        $(call add-to-product-copy-files-if-exists, vendor/nvidia/tegra/icera/ril/icera-util/ril_atc.usb.config:system/etc/ril_atc.config)

## enable Wifi Access Point monitor (needed for two-step SAR backoff)
PRODUCT_PACKAGES += icera-wifiAPNotifier

## SKU specific overrides
include frameworks/native/build/phone-xhdpi-2048-dalvik-heap.mk

DEVICE_PACKAGE_OVERLAYS += $(LOCAL_PATH)/../../product/phone/overlay-phone

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml

## GPS configuration
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/gpsconfig-ardbeg.xml:system/etc/gps/gpsconfig.xml

## Add NVCameraAwesome app
PRODUCT_PACKAGES += NVCameraAwesome

## Barometer sensor
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.barometer.xml:system/etc/permissions/android.hardware.sensor.barometer.xml

## Sensor/Touch calibration init.xx.rc file
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/../../common/init.cal_fct.rc:root/init.cal.rc

# NFC packages
PRODUCT_PACKAGES += \
  libnfc \
  libnfc_jni \
  Nfc \
  Tag

## GMS apps
$(call inherit-product-if-exists, 3rdparty/google/gms-apps/tablet/32/products/gms.mk)
