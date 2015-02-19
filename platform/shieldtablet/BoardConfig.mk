TARGET_KERNEL_DT_NAME ?= tegra124-tn8

include device/nvidia/platform/ardbeg/BoardConfig.mk

TARGET_DEVICE := shieldtablet
TARGET_SYSTEM_PROP := device/nvidia/platform/ardbeg/system.prop

BOARD_BUILD_NVFLASH := false

APPFILTERCLASS := com.nvidia.appfilter
# Raydium library version to be used
RAYDIUM_PRODUCT_BRANCH := dev-kernel-shieldtablet8

# Double buffered display surfaces reduce memory usage, but will decrease performance.
# The default is to triple buffer the display surfaces.
BOARD_DISABLE_TRIPLE_BUFFERED_DISPLAY_SURFACES := true

# sepolicy
# try to detect AOSP master-based policy vs small KitKat policy
ifeq ($(wildcard external/sepolicy/lmkd.te),)
# KitKat based board specific sepolicy
BOARD_SEPOLICY_DIRS += device/nvidia/platform/shieldtablet/products/sepolicy/
else
# AOSP master based board specific sepolicy
BOARD_SEPOLICY_DIRS += device/nvidia/platform/shieldtablet/products/sepolicy_aosp/
endif

# OTA
TARGET_RECOVERY_FSTAB := device/nvidia/platform/ardbeg/fstab.tn8

# clear variables that ST8 doesn't need
TARGET_KERNEL_VCM_BUILD :=
TARGET_QUICKBOOT :=
TARGET_QUICKBOOT_PRODUCTION_CONFIG :=
QUICKBOOT_PRODUCTION_CONFIG :=
QUICKBOOT_DEFAULT_CONFIG :=
TARGET_BOOT_MEDIUM :=
QUICKBOOT_TARGET_OS :=
TARGET_QB_FLASH_TOOL :=
