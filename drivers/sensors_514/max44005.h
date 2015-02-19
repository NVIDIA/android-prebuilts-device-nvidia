/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation. All Rights Reserved.
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


#ifndef ANDROID_MAX44005_SENSOR_H
#define ANDROID_MAX44005_SENSOR_H

#include "sensors.h"
#include "SensorBase.h"

#define MAX44005LIGHT_DEF(handle) {           \
    "MAX44005 Light sensor", \
    "MAXIM",                               \
    1, handle,                                \
    SENSOR_TYPE_LIGHT, 57452.0f, 1.0f,         \
    0.09f, 0, 0, 0, { } }

#define MAX44005PROX_DEF(handle) {            \
    "MAX44005 Proximity sensor",     \
    "MAXIM",                               \
    1, handle,                                \
    SENSOR_TYPE_PROXIMITY, 1.0f, 1.0f,        \
    0.40f, 0, 0, 0, { } }

#define MIN_POLL_DELAY 200000000
#define CLEAR_LUX_CONVERSION_FACTOR (57452.0/32767.0)

#define MAX44005_ALS_SYSFS_PATH "/sys/class/sensors/light/device/amb_clear"
#define MAX44005_RESOLUTION_PATH "/sys/class/sensors/light/device/als_resolution"

class Max44005SensorBase : public SensorBase {
    bool mEnabled;
    int mLast_value;
    const char *mSysPath;
    int32_t mType;
    int64_t mLastns;
    int64_t mPollingDelay;
protected:
    int sid;
public:
            Max44005SensorBase(const char *sysPath, int sensor_id);
    virtual ~Max44005SensorBase();
    virtual int readEvents(sensors_event_t *data, int count);
    virtual void toEvent(sensors_event_t &evt, int value) = 0;
    virtual bool equals(int64_t, int64_t);
    virtual bool hasPendingEvents() const;
    virtual int enable(int32_t handle, int enabled);
    virtual int setDelay(int32_t handle, int64_t ns);
};

class Max44005Light : public Max44005SensorBase {
    float mResolution;
public:
            Max44005Light(const char *sysPath, int sensor_id);
    virtual ~Max44005Light();
    virtual void toEvent(sensors_event_t &evt, int value);
    static void fillSensorDef(sensor_t *ssensor_list, int &curIndex);
    static SensorBase* getInstance();
};

class Max44005Prox : public Max44005SensorBase {
    unsigned int mProximityThreshold;
public:
            Max44005Prox(const char *sysPath, int sensor_id,
                         unsigned int ProxThreshold);
    virtual ~Max44005Prox();
    virtual void toEvent(sensors_event_t &evt, int value);
    virtual bool equals(int64_t, int64_t);
};

#endif //ANDROID_MAX44005_SENSOR_H
