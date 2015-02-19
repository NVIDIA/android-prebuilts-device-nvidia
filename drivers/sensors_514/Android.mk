# Copyright (C) 2008 The Android Open Source Project
# Copyright (c) 2013 - 2014, NVIDIA CORPORATION.  All rights reserved.
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
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors_514/mlsdk/driver/include
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors_514/mlsdk/driver/include/linux
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors_514/mlsdk/HAL/include
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors_514/mlsdk/mllite
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors_514/mlsdk/mllite/linux
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors_514/mlsdk/mpl
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libdl libcutils libutils
LOCAL_CPPFLAGS+=-DLINUX=1
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.nvs_input
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := nvs_input.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libsensors.base
LOCAL_CPPFLAGS+=-DLINUX=1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.mpl
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_CFLAGS += -DINVENSENSE_COMPASS_CAL
LOCAL_CFLAGS += -std=gnu++0x
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors_514/mlsdk/driver/include
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors_514/mlsdk/driver/include/linux
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors_514/mlsdk/HAL/include
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors_514/mlsdk/mllite
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors_514/mlsdk/mllite/linux
LOCAL_C_INCLUDES += device/nvidia/drivers/sensors_514/mlsdk/mpl
LOCAL_SRC_FILES := MPLSensor.cpp MPLSupport.cpp CompassSensor.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libdl libsensors.base \
			  libmllite libmplmpu libsensors.nvs_input
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
LOCAL_MODULE := libsensors.isl29018
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := isl29018.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libsensors.base
LOCAL_CPPFLAGS+=-DLINUX=1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.isl29028
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := isl29028.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libsensors.base libsensors.isl29018
LOCAL_CPPFLAGS+=-DLINUX=1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.iio.lights
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := lightsensor.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libcutils libsensors.base
LOCAL_CPPFLAGS+=-DLINUX=1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.max44005
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := max44005.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libsensors.base
LOCAL_CPPFLAGS+=-DLINUX=1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.bmpx80
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := bmpx80.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libsensors.base
LOCAL_CPPFLAGS+=-DLINUX=1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.ltr558als
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := ltr558als.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libsensors.base
LOCAL_CPPFLAGS+=-DLINUX=1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.cm3217
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := cm3217.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libsensors.base
LOCAL_CPPFLAGS+=-DLINUX=1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.cm3218
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := cm3218.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libsensors.base
LOCAL_CPPFLAGS+=-DLINUX=1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.hwmon
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := hwmon.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libsensors.base
LOCAL_CPPFLAGS+=-DLINUX=1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libsensors.tcs3772
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := tcs3772.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := liblog libsensors.base
LOCAL_CPPFLAGS+=-DLINUX=1
include $(NVIDIA_SHARED_LIBRARY)

subdir_makefiles := \
	$(LOCAL_PATH)/mlsdk/Android.mk

include $(subdir_makefiles)

endif # !TARGET_SIMULATOR
