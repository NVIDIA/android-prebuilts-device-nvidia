TARGET_BOARD_PLATFORM := tegra
TARGET_TEGRA_VERSION := t124
TARGET_TEGRA_FAMILY := t12x

# CPU options
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_CPU_VARIANT := cortex-a15
TARGET_CPU_SMP := true

BOARD_BUILD_BOOTLOADER := true

TARGET_KERNEL_DT_NAME ?= tegra124-ardbeg

REFERENCE_DEVICE := ardbeg

ifeq ($(NO_ROOT_DEVICE),1)
  TARGET_PROVIDES_INIT_RC := true
else
  TARGET_PROVIDES_INIT_RC := false
endif

BOARD_USES_GENERIC_AUDIO := false
BOARD_USES_ALSA_AUDIO := true
ifeq ($(PLATFORM_IS_NEXT),1)
  USE_CUSTOM_AUDIO_POLICY := 1
endif
BOARD_SUPPORT_NVOICE := true
BOARD_SUPPORT_NVAUDIOFX := true

TARGET_USERIMAGES_USE_EXT4 := true
BOARD_USERDATAIMAGE_PARTITION_SIZE  := 12799754240
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 1342177280
BOARD_FLASH_BLOCK_SIZE := 4096

BOARD_SUPPORT_PARAGON_FUSE_UFSD := true

USE_OPENGL_RENDERER := true

# VCM Kernel
# FIXME: This shouldn't be set here. Should be set in product Makefile for VCM.
TARGET_KERNEL_VCM_BUILD ?= true

# Quickboot
TARGET_QUICKBOOT ?= true
TARGET_QUICKBOOT_PRODUCTION_CONFIG ?= true
QUICKBOOT_PRODUCTION_CONFIG := vcm30t124_nor_android_production_defconfig
QUICKBOOT_DEFAULT_CONFIG := vcm30t124_nor_android_defconfig
TARGET_BOOT_MEDIUM := nor
QUICKBOOT_TARGET_OS := android
# Quickboot flash tools
TARGET_QB_FLASH_TOOL := true

# OTA
TARGET_RECOVERY_UPDATER_LIBS += libnvrecoveryupdater
TARGET_RECOVERY_UI_LIB := librecovery_ui_default
TARGET_RECOVERY_UPDATER_EXTRA_LIBS += libfs_mgr
TARGET_RECOVERY_FSTAB = device/nvidia/platform/ardbeg/fstab.ardbeg

BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR ?= device/nvidia/platform/ardbeg/bluetooth
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := true

# powerhal
BOARD_USES_POWERHAL := true

# Wifi related defines
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
WPA_SUPPLICANT_VERSION      := VER_0_8_X
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_bcmdhd
BOARD_WLAN_DEVICE           := bcmdhd
BOARD_HOSTAPD_DRIVER        := NL80211
BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_bcmdhd
WIFI_DRIVER_FW_PATH_STA     := "/data/misc/wifi/firmware/fw_bcmdhd.bin"
WIFI_DRIVER_FW_PATH_AP      := "/data/misc/wifi/firmware/fw_bcmdhd_apsta.bin"
WIFI_DRIVER_FW_PATH_P2P     := "/data/misc/wifi/firmware/fw_bcmdhd_p2p.bin"
WIFI_DRIVER_FW_PATH_PARAM   := "/data/misc/wifi/firmware/firmware_path"
WIFI_DRIVER_MODULE_ARG      := "iface_name=wlan0"
WIFI_DRIVER_MODULE_NAME     := "bcmdhd"

# GPS
BOARD_GPS_LIBRARIES := true

# Default HDMI mirror mode
# Crop (default) picks closest mode, crops to screen resolution
# Scale picks closest mode, scales to screen resolution (aspect preserved)
# Center picks a mode greater than or equal to the panel size and centers;
#     if no suitable mode is available, reverts to scale
BOARD_HDMI_MIRROR_MODE := Scale

# NVDPS can be enabled when display is set to continuous mode.
BOARD_HAS_NVDPS := true

# This should be set to true for boards that support 3DVision.
BOARD_HAS_3DV_SUPPORT := true

# Allow this variable to be overridden to n for non-secure OS build
SECURE_OS_BUILD ?= y
ifeq ($(SECURE_OS_BUILD),y)
    SECURE_OS_BUILD := tlk
endif

# Use Nvidia optimized renderscript driver
OVERRIDE_RS_DRIVER := libnvRSDriver.so

# Double buffered display surfaces reduce memory usage, but will decrease performance.
# The default is to triple buffer the display surfaces.
# BOARD_DISABLE_TRIPLE_BUFFERED_DISPLAY_SURFACES := true

include device/nvidia/common/BoardConfig.mk

# Use CMU-style config with Nvcms
NVCMS_CMU_USE_CONFIG := false

-include 3rdparty/trustedlogic/samples/hdcp/tegra3/build/arm_android/config.mk

# Icera modem definitions
-include device/nvidia/common/icera/BoardConfigIcera.mk

# Raydium touch definitions
include device/nvidia/drivers/touchscreen/raydium/BoardConfigRaydium.mk

# Maxim touch definitions
include device/nvidia/drivers/touchscreen/maxim/BoardConfigMaxim.mk

# BOARD_WIDEVINE_OEMCRYPTO_LEVEL
# The security level of the content protection provided by the Widevine DRM plugin depends
# on the security capabilities of the underlying hardware platform.
# There are Level 1/2/3. To run HD contents, should be Widevine level 1 security.
BOARD_WIDEVINE_OEMCRYPTO_LEVEL := 1

# Dalvik option
DALVIK_ENABLE_DYNAMIC_GC := true

# Using the NCT partition
TARGET_USE_NCT := true
# LBH related defines
# use LBH partition and resources in it
BOARD_HAVE_LBH_SUPPORT := true

#Use tegra health HAL library
BOARD_HAL_STATIC_LIBRARIES := libhealthd.tegra

#Display static images for charging
BOARD_CHARGER_STATIC_IMAGE := true

# sepolicy
# try to detect AOSP master-based policy vs small KitKat policy
ifeq ($(wildcard external/sepolicy/lmkd.te),)
# KitKat based board specific sepolicy
BOARD_SEPOLICY_DIRS += device/nvidia/platform/ardbeg/sepolicy/
else
# AOSP master based board specific sepolicy
BOARD_SEPOLICY_DIRS += device/nvidia/platform/ardbeg/sepolicy_aosp/
endif

# Per-application sizes for shader cache
MAX_EGL_CACHE_SIZE := 4194304
MAX_EGL_CACHE_ENTRY_SIZE := 262144
