#
# Copyright (c) 2014 NVIDIA Corporation.  All Rights Reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

PRODUCT_PACKAGES += \
    hostapd \
    wpa_supplicant \
    wpa_supplicant.conf

PRODUCT_COPY_FILES += \
  frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
  frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
  frameworks/native/data/etc/android.hardware.bluetooth_le.xml:system/etc/permissions/android.hardware.bluetooth_le.xml \
  frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
  device/nvidia/common/wifi_loader.sh:system/bin/wifi_loader.sh \
  device/nvidia/common/wpa_supplicant.sh:system/bin/wpa_supplicant.sh \
  device/nvidia/common/gps_select.sh:system/bin/gps_select.sh \
  device/nvidia/common/init.comms.rc:root/init.comms.rc \
  device/nvidia/drivers/comms/brcm_wpa.conf:/system/etc/firmware/brcm_wpa.conf \
  device/nvidia/drivers/comms/brcm_p2p.conf:/system/etc/firmware/brcm_p2p.conf

ifeq ($(PLATFORM_IS_NEXT),1)
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/bcm4752_next_64/glgps_nvidiategraandroid:system/bin/glgps_nvidiaTegra2android \
    vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/bcm4752_next_64/gps.nvidiategraandroid.so:system/lib64/hw/gps.brcm.so
else
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/bcm4752/glgps_nvidiaTegra2android:system/bin/glgps_nvidiaTegra2android \
    vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/bcm4752/gps.tegra.so:system/lib/hw/gps.brcm.so
endif

PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/gps.conf:system/etc/gps.conf

