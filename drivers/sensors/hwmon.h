/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_TEMPERATURE_MONITOR_H
#define ANDROID_TEMPERATURE_MONITOR_H

#include "SensorBase.h"

#define ADT7461TEMP_DEF(HANDLE) {         \
    .name = "ADT7461 Temperature Monitor",\
    .vendor = "Analog Devices",           \
    .version = 1,                         \
    .handle = HANDLE,                     \
    .type = SENSOR_TYPE_TEMPERATURE,      \
    .maxRange = 90.0f,                    \
    .resolution = 1.0f,                   \
    .power = 0.17f,                       \
    .minDelay = 0,                        \
    .fifoReservedEventCount = 0,          \
    .fifoMaxEventCount = 0 }

class HwmonTemp : public SensorBase {
    bool mEnabled;
    bool mHasPendingEvent;
    float mLast_value;
    bool mAlready_warned;
    const char *mSysPath;
    int sid;

public:
            HwmonTemp(const char *mSysPath, int sensor_id);
    virtual ~HwmonTemp();
    virtual int readEvents(sensors_event_t* data, int count);
    virtual bool hasPendingEvents() const;
    virtual int enable(int32_t handle, int enabled);
    virtual int getFd() const;

};

#endif  // ANDROID_TEMPERATURE_MONITOR_H
