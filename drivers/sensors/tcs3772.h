/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Copyright (c) 2013, NVIDIA Corporation. All Rights Reserved.
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

#ifndef ANDROID_TCS3772_H
#define ANDROID_TCS3772_H

#include "sensors.h"
#include "SensorBase.h"

#define TCS3772_INTEGRATION_TIME 700000000 /* 700 ms */
#define TCS3772_LUX_CONV_FACTOR 0.06

class Tcs3772Base : public SensorBase {
    bool mEnabled;
    int mLastValue;
    const char *mSysPath;

    int32_t mType;
    int64_t mLastns;
    int64_t mPollingDelay;
    const char *mSysValuePath;
    const char *mSysEnablePath;
    const char *mSysRegulatorEnablePath;
public:
            Tcs3772Base(const char *sysPath, int sid);
    virtual ~Tcs3772Base();
    virtual int readEvents(sensors_event_t *data, int count);
    virtual void toEvent(sensors_event_t &evt, int value);
    virtual bool equals(int64_t, int64_t);
    virtual bool hasPendingEvents() const;
    virtual int enable(int32_t handle, int enabled);
    virtual int setDelay(int32_t handle, int64_t ns);
};

/******************************************************************************/

class Tcs3772Light : public Tcs3772Base {
public:
             Tcs3772Light(const char *sysPath);
    ~Tcs3772Light();
    void toEvent(sensors_event_t &evt, int value);
};

/******************************************************************************/

class Tcs3772Prox : public Tcs3772Base {
    unsigned int mProxThreshold;
public:
             Tcs3772Prox(const char *sysPath, unsigned int proxThreshold);
    ~Tcs3772Prox();
    virtual bool equals(int64_t, int64_t);
    void toEvent(sensors_event_t &evt, int value);
};

#endif /* ANDROID_TCS3772_H */
