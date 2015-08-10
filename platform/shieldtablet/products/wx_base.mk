# Copyright (c) 2014, NVIDIA Corporation.  All rights reserved.
#
# Shield Tablet 8 common makefile
#
# AndroidProducts.mk is included before BoardConfig.mk, variable essential at
# start of build and used in here should always be intialized in this file

## All essential packages
$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)
$(call inherit-product, device/nvidia/platform/ardbeg/device.mk)

## Thse are default settings, it gets changed as per sku manifest properties
PRODUCT_DEVICE := shieldtablet
PRODUCT_MANUFACTURER := NVIDIA
PRODUCT_BRAND := nvidia

## Fury LBH  feature
-include vendor/nvidia/fury/tools/lbh/fury.mk

# _product_private_path is cleared in wx_ product makefiles
_product_private_path := vendor/nvidia/shieldtablet

## ST8 Kernel
KERNEL_PATH=$(CURDIR)/kernel-shieldtablet8

## Jetson TK1 Kernel
KERNEL_PATH=$(CURDIR)/kernel

## common apps for all skus
$(call inherit-product-if-exists, $(_product_private_path)/wx_common.mk)
