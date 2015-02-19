#
# Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
#

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#cd $(TOP)/vender/nvidia/fury
FURY_CONFIG_PATH := vendor/nvidia/fury/configs/tn8/
FURY_CONFIG_NAMES := $(shell cd $(TOP)/$(FURY_CONFIG_PATH); ls -d *)

nvflash_cfg_default := tn8_default

nvflash_cfg_default_target := $(PRODUCT_OUT)/flash.cfg

define nvflash-cfg-populate-lbh
$(foreach _lbh,$(FURY_CONFIG_NAMES), \
  $(eval _nct_source := $(FURY_CONFIG_PATH)/$(_lbh)/nct_$(_lbh).txt) \
  if [ -z $(_nct_source) ]; then \
      echo -e "\nWarning: file $(_nct_src) does not exist, please check your configuration and script!\n"; \
  else \
  cp $(_nct_source) $(PRODUCT_OUT)/nct_$(_lbh).txt; \
  fi;\
  $(eval _target := $(PRODUCT_OUT)/flash_$(_lbh).cfg) \
  $(eval _out2 := $(if $(call streq,$(_lbh),$(nvflash_cfg_default)),| tee $(nvflash_cfg_default_target),)) \
  mkdir -p $(dir $(_target)); \
  $(eval sedoption := -e "s/filename=lbh.img/filename=lbh_$(_lbh).img/" \
	-e "s/filename=nct_tn8.txt//")
  sed $(sedoption) < $(NVFLASH_CFG_BASE_FILE) $(_out2) > $(_target); \
)
endef

LOCAL_MODULE := nvflash_cfg_populator
LOCAL_MODULE_CLASS := FAKE
LOCAL_MODULE_SUFFIX := -timestamp
include $(BUILD_SYSTEM)/base_rules.mk
$(LOCAL_BUILT_MODULE): $(NVFLASH_CFG_BASE_FILE)
	$(call nvflash-cfg-populate-lbh) \
	mkdir -p $(dir $@); \
	rm -rf $@; \
	touch $@

#$(hide) $(call nvflash-cfg-populate-lbh) \

endif # !TARGET_SIMULATOR
