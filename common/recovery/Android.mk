LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_SRC_FILES := nv_recovery_updater.c
LOCAL_C_INCLUDES += bootable/recovery
LOCAL_C_INCLUDES += system/core/fs_mgr/include

LOCAL_MODULE := libnvrecoveryupdater

LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
LOCAL_NVIDIA_NO_COVERAGE := true

include $(NVIDIA_STATIC_LIBRARY)
