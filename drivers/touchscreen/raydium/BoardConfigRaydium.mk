# Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

# This lists the SE Linux policies necessary to build a device using
# Raydium touch

# KK policies not supported
ifneq ($(PLATFORM_IS_AFTER_KITKAT),)

# Raydium touch policies
BOARD_SEPOLICY_DIRS += device/nvidia/drivers/touchscreen/raydium/sepolicy/
BOARD_SEPOLICY_UNION += \
	device.te \
	file.te \
	file_contexts \
	mediaserver.te \
	raydium.te \
	service.te \
	service_contexts \
	surfaceflinger.te \
	system_app.te \
	ueventd.te \

endif
