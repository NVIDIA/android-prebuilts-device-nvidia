# NVIDIA Tegra4 "Ardbeg" development system
#
# Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
# Product to build bootloader

PRODUCT_NAME := ardbegbl
PRODUCT_DEVICE := ardbeg
PRODUCT_MODEL := ardbeg
PRODUCT_MANUFACTURER := NVIDIA
PRODUCT_BRAND := nvidia

## The base dtb file name used for this product
TARGET_KERNEL_DT_NAME := tegra124-ardbeg

# Disable dependency from libcompiler rt and kernel
WITHOUT_LIBCOMPILER_RT := 1
TARGET_NO_KERNEL := true
