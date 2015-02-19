/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All Rights Reserved.
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

#include <fcntl.h>
#include <cutils/log.h>
#include <stdlib.h>
#include <linux/input.h>
#include <hardware/sensors.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include "tcs3772.h"
#include "SensorUtil.h"

using namespace std;

#define MAX_SENSOR_PATH 128

/******************************************************************************/

Tcs3772Base::Tcs3772Base(const char *sysPath,int sid)
    : SensorBase(NULL, NULL),
      mEnabled(false),
      mLastValue(0)
{
    char *sysValuePath = new char[MAX_SENSOR_PATH];
    char *sysEnablePath = new char[MAX_SENSOR_PATH];
    char *sysRegulatorEnablePath = new char[MAX_SENSOR_PATH];
    char *in_name = new char[MAX_SENSOR_PATH];
    memset(sysValuePath, 0, MAX_SENSOR_PATH);
    memset(sysEnablePath, 0, MAX_SENSOR_PATH);
    memset(sysRegulatorEnablePath, 0, MAX_SENSOR_PATH);
    in_name= strcpy(in_name, sysPath);
    if (sid == ID_L)
        in_name = strcat(in_name, "/in_illuminance");
    else if (sid == ID_P)
        in_name = strcat(in_name, "/in_proximity");

        sysValuePath = strcpy(sysValuePath, in_name);
        sysValuePath = strcat(sysValuePath, "_raw");
        mSysValuePath = sysValuePath;
        sysEnablePath = strcpy(sysEnablePath, in_name);
        sysEnablePath = strcat(sysEnablePath, "_enable");
        mSysEnablePath = sysEnablePath;
        sysRegulatorEnablePath = strcpy(sysRegulatorEnablePath, in_name);
        sysRegulatorEnablePath = strcat(sysRegulatorEnablePath, "_regulator_enable");
        mSysRegulatorEnablePath = sysRegulatorEnablePath;
}

Tcs3772Base::~Tcs3772Base() {
}

int Tcs3772Base::enable(int32_t handle, int en) {
    if ((en != 0) && (en != 1))
        return -EINVAL;

    if (en == mEnabled)
        return 0;

    if (en) {
        int ret = writeIntToFile(mSysRegulatorEnablePath, en);

        if (ret <= 0)
            return ret;

        ret = writeIntToFile(mSysEnablePath, en);

        if (ret <= 0)
            return ret;

        mLastValue = -1;
        mLastns = getTimestamp();
    } else {
        int ret = writeIntToFile(mSysEnablePath, en);

        if (ret <= 0)
            return ret;

        ret = writeIntToFile(mSysRegulatorEnablePath, en);

        if (ret <= 0)
            return ret;
    }
    mEnabled = en;

    return 0;
}

int Tcs3772Base::setDelay(int32_t handle, int64_t ns) {
    mPollingDelay = ns < TCS3772_INTEGRATION_TIME ?
                                                TCS3772_INTEGRATION_TIME : ns;
    return 0;
}

bool Tcs3772Base::hasPendingEvents() const {
    if (!mEnabled)
        return false;

    if ((getTimestamp() - mLastns) < mPollingDelay)
        return false;

    return true;
}

bool Tcs3772Base::equals(int64_t val1, int64_t val2) {
    return val1 == val2;
}

void Tcs3772Base::toEvent(sensors_event_t &ev, int val) {
// Not used at all, just a dummy
}

int Tcs3772Base::readEvents(sensors_event_t* data, int count) {
    unsigned int value = 0;
    int ret = 0;

    if (count < 1 || data == NULL || !mEnabled) {
        ALOGV("Will not work on zero count(%i) or null pointer\n", count);
        return 0;
    }

    if (!hasPendingEvents())
        return 0;

    ret = readIntFromFile(mSysValuePath, &value);
    if (ret <= 0) {
        ALOGV("read from %s failed", mSysValuePath);
        return 0;
    }

    if (equals(value, mLastValue))
        return 0;

    mLastValue = value;
    toEvent(*data, value);
    mLastns = (*data).timestamp;
    return 1;
}

/******************************************************************************/

Tcs3772Light::Tcs3772Light(const char *sysPath)
    : Tcs3772Base(sysPath, ID_L)
{
}

Tcs3772Light::~Tcs3772Light() {
}

void Tcs3772Light::toEvent(sensors_event_t &evt, int value) {
    evt.version = sizeof(sensors_event_t);
    evt.sensor = ID_L;
    evt.type = SENSOR_TYPE_LIGHT;
    evt.light = (float)value * TCS3772_LUX_CONV_FACTOR;
    evt.timestamp = getTimestamp();
    ALOGV("LightSensor: time is %llu", evt.timestamp );
    ALOGV("LightSensor: value is %i", value );
    ALOGV("LightSensor: light_value is %f", evt.light );
}

/******************************************************************************/

Tcs3772Prox::Tcs3772Prox(const char *sysPath, unsigned int proxThreshold)
    : Tcs3772Base(sysPath, ID_P),
    mProxThreshold(proxThreshold)
{
}

Tcs3772Prox::~Tcs3772Prox() {
}

bool Tcs3772Prox::equals (int64_t val1, int64_t val2) {
    if ((val1 < 0) || (val2 < 0))
        return false;
    val1 = (unsigned int)val1 > mProxThreshold ? 0 : 1;
    val2 = (unsigned int)val2 > mProxThreshold ? 0 : 1;
    return val1 == val2;
}

void Tcs3772Prox::toEvent(sensors_event_t &evt, int value) {
    evt.version = sizeof(sensors_event_t);
    evt.sensor = ID_P;
    evt.type = SENSOR_TYPE_PROXIMITY;
    /* 0 = near; 1 = far. Based on how android expects */
    evt.distance = (unsigned int)value > mProxThreshold ? 0 : 1;
    evt.timestamp = getTimestamp();
    ALOGV("ProximitySensor: value is %i", value );
    ALOGV("ProximitySensor: time is %llu", evt.timestamp );
    ALOGV("ProximitySensor: distance_value is %f", evt.distance );
}
