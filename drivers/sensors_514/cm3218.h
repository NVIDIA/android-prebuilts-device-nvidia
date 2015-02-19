/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Copyright (c) 2012-2013, NVIDIA Corporation. All Rights Reserved.
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

#ifndef ANDROID_LIGHT_SENSOR_H
#define ANDROID_LIGHT_SENSOR_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include "sensors.h"
#include "SensorBase.h"
#include "InputEventReader.h"


/*****************************************************************************/

#define CM3218LIGHT_DEF {                     \
    "CM3218 Light Sensor",                    \
    "Capella Microsystems",                   \
    1, ID_L,                                  \
    SENSOR_TYPE_LIGHT, 10240.0f, 1.0f,        \
    0.5f, 0, 0, 0, { } }

#define CM3218_INTEGRATION_TIME 600000000 /* 600 ms */
#define CM3218_LUX_CONV_FACTOR 0.009

class Cm3218Base : public SensorBase {
    bool mEnabled;
    unsigned int mLastValue;
    const char *mSysPath;

    int32_t mType;
    int64_t mLastns;
    int64_t mPollingDelay;
    const char *mSysValuePath;
    const char *mSysEnablePath;
    const char *mSysRegulatorEnablePath;
public:
            Cm3218Base(const char *sysPath, int sid);
    virtual ~Cm3218Base();
    virtual int readEvents(sensors_event_t *data, int count);
    virtual void toEvent(sensors_event_t &evt, int value);
    virtual bool equals(int64_t, int64_t);
    virtual bool hasPendingEvents() const;
    virtual int enable(int32_t handle, int enabled);
    virtual int setDelay(int32_t handle, int64_t ns);
};

/******************************************************************************/

class Cm3218Light : public Cm3218Base {
    float mLuxConvFactor;
public:
             Cm3218Light(const char *sysPath);
             Cm3218Light(const char *sysPath, float luxConvFactor);
    ~Cm3218Light();
    void toEvent(sensors_event_t &evt, int value);
};

/******************************************************************************/

#endif  // ANDROID_LIGHT_SENSOR_H
