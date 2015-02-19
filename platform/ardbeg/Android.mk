# Copyright (C) 2012 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifneq (,$(filter $(REFERENCE_DEVICE),ardbeg t132))
LOCAL_PATH := $(my-dir)
subdir_makefiles := \
	$(LOCAL_PATH)/comms/Android.mk \
	$(LOCAL_PATH)/sensors_all/Android.mk

ifndef PLATFORM_IS_NEXT
ifeq ($(BOARD_HAVE_LBH_SUPPORT), true)
LBH_MAKEFILE := $(TOP)/vendor/nvidia/fury/tools/lbh/AndroidLBH.mk
ifeq ($(wildcard $(LBH_MAKEFILE)),$(LBH_MAKEFILE))
ifeq ($(PRODUCT_MODEL),t132loki)
FURY_CODE_NAME := loki
else
FURY_CODE_NAME := tn8
endif
subdir_makefiles += \
        $(LBH_MAKEFILE)
endif
endif
endif

include $(subdir_makefiles)
endif
