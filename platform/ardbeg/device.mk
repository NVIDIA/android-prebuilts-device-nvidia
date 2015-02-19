# NVIDIA Tegra5 "Ardbeg" development system
#
# Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.

## Common packages
$(call inherit-product-if-exists, device/nvidia/platform/ardbeg/device-common.mk)

PRODUCT_COPY_FILES += \
  $(LOCAL_PATH)/bt_vendor.conf:system/etc/bluetooth/bt_vendor.conf

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/../../common/init.ray_touch.rc:root/init.ray_touch.rc

PRODUCT_PACKAGES += \
    hostapd \
    wpa_supplicant \
    wpa_supplicant.conf

#enable Widevine drm
PRODUCT_PROPERTY_OVERRIDES += drm.service.enabled=true
PRODUCT_PACKAGES += \
    com.google.widevine.software.drm.xml \
    com.google.widevine.software.drm \
    libdrmwvmplugin \
    libwvm \
    libWVStreamControlAPI_L1 \
    libwvdrm_L1

#needed by google GMS lib:libpatts_engine_jni_api.so
PRODUCT_PACKAGES += \
    libwebrtc_audio_coding

#Marvell firmware package
PRODUCT_PACKAGES += sd8897_uapsta.bin \
    sd8797_uapsta.bin

# Live Wallpapers
PRODUCT_PACKAGES += \
    LiveWallpapers \
    LiveWallpapersPicker \
    HoloSpiralWallpaper \
    MagicSmokeWallpapers \
    NoiseField \
    Galaxy4 \
    VisualizationWallpapers \
    PhaseBeam \
    librs_jni

PRODUCT_PACKAGES += \
    sensors.tegra \
    lights.tegra \
    audio.primary.tegra \
    audio.a2dp.default \
    audio.usb.default \
    libaudiopolicymanager \
    audio.nvwc.tegra \
    power.tegra \
    setup_fs \
    drmserver \
    Gallery2 \
    libdrmframework_jni \
    overlaymon \
    e2fsck \
    charger \
    charger_res_images

#NvEsrd
PRODUCT_PACKAGES += \
    NvEsrd

# Homepage provider APK for Google Chrome browser
PRODUCT_PACKAGES += \
    ChromeCustomizations

# Partner bookmarks provider APK for Google Chrome browser
PRODUCT_PACKAGES += \
    PartnerBookmarksProvider

PRODUCT_PACKAGES += \
    tos.img

#TegraOTA
PRODUCT_PACKAGES += \
    TegraOTA

#Stats
ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
    PRODUCT_PACKAGES += \
        Stats
endif

#ControllerMapper
ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
    PRODUCT_PACKAGES += \
        ControllerMapper
endif

PRODUCT_PACKAGES += \
    lbh_images

# HDCP SRM Support
PRODUCT_PACKAGES += \
    hdcp1x.srm \
    hdcp2x.srm \
    hdcp2xtest.srm

# Application for sending feedback to NVIDIA
PRODUCT_PACKAGES += \
    nvidiafeedback

## Calibration notifier
PRODUCT_PACKAGES += CalibNotifier
PRODUCT_COPY_FILES += \
    device/nvidia/platform/ardbeg/calibration/calib_cfg.xml:system/etc/calib_cfg.xml

# Paragon filesystem solution binaries
PRODUCT_PACKAGES += \
    mount.ufsd \
    chkufsd \
    mkexfat \
    chkexfat \
    mkhfs \
    chkhfs \
    mkntfs \
    chkntfs
