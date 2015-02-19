/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All Rights Reserved.
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

#include "max44005.h"
#include "SensorUtil.h"

/*****************************************************************************/

Max44005SensorBase::Max44005SensorBase(const char *sysPath, int sensor_id)
    : SensorBase(NULL, NULL),
      mEnabled(false),
      mLast_value(-1),
      mSysPath(sysPath),
      mPollingDelay(MIN_POLL_DELAY),
      sid(sensor_id)
{
}

Max44005SensorBase::~Max44005SensorBase() {
}

int Max44005SensorBase::enable(int32_t handle, int en) {
    if ((en != 0) && (en != 1))
        return -EINVAL;

    if (en == mEnabled)
        return 0;

    int ret = writeIntToFile(mSysPath, en);

    if (ret <= 0)
        return ret;

    mEnabled = en;

    if (en) {
        mLast_value = -1;
        mLastns = getTimestamp();
    }

    return 0;
}

int Max44005SensorBase::setDelay(int32_t handle, int64_t ns) {
    mPollingDelay = ns < MIN_POLL_DELAY ? MIN_POLL_DELAY : ns;
    return 0;
}

bool Max44005SensorBase::hasPendingEvents() const {
    if (!mEnabled)
        return false;

    if ((getTimestamp() - mLastns) < mPollingDelay)
        return false;

    return true;
}

bool Max44005SensorBase::equals (int64_t val1, int64_t val2) {
    return val1 == val2;
}

int Max44005SensorBase::readEvents(sensors_event_t* data, int count) {
    unsigned int value = 0;

    if (count < 1 || data == NULL || !mEnabled) {
        ALOGV("Will not work on zero count(%i) or null pointer\n", count);
        return 0;
    }

    if (!hasPendingEvents())
        return 0;

    int amt = readIntFromFile(mSysPath, &value);
    if (amt <= 0) {
        ALOGV("SensorId=%d : read from %s failed", sid, mSysPath);
        return 0;
    }

    if (equals(value, mLast_value))
        return 0;

    mLast_value = value;
    toEvent(*data, value);
    mLastns = (*data).timestamp;
    return 1;
}

/*****************************************************************************/

Max44005Light::Max44005Light(const char *sysPath, int sensor_id)
    : Max44005SensorBase(sysPath, sensor_id)
{
    float resolution;
    int ret = readFloatFromFile(MAX44005_RESOLUTION_PATH, &resolution);
    if (ret == 1)
        mResolution = resolution;
    else
        mResolution = CLEAR_LUX_CONVERSION_FACTOR;
}

Max44005Light::~Max44005Light() {
}

void Max44005Light::toEvent(sensors_event_t &evt, int value) {
    evt.version = sizeof(sensors_event_t);
    evt.sensor = sid;
    evt.type = SENSOR_TYPE_LIGHT;
    evt.light = (float)value * mResolution;
    evt.timestamp = getTimestamp();
    ALOGV("LightSensor: time is %llu", evt.timestamp );
    ALOGV("LightSensor: value is %i", value );
    ALOGV("LightSensor: light_value is %f", evt.light );
}

SensorBase* Max44005Light::getInstance(void)
{
    int err = access(MAX44005_ALS_SYSFS_PATH, F_OK);
    if (err < 0)
        return NULL;
    return new Max44005Light(MAX44005_ALS_SYSFS_PATH, ID_L);
}

void Max44005Light::fillSensorDef(sensor_t *ssensor_list, int &curIndex)
{
    int err = access(MAX44005_ALS_SYSFS_PATH, F_OK);
    if (err < 0)
        return;

    float resolution;
    int ret = readFloatFromFile(MAX44005_RESOLUTION_PATH, &resolution);

    sensor_t *sensor_def = &ssensor_list[curIndex];
    sensor_def->name = "MAX44005 Light sensor";
    sensor_def->vendor = "MAXIM";
    sensor_def->version = 1;
    sensor_def->maxRange = 57452.0f;
    sensor_def->handle = ID_L;
    sensor_def->type = SENSOR_TYPE_LIGHT;
    sensor_def->power = 6.0f;
    sensor_def->minDelay = 100;
    if (ret == 1)
        sensor_def->resolution = resolution;
    else
        sensor_def->resolution = CLEAR_LUX_CONVERSION_FACTOR;
    curIndex++;
}

/*****************************************************************************/

Max44005Prox::Max44005Prox(const char *sysPath, int sensor_id, unsigned int ProxThreshold)
    : Max44005SensorBase(sysPath, sensor_id),
      mProximityThreshold(ProxThreshold)
{
}

Max44005Prox::~Max44005Prox() {
}

bool Max44005Prox::equals (int64_t val1, int64_t val2) {
    if ((val1 < 0) || (val2 < 0))
        return false;
    val1 = (unsigned int)val1 > mProximityThreshold ? 0 : 1;
    val2 = (unsigned int)val2 > mProximityThreshold ? 0 : 1;
    return val1 == val2;
}

void Max44005Prox::toEvent(sensors_event_t &evt, int value) {
    evt.version = sizeof(sensors_event_t);
    evt.sensor = sid;
    evt.type = SENSOR_TYPE_PROXIMITY;
    /* 0 = near; 1 = far. Based on how android expects */
    evt.distance = (unsigned int)value > mProximityThreshold ? 0 : 1;
    evt.timestamp = getTimestamp();
    ALOGV("ProximitySensor: value is %i", value );
    ALOGV("ProximitySensor: time is %llu", evt.timestamp );
    ALOGV("ProximitySensor: distance_value is %f", evt.distance );
}
