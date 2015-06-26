# NVIDIA Tegra5 "Ardbeg" development system
#
# Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.
#
# ------------------------------------------------
# Common options for both mobile and automotive
# ------------------------------------------------

## Common product locale
PRODUCT_LOCALES += en_US

## Common property overrides
PRODUCT_PROPERTY_OVERRIDES += ro.com.google.clientidbase=android-nvidia

## Common packages
$(call inherit-product-if-exists, vendor/nvidia/tegra/secureos/nvsi/nvsi.mk)
$(call inherit-product-if-exists, frameworks/base/data/videos/VideoPackage2.mk)
$(call inherit-product-if-exists, vendor/nvidia/tegra/core/android/t124/full.mk)
$(call inherit-product-if-exists, vendor/nvidia/tegra/core/nvidia-tegra-vendor.mk)
$(call inherit-product-if-exists, vendor/nvidia/tegra/ardbeg/partition-data/factory-ramdisk/factory.mk)

include device/nvidia/common/dalvik/tablet-10in-hdpi-2048-dalvik-heap.mk
include device/nvidia/drivers/touchscreen/raydium/raydium.mk

PRODUCT_LOCALES += mdpi hdpi xhdpi

NV_VCM_FLASH_SCRIPT_PATH := vendor/nvidia/tegra/embedded/tools
NV_VCM_FLASH_FILES_PATH := $(NV_VCM_FLASH_SCRIPT_PATH)/boards/vcm30t124

ifneq ($(wildcard $(NV_VCM_FLASH_FILES_PATH)),)
PRODUCT_COPY_FILES += \
    $(NV_VCM_FLASH_FILES_PATH)/nvflash/VCM30T124_2GB_H5TC4G63AFR.bct:VCM30T124_2GB_H5TC4G63AFR.bct \
    $(NV_VCM_FLASH_FILES_PATH)/nvflash/VCM30T124_4GB_MT41K512M16.bct:VCM30T124_4GB_MT41K512M16.bct \
    $(NV_VCM_FLASH_FILES_PATH)/nvflash/quickboot_snor_android.cfg:quickboot_snor_android.cfg
endif

ifeq ($(wildcard vendor/nvidia/tegra/embedded/tools),vendor/nvidia/tegra/embedded/tools)
PRODUCT_COPY_FILES += \
    $(NV_VCM_FLASH_SCRIPT_PATH)/scripts/bootburn/bootburn.sh:bootburn.sh \
    $(NV_VCM_FLASH_SCRIPT_PATH)/scripts/bootburn/bootburn_adb.sh:bootburn_adb.sh \
    $(NV_VCM_FLASH_SCRIPT_PATH)/boards/vcm30t124/bootburn_helper.sh:bootburn_helper.sh \
    $(NV_VCM_FLASH_SCRIPT_PATH)/utils/flashramdisk.img:flashramdisk.img \
    $(NV_VCM_FLASH_SCRIPT_PATH)/utils/e2fsck:e2fsck \
    $(NV_VCM_FLASH_SCRIPT_PATH)/utils/resize2fs:resize2fs \
    $(NV_VCM_FLASH_SCRIPT_PATH)/utils/wr_sh.sh:wr_sh.sh \
    $(NV_VCM_FLASH_SCRIPT_PATH)/utils/GP1.img:GP1.img
endif

ifeq ($(wildcard vendor/nvidia/tegra/core-private),vendor/nvidia/tegra/core-private)
    NVFLASH_FILES_PATH := vendor/nvidia/tegra/bootloader/nvbootloader/odm-partner/ardbeg
else
    NVFLASH_FILES_PATH := vendor/nvidia/tegra/odm/ardbeg
endif

PRODUCT_COPY_FILES += \
    $(NVFLASH_FILES_PATH)/nvflash/PM358_Hynix_2GB_H5TC4G63AFR_RDA_792MHz.cfg:flash_pm358_792.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/PM359_Hynix_2GB_H5TC4G63AFR_RDA_792MHz.cfg:flash_pm359_792.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1780_Hynix_2GB_H5TC4G63AFR_RDA_408Mhz.cfg:flash_e1780_408.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1780_Hynix_2GB_H5TC4G63AFR_RDA_792Mhz.cfg:flash_e1780_792.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1780_Hynix_4GB_H5TC8G63AMR_PBA_792Mhz.cfg:flash_e1780_4gb_792.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1780_Hynix_2GB_H5TC4G63AFR_RDA_924Mhz.cfg:flash_e1780_924.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1782_Hynix_4GB_H5TC8G63AMR_PBA_792Mhz.cfg:flash_e1782_4gb_792.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1782_Hynix_2GB_H5TC4G63AFR_RDA_924Mhz.cfg:flash_e1782_924.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1782_Hynix_4GB_H5TC8G63AMR_PBA_792Mhz_spi.cfg:flash_e1782_4gb_792_spi.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1782_Hynix_2GB_H5TC4G63AFR_RDA_924Mhz_spi.cfg:flash_e1782_924_spi.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1792_Elpida_2GB_EDFA164A2MA_JD_F_792MHz.cfg:flash_e1792_792.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1792_Elpida_2GB_EDFA164A2MA_JD_F_924MHz.cfg:flash_e1792_924.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1791_Elpida_4GB_edfa232a2ma_792MHz.cfg:flash_e1791_4gb_792.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1791_Elpida_4GB_edfa232a2ma_924MHz.cfg:flash_e1791_4gb_924.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1761_Hynix_4GB_H5TC4G63AFR_PBA_792Mhz.cfg:flash_e1761_4gb_792.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1761_Hynix_2GB_H5TC4G63AFR_PBA_792Mhz.cfg:flash_e1761_2gb_792.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1761_Hynix_2GB_H5TC4G63AFR_RDA_924MHz.cfg:flash_e1761_2gb_924.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1922_samsung_pop_3GB_K3QF6F60MM_924MHz.cfg:flash_e1922_3gb_924.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E1923_samsung_pop_3GB_K3QF6F60MM_924MHz.cfg:flash_e1923_3gb_924.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/eks_nokey.dat:eks.dat \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_ardbeg.txt:NCT_ardbeg.txt \
    $(NVFLASH_FILES_PATH)/nvflash/nct_tn8.txt:nct_tn8.txt \
    $(NVFLASH_FILES_PATH)/nvflash/nct_tn8-ffd.txt:nct_tn8-ffd.txt \
    $(NVFLASH_FILES_PATH)/partition_data/config/nvcamera.conf:system/etc/nvcamera.conf \
    $(NVFLASH_FILES_PATH)/nvflash/common_bct.cfg:bct.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/fuse_write.txt:fuse_write.txt \
    $(NVFLASH_FILES_PATH)/nvflash/tn8_bootsplash.bmp:tn8_bootsplash.bmp \
    $(NVFLASH_FILES_PATH)/nvflash/tn8_bootsplash_land.bmp:tn8_bootsplash_land.bmp \
    $(NVFLASH_FILES_PATH)/nvflash/lowbat.bmp:lowbat.bmp \
    $(NVFLASH_FILES_PATH)/nvflash/charging.bmp:charging.bmp \
    $(NVFLASH_FILES_PATH)/nvflash/charged.bmp:charged.bmp \
    $(NVFLASH_FILES_PATH)/nvflash/fullycharged.bmp:fullycharged.bmp \
    $(NVFLASH_FILES_PATH)/nvflash/charging.png:root/res/images/charger/charging.png \
    $(NVFLASH_FILES_PATH)/nvflash/charged.png:root/res/images/charger/charged.png \
    $(NVFLASH_FILES_PATH)/nvflash/fullycharged.png:root/res/images/charger/fullycharged.png \
    $(NVFLASH_FILES_PATH)/nvflash/nvbdktest_plan.txt:nvbdktest_plan.txt

ifeq ($(APPEND_DTB_TO_KERNEL), true)
PRODUCT_COPY_FILES += \
    $(NVFLASH_FILES_PATH)/nvflash/android_fastboot_nvtboot_emmc_full.cfg:flash.cfg
    $(NVFLASH_FILES_PATH)/nvflash/android_fastboot_nvtboot_spi_sata_full.cfg:flash_spi_sata.cfg
else
ifeq ($(BUILD_NV_CRASHCOUNTER),true)
PRODUCT_COPY_FILES += \
    $(NVFLASH_FILES_PATH)/nvflash/android_cc_fastboot_dtb_emmc_full.cfg:flash.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/android_cc_fastboot_nvtboot_dtb_spi_sata_full.cfg:flash_spi_sata.cfg
else
PRODUCT_COPY_FILES += \
    $(NVFLASH_FILES_PATH)/nvflash/android_fastboot_nvtboot_nct_dtb_emmc_full.cfg:flash.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/android_fastboot_nvtboot_dtb_spi_sata_full.cfg:flash_spi_sata.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/android_fastboot_nvtboot_nct_dtb_emmc_full.cfg:flash_nct.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/laguna_android_fastboot_nvtboot_dtb_emmc_full.cfg:laguna_flash.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/tn8_android_fastboot_nvtboot_dtb_emmc_full.cfg:tn8_flash.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/tn8_android_fastboot_nvtboot_dtb_emmc_full_signed.cfg:tn8_flash_signed.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/tn8_android_fastboot_nvtboot_dtb_emmc_full_mfgtest.cfg:tn8diag_flash.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/tn8_android_fastboot_nvtboot_dtb_emmc_full_mfgtest_signed.cfg:tn8diag_flash_signed.cfg

NVFLASH_CFG_BASE_FILE := $(NVFLASH_FILES_PATH)/nvflash/tn8_android_fastboot_nvtboot_dtb_emmc_full.cfg
endif
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/jetson_android_fastboot_nvtboot_dtb_emmc_full.cfg:jetson_flash.cfg

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/flash/fastboot.bin:fastboot.bin \
    $(LOCAL_PATH)/flash/nvflash:nvflash \
    $(LOCAL_PATH)/flash/PM375_Hynix_2GB_H5TC4G63AFR_RDA_924MHz.cfg:flash_pm375_924.cfg

NVFLASH_FILES_PATH :=

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/tablet_core_hardware.xml:system/etc/permissions/tablet_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.hardware.camera.xml:system/etc/permissions/android.hardware.camera.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.nfc.xml:system/etc/android.hardware.nfc.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:system/etc/permissions/android.hardware.bluetooth_le.xml

# OPENGL AEP permission file
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:system/etc/permissions/android.hardware.opengles.aep.xml

PRODUCT_COPY_FILES += \
    $(call add-to-product-copy-files-if-exists,frameworks/native/data/etc/android.hardware.ethernet.xml:system/etc/permissions/android.hardware.ethernet.xml)

PRODUCT_COPY_FILES += \
  $(LOCAL_PATH)/ueventd.ardbeg.rc:root/ueventd.ardbeg.rc \
  $(LOCAL_PATH)/ueventd.ardbeg.rc:root/ueventd.laguna.rc \
  $(LOCAL_PATH)/ueventd.ardbeg.rc:root/ueventd.tn8.rc \
  $(LOCAL_PATH)/ueventd.ardbeg.rc:root/ueventd.jetson-tk1.rc \
  $(LOCAL_PATH)/ueventd.ardbeg.rc:root/ueventd.ardbeg_sata.rc \
  $(LOCAL_PATH)/tegra-kbc.kl:system/usr/keylayout/tegra-kbc.kl \
  $(LOCAL_PATH)/gpio-keys.kl:system/usr/keylayout/gpio-keys.kl \
  $(LOCAL_PATH)/Vendor_0955_Product_7210.kl:system/usr/keylayout/Vendor_0955_Product_7210.kl \
  $(LOCAL_PATH)/../../common/dhcpcd.conf:system/etc/dhcpcd/dhcpcd.conf \
  $(LOCAL_PATH)/../../drivers/touchscreen/raydium_ts.idc:system/usr/idc/touch.idc \
  $(LOCAL_PATH)/../../drivers/touchscreen/sensor00fn11.idc:system/usr/idc/sensor00fn11.idc \
  $(LOCAL_PATH)/../../common/wifi_loader.sh:system/bin/wifi_loader.sh \
  $(LOCAL_PATH)/../../common/init.comms.rc:root/init.comms.rc \
  $(LOCAL_PATH)/../../common/init.ussrd.rc:root/init.ussrd.rc \
  $(LOCAL_PATH)/../../common/wpa_supplicant.sh:system/bin/wpa_supplicant.sh \
  $(LOCAL_PATH)/../../common/gps_select.sh:system/bin/gps_select.sh \
  $(LOCAL_PATH)/../../common/ussr_setup.sh:system/bin/ussr_setup.sh \
  $(LOCAL_PATH)/ussrd.conf:system/etc/ussrd.conf \
  $(LOCAL_PATH)/../../common/set_hwui_params.sh:system/bin/set_hwui_params.sh \
  $(LOCAL_PATH)/../../common/comms/marvel_wpa.conf:/system/etc/firmware/marvel_wpa.conf \
  $(LOCAL_PATH)/../../common/comms/marvel_p2p.conf:/system/etc/firmware/marvel_p2p.conf \
  $(LOCAL_PATH)/../../drivers/comms/brcm_wpa.conf:/system/etc/firmware/brcm_wpa.conf \
  $(LOCAL_PATH)/../../drivers/comms/brcm_p2p.conf:/system/etc/firmware/brcm_p2p.conf

ifeq ($(PLATFORM_IS_AFTER_KITKAT),1)
ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
PRODUCT_COPY_FILES += \
    frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:system/etc/media_codecs_google_audio.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:system/etc/media_codecs_google_video.xml \
    $(LOCAL_PATH)/media_profiles.xml:system/etc/media_profiles.xml \
    $(LOCAL_PATH)/media_codecs.xml:system/etc/media_codecs.xml \
    $(LOCAL_PATH)/audio_policy.conf:system/etc/audio_policy.conf
else
PRODUCT_COPY_FILES += \
    frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:system/etc/media_codecs_google_audio.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:system/etc/media_codecs_google_video.xml \
    $(LOCAL_PATH)/media_profiles_noenhance.xml:system/etc/media_profiles.xml \
    $(LOCAL_PATH)/media_codecs_noenhance.xml:system/etc/media_codecs.xml \
    $(LOCAL_PATH)/audio_policy_noenhance.conf:system/etc/audio_policy.conf
endif
else
ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/media_profiles_kk.xml:system/etc/media_profiles.xml \
    $(LOCAL_PATH)/media_codecs_kk.xml:system/etc/media_codecs.xml \
    $(LOCAL_PATH)/audio_policy_kk.conf:system/etc/audio_policy.conf
else
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/media_profiles_noenhance.xml:system/etc/media_profiles.xml \
    $(LOCAL_PATH)/media_codecs_noenhance_kk.xml:system/etc/media_codecs.xml \
    $(LOCAL_PATH)/audio_policy_kk.conf:system/etc/audio_policy.conf
endif
endif

# t124 SOC rc files
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/../../common/init.tegra.rc:root/init.tegra.rc \
    $(LOCAL_PATH)/../../soc/t124/init.t124.rc:root/init.t124.rc \
    $(LOCAL_PATH)/../../common/init.tegra_sata.rc:root/init.tegra_sata.rc \
    $(LOCAL_PATH)/../../common/init.tegra_emmc.rc:root/init.tegra_emmc.rc

# p1859 rc files
ifneq ($(wildcard $(LOCAL_PATH)/../p1859),)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/../p1859/init.tegra.p1859.rc:root/init.tegra.p1859.rc \
    $(LOCAL_PATH)/../p1859/init.tegra_emmc.p1859.rc:root/init.tegra_emmc.p1859.rc \
    $(LOCAL_PATH)/../p1859/init.icera.p1859.rc:root/init.icera.p1859.rc \
    $(LOCAL_PATH)/../p1859/rebindEthernet.sh:system/bin/rebindEthernet.sh \
    $(LOCAL_PATH)/../p1859/bt_vendor.p1859.conf:system/etc/bluetooth/bt_vendor.p1859.conf \
    $(LOCAL_PATH)/../p1859/ueventd.p1859.rc:root/ueventd.p1859.rc \
    $(LOCAL_PATH)/../p1859/power.p1859.rc:system/etc/power.p1859.rc \
    $(LOCAL_PATH)/../p1859/init.p1859.rc:root/init.p1859.rc \
    $(LOCAL_PATH)/../p1859/fstab.p1859:root/fstab.p1859
endif

# ardbeg rc files
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/power.ardbeg.rc:system/etc/power.ardbeg.rc \
    $(LOCAL_PATH)/power.tn8.rc:system/etc/power.tn8.rc \
    $(LOCAL_PATH)/power.jetson-tk1.rc:system/etc/power.jetson-tk1.rc \
    $(LOCAL_PATH)/init.ardbeg.rc:root/init.ardbeg.rc \
    $(LOCAL_PATH)/init.laguna.rc:root/init.laguna.rc \
    $(LOCAL_PATH)/init.tn8.rc:root/init.tn8.rc \
    $(LOCAL_PATH)/init.jetson-tk1.rc:root/init.jetson-tk1.rc \
    $(LOCAL_PATH)/init.recovery.tn8.rc:root/init.recovery.tn8.rc \
    $(LOCAL_PATH)/init.recovery.jetson-tk1.rc:root/init.recovery.jetson-tk1.rc \
    $(LOCAL_PATH)/init.tn8_common.rc:root/init.tn8_common.rc \
    $(LOCAL_PATH)/init.jetson-tk1_common.rc:root/init.jetson-tk1_common.rc \
    $(LOCAL_PATH)/init.tn8_emmc.rc:root/init.tn8_emmc.rc \
    $(LOCAL_PATH)/init.jetson-tk1_emmc.rc:root/init.jetson-tk1_emmc.rc \
    $(LOCAL_PATH)/init.ardbeg_sata.rc:root/init.ardbeg_sata.rc \
    $(LOCAL_PATH)/fstab.ardbeg:root/fstab.ardbeg \
    $(LOCAL_PATH)/fstab.tn8:root/fstab.tn8 \
    $(LOCAL_PATH)/fstab.jetson-tk1:root/fstab.jetson-tk1 \
    $(LOCAL_PATH)/fstab.laguna:root/fstab.laguna \
    $(LOCAL_PATH)/fstab.ardbeg_sata:root/fstab.ardbeg_sata \
    $(LOCAL_PATH)/init.tn8.usb.rc:root/init.tn8.usb.rc \
    $(LOCAL_PATH)/init.jetson-tk1.usb.rc:root/init.jetson-tk1.usb.rc \
    $(LOCAL_PATH)/../../common/init.nv_dev_board.usb.rc:root/init.nv_dev_board.usb.rc \
    $(LOCAL_PATH)/../../common/init.tlk.rc:root/init.tlk.rc \
    $(LOCAL_PATH)/../../common/init.hdcp.rc:root/init.hdcp.rc

ifeq ($(NO_ROOT_DEVICE),1)
  PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init_no_root_device.rc:root/init.rc
endif

# Face detection model
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/core/include/ft/model_frontalface.xml:system/etc/model_frontal.xml

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/../../common/cluster:system/bin/cluster \
    $(LOCAL_PATH)/../../common/cluster_get.sh:system/bin/cluster_get.sh \
    $(LOCAL_PATH)/../../common/cluster_set.sh:system/bin/cluster_set.sh \
    $(LOCAL_PATH)/../../common/dcc:system/bin/dcc \
    $(LOCAL_PATH)/../../common/hotplug:system/bin/hotplug \
    $(LOCAL_PATH)/../../common/mount_debugfs.sh:system/bin/mount_debugfs.sh

PRODUCT_COPY_FILES += \
    device/nvidia/platform/ardbeg/nvcms/device.cfg:system/lib/nvcms/device.cfg

PRODUCT_COPY_FILES += \
    device/nvidia/common/bdaddr:system/etc/bluetooth/bdaddr \
    device/nvidia/platform/ardbeg/audioConfig_qvoice_icera_pc400.xml:system/etc/audioConfig_qvoice_icera_pc400.xml \
    device/nvidia/platform/ardbeg/nvaudio_conf.xml:system/etc/nvaudio_conf.xml \
    device/nvidia/platform/ardbeg/nvaudio_fx.xml:system/etc/nvaudio_fx.xml

PRODUCT_COPY_FILES += \
    device/nvidia/platform/ardbeg/enctune.conf:system/etc/enctune.conf

ifeq ($(PLATFORM_IS_NEXT),1)
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/bcm4752_next/glgps_nvidiategraandroid:system/bin/glgps_nvidiaTegra2android \
    vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/bcm4752_next/gps.nvidiategraandroid.so:system/lib/hw/gps.brcm.so
else
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/bcm4752/glgps_nvidiaTegra2android:system/bin/glgps_nvidiaTegra2android \
    vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/bcm4752/gps.tegra.so:system/lib/hw/gps.brcm.so
endif

PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/gps.conf:system/etc/gps.conf

ifneq ($(wildcard vendor/nvidia/tegra/core-private),)
    BCMBINARIES_PATH := vendor/nvidia/tegra/3rdparty/bcmbinaries
else ifneq ($(wildcard vendor/nvidia/tegra/prebuilt/ardbeg/3rdparty/bcmbinaries),)
    BCMBINARIES_PATH := vendor/nvidia/tegra/prebuilt/ardbeg/3rdparty/bcmbinaries
else ifneq ($(wildcard vendor/nvidia/tegra/prebuilt/shieldtablet/3rdparty/bcmbinaries),)
    BCMBINARIES_PATH := vendor/nvidia/tegra/prebuilt/shieldtablet/3rdparty/bcmbinaries
endif

PRODUCT_COPY_FILES += \
    $(BCMBINARIES_PATH)/bcm43241/bluetooth/AB113_BCM43241B0_0012_Azurewave_AW-AH691_TEST.HCD:system/etc/firmware/bcm43241.hcd \
    $(BCMBINARIES_PATH)/bcm43241/wlan/sdio-ag-pno-p2p-proptxstatus-dmatxrc-rxov-pktfilter-keepalive-aoe-vsdb-wapi-wl11d-sr-srvsdb-opt1.bin:system/vendor/firmware/bcm43241/fw_bcmdhd.bin \
    $(BCMBINARIES_PATH)/bcm43241/wlan/bcm943241ipaagb_p100_hwoob.txt:system/etc/nvram_43241.txt \
    $(BCMBINARIES_PATH)/bcm43241-b4/wlan/nvram_43241.txt:system/etc/nvram_43241-b4.txt \
    $(BCMBINARIES_PATH)/bcm43241-b4/wlan/sdio-ag-pno-p2p-proptxstatus-dmatxrc-rxov-pktfilter-keepalive-aoe-lpc-wl11u-vsdb-wapi-wl11d-sr-srvsdb-tdls-opt1-autoabn-pclose-reassocwar.bin:system/vendor/firmware/bcm43241-b4/fw_bcmdhd.bin \
    $(BCMBINARIES_PATH)/bcm43241-b4/bluetooth/BCM4324B3_002.004.006.0130.0000_Azurewave_AW-AH691A_TEST_ONLY.HCD:system/etc/firmware/BCM4324B3.hcd \
    $(BCMBINARIES_PATH)/bcm43341/bluetooth/BCM43341A0_001.001.030.0015.0000_Generic_UART_37_4MHz_wlbga_iTR_Pluto_Evaluation_for_NVidia.hcd:system/etc/firmware/BCM43341A0_001.001.030.0015.0000.hcd \
    $(BCMBINARIES_PATH)/bcm43341/bluetooth/BCM43341B0_002.001.014.0008.0011.hcd:system/etc/firmware/BCM43341B0_002.001.014.0008.0011.hcd \
    $(BCMBINARIES_PATH)/bcm43341/wlan/sdio-ag-pno-pktfilter-keepalive-aoe-idsup-idauth-wme.bin:system/vendor/firmware/bcm43341/fw_bcmdhd.bin \
    $(BCMBINARIES_PATH)/bcm43341/wlan/sdio-ag-pno-p2p-proptxstatus-dmatxrc-rxov-pktfilter-keepalive-aoe-sr-wapi-wl11d.bin:system/vendor/firmware/bcm43341/fw_bcmdhd_a0.bin \
    $(BCMBINARIES_PATH)/bcm43341/wlan/bcm943341wbfgn_2_hwoob.txt:system/etc/nvram_rev2.txt \
    $(BCMBINARIES_PATH)/bcm43341/wlan/nvram_43341_rev3.txt:system/etc/nvram_rev3.txt \
    $(BCMBINARIES_PATH)/bcm43341/wlan/bcm943341wbfgn_4_hwoob.txt:system/etc/nvram_rev4.txt \
    $(BCMBINARIES_PATH)/bcm4335/bluetooth/BCM4335B0_002.001.006.0037.0046_ORC.hcd:system/etc/firmware/bcm4335.hcd \
    $(BCMBINARIES_PATH)/bcm4335/wlan/bcm94335wbfgn3_r04_hwoob.txt:system/etc/nvram_4335.txt \
    $(BCMBINARIES_PATH)/bcm4335/wlan/sdio-ag-pool-p2p-pno-pktfilter-keepalive-aoe-ccx-sr-vsdb-proptxstatus-lpc-wl11u-autoabn.bin:system/vendor/firmware/bcm4335/fw_bcmdhd.bin

# embedded p1859
PRODUCT_PACKAGES += \
    mkbootimg_embedded \
    quickboot \
    quickboot1.bin \
    rcmboot.bin \
    cpu_stage2.bin \
    nvml \
    nvimagegen \
    nvskuflash \
    nvskupreserve

# Nvidia Miracast
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/../../common/miracast/com.nvidia.miracast.xml:system/etc/permissions/com.nvidia.miracast.xml

# NvBlit JNI library
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/graphics-partner/android/frameworks/Graphics/com.nvidia.graphics.xml:system/etc/permissions/com.nvidia.graphics.xml

#enable Widevine drm
PRODUCT_PROPERTY_OVERRIDES += drm.service.enabled=true

# we have enough storage space to hold precise GC data
PRODUCT_TAGS += dalvik.gc.type-precise

PRODUCT_CHARACTERISTICS := tablet

# Set default USB interface
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    persist.sys.usb.config=mtp

DEVICE_PACKAGE_OVERLAYS := $(LOCAL_PATH)/../../common/overlay-common \
                           $(LOCAL_PATH)/../../product/tablet/overlay-tablet

# Enable secure USB debugging in user release build
ifeq ($(TARGET_BUILD_TYPE),release)
ifeq ($(TARGET_BUILD_VARIANT),user)
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.adb.secure=1
endif
endif

# Copy public/private tnspecs
ifeq ($(wildcard vendor/nvidia/ardbeg/tnspec.json),vendor/nvidia/ardbeg/tnspec.json)
PRODUCT_COPY_FILES += vendor/nvidia/ardbeg/tnspec.json:tnspec.json
else ifeq ($(wildcard $(LOCAL_PATH)/tnspec.json),$(LOCAL_PATH)/tnspec.json)
PRODUCT_COPY_FILES += $(LOCAL_PATH)/tnspec.json:tnspec-public.json
endif

## Blake firmware files
LOCAL_BLAKE_FW_PATH=vendor/nvidia/blake/firmware/firmware-bin
PRODUCT_COPY_FILES += \
    $(call add-to-product-copy-files-if-exists, $(LOCAL_BLAKE_FW_PATH)/nVidiaBlake-UPDATE.ozu:system/etc/firmware/nVidiaBlake-UPDATE.ozu) \
    $(call add-to-product-copy-files-if-exists, $(LOCAL_BLAKE_FW_PATH)/nVidiaBlake-UPDATE.xmg:system/etc/firmware/nVidiaBlake-UPDATE.xmg) \
    $(call add-to-product-copy-files-if-exists, $(LOCAL_BLAKE_FW_PATH)/touchpad_fw.bin:system/etc/firmware/touchpad_fw.bin) \
    $(call add-to-product-copy-files-if-exists, $(LOCAL_BLAKE_FW_PATH)/touchbutton_fw.bin:system/etc/firmware/touchbutton_fw.bin)
