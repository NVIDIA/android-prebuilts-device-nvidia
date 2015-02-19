# Copyright (C) 2008 The Android Open Source Project
# Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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


LOCAL_PATH:= $(call my-dir)

ifneq ($(TARGET_SIMULATOR),true)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.base
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := SensorBase.cpp SensorUtil.cpp InputEventReader.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libdl libcutils libutils
LOCAL_CPPFLAGS+=-DLINUX=1
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.nvs
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := Nvs.cpp
LOCAL_SRC_FILES += Nvsb.cpp
LOCAL_SRC_FILES += NvsAccel.cpp
LOCAL_SRC_FILES += NvsAnglvel.cpp
LOCAL_SRC_FILES += NvsIlluminance.cpp
LOCAL_SRC_FILES += NvsMagn.cpp
LOCAL_SRC_FILES += NvsPressure.cpp
LOCAL_SRC_FILES += NvsProximity.cpp
LOCAL_SRC_FILES += NvsTemp.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libsensors.base
LOCAL_CPPFLAGS+=-DLINUX=1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.mpl
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_CFLAGS += -DINVENSENSE_COMPASS_CAL
LOCAL_CFLAGS += -std=gnu++0x
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors/mlsdk/driver/include
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors/mlsdk/mllite
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors/mlsdk/mllite/linux
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors/mlsdk/mpl
LOCAL_SRC_FILES := MPLSensor.cpp MPLSupport.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libdl libsensors.base \
			  libmllite libmplmpu libsensors.nvs
LOCAL_CPPFLAGS+=-DLINUX=1
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.adxl34x
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := adxl34x.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libsensors.base
LOCAL_CPPFLAGS+=-DLINUX=1
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.hwmon
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := hwmon.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libsensors.base
LOCAL_CPPFLAGS+=-DLINUX=1
include $(NVIDIA_SHARED_LIBRARY)

subdir_makefiles := \
	$(LOCAL_PATH)/mlsdk/Android.mk

include $(subdir_makefiles)

endif # !TARGET_SIMULATOR
