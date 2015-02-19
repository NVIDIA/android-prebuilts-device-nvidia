# NVIDIA Tegra5 "Ardbeg" development system
#
# Copyright (c) 2013-2014, NVIDIA Corporation.  All rights reserved.
#
# AndroidProducts.mk is included before BoardConfig.mk, variable essential at
# start of build and used in here should always be intialized in this file

## All essential packages
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_no_telephony.mk)

include $(LOCAL_PATH)/wx_base.mk

## Thse are default settings, it gets changed as per sku manifest properties
PRODUCT_NAME := wx_na_wf
PRODUCT_MODEL := wx_na_wf

## Value of PRODUCT_NAME is mangeled before it can be
## used because of call to inherits, store their values to
## use later in this file below
_product_name := $(strip $(PRODUCT_NAME))

## SKU specific overrides
PRODUCT_COPY_FILES += \
    device/nvidia/common/init.none.rc:root/init.none.rc

PRODUCT_PROPERTY_OVERRIDES += ro.radio.noril=true

## GPS configuration
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/gpsconfig-wf-ardbeg.xml:system/etc/gps/gpsconfig.xml

PRODUCT_PACKAGE_OVERLAYS += device/nvidia/product/tablet/overlay-tablet

## common settings for all sku except for diag
include $(LOCAL_PATH)/wx_common_user.mk

## common apps for all skus
$(call inherit-product-if-exists, $(_product_private_path)/wx_common_user.mk)

## nvidia apps for this sku
$(call inherit-product-if-exists, $(_product_private_path)/$(_product_name).mk)

## Clean local variables
_product_name :=
_product_private_path :=
