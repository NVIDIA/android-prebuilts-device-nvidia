/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
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

#include "ltr558als.h"
#include "SensorUtil.h"

/*****************************************************************************/

ltr558Light::ltr558Light(const char *sysPath, const char *sysEnablePath, int sensor_id)
    : SensorBase(NULL, NULL),
      mEnabled(false),
      mHasPendingEvent(false),
      mLast_value(-1),
      mAlready_warned(false),
      mSysPath(sysPath),
      mSysEnablePath(sysEnablePath),
      sid(sensor_id)
{
}

ltr558Light::~ltr558Light() {
}

int ltr558Light::enable(int32_t handle, int en) {
    int err = writeIntToFile(mSysEnablePath, en);
    if (err <= 0)
        return err;

    if (en != 0) {
        mEnabled = true;
        mLast_value = -1;
        mPollingDelay = MAX_POLLING_DELAY;
        mLastns.timestamp = 0;
    }
    else
        mEnabled = false;
    return 0;
}

int ltr558Light::setDelay(int32_t handle, int64_t ns) {
    mPollingDelay = ns;
    return 0;
}

bool ltr558Light::hasPendingEvents() const {
    if(mEnabled)
        return true;
    else
        return false;
}

int ltr558Light::readEvents(sensors_event_t* data, int count) {
    unsigned int value = 0;
    sensors_event_t evt;
    sensors_event_t Currentns;
    int64_t Delay;

    if (count < 1 || data == NULL || !mEnabled) {
        ALOGE("Will not work on zero count(%i) or null pointer\n", count);
        return 0;
    }

    Currentns.timestamp = getTimestamp();
    Delay = Currentns.timestamp - mLastns.timestamp;
    if (Delay < mPollingDelay)
        return 0;
    else
        mLastns.timestamp = Currentns.timestamp;

    input_event const* event;

    int amt = readIntFromFile(mSysPath, &value);
    if (amt <= 0 && mAlready_warned == false) {
        ALOGE("LightSensor: read from %s failed", mSysPath);
        mAlready_warned = false;
        return 0;
    }

    if (((float)value) == mLast_value)
        return 0;

    evt.version = sizeof(sensors_event_t);
    evt.sensor = sid;
    evt.type = SENSOR_TYPE_LIGHT;
    evt.light = (float)value;
    evt.timestamp = Currentns.timestamp;
    *data = evt;
    mLast_value = evt.light;
    ALOGV("LightSensor: value is %i", (int)value );
    return 1;
}

ltr558Prox::ltr558Prox(const char *sysPath, const char *sysEnablePath,
                       const char *sysThresPath, int sensor_id)
    : SensorBase(NULL, NULL),
      mEnabled(0),
      mHasPendingEvent(false),
      mAlready_warned(false),
      mLast_value(-1),
      sid(sensor_id)
{
    minValue = UINT_MAX;

    if (sysPath) {
        mSysPath = new char[strlen(sysPath) + 1];
        strcpy(mSysPath, sysPath);
    }

    if (sysEnablePath) {
        mSysEnablePath = new char[strlen(sysEnablePath) + 1];
        strcpy(mSysEnablePath, sysEnablePath);
    }

    mProximityThreshold = 1;
    if (sysThresPath) {
        mSysThresPath = new char[strlen(sysThresPath) + 1];
        strcpy(mSysThresPath, sysThresPath);
        readIntFromFile(mSysThresPath, &mProximityThreshold);
    }
}

ltr558Prox::~ltr558Prox() {
    delete[] mSysPath;
    delete[] mSysEnablePath;
    delete[] mSysThresPath;
}

int ltr558Prox::fillPaths(char *path, char *enable_path, char  *thres_path)
{
    char pathbuffer[MAX_SENSOR_PATH_LEN] = { 0 };
    char name[MAX_SENSOR_NAME_LEN] = {0};
    strcpy(pathbuffer, LTR_SENSOR_SYSFS_PATH_NAME);
    char *ptrToDevNum = strstr(pathbuffer, "0");
    int i = 0, err;

    do {
        *ptrToDevNum = '0' + i;
        err = readStringFromFile(pathbuffer, name);
        if (err != 1)
            continue;
        if (!strncmp(name, LTR_DEVICE_NAME, strlen(LTR_DEVICE_NAME)))
            break;
    } while (++i < MAX_IIO_DEV);

    if (i != MAX_IIO_DEV) {
        pathbuffer[strlen(pathbuffer) - 4] = '\0';
        if (path) {
            strcpy(path, pathbuffer);
            strcat(path, "proximity_value");
        }
        if (enable_path) {
            strcpy(enable_path, pathbuffer);
            strcat(enable_path, "proximity_enable");
        }
        if (thres_path) {
            strcpy(thres_path, pathbuffer);
            strcat(thres_path, "proximity_detect_threshold");
        }
        return 1;
    }
    return 0;
}

int ltr558Prox::enable(int32_t handle, int en) {
    int err = writeIntToFile(mSysEnablePath, en);
    if (err <= 0)
        return err;

    if (en != 0) {
        mLast_value = -1;
        mEnabled = true;
        mPollingDelay = MAX_POLLING_DELAY;
        mLastns.timestamp = 0;
    }
    else
       mEnabled = false;
    return 0;
}

int ltr558Prox::setDelay(int32_t handle, int64_t ns) {
    mPollingDelay = ns;
    return 0;
}

bool ltr558Prox::hasPendingEvents() const {
    if(mEnabled)
        return true;
    else
        return false;
}

int ltr558Prox::readEvents(sensors_event_t* data, int count) {
    unsigned int value = 0;
    float event_value;
    sensors_event_t Currentns;
    int64_t Delay;

    if (count < 1 || data == NULL || !mEnabled)
        return 0;

    Currentns.timestamp = getTimestamp();
    Delay = Currentns.timestamp - mLastns.timestamp;
    if (Delay < mPollingDelay)
        return 0;
    else
        mLastns.timestamp = Currentns.timestamp;

    int amt = readIntFromFile(mSysPath, &value);
    if (amt <= 0 && mAlready_warned == false) {
        ALOGE("ProximitySensor: read from %s failed", mSysPath);
        mAlready_warned = true;
        return 0;
    }

    if (value < minValue) {
        minValue = value;
        mProximityThreshold = .0364 * minValue + 63.468;
    }

    value = value - minValue;

    /*
     * android expects distance to be float.
     * It also expects sensors with maxRange=1.0 report only near/far event
     * 0.0f is treated as near, 1.0f is treated as far
     */
    event_value =  value > mProximityThreshold ? 0.0f : 1.0f;

    if (event_value == mLast_value)
        return 0;

    (*data).version = sizeof(sensors_event_t);
    (*data).sensor = sid;
    (*data).type = SENSOR_TYPE_PROXIMITY;
    (*data).distance =  event_value;
    (*data).timestamp = Currentns.timestamp;
    mLast_value = event_value;
    ALOGV("ProximitySensor: value is %i", (int)value );
    return 1;
}

void ltr558Prox::fillSensorDef(sensor_t* ssensor_list, int &curIndex)
{
    char prox_path[MAX_SENSOR_PATH_LEN] = {0};
    char prox_enable_path[MAX_SENSOR_PATH_LEN] = {0};
    char prox_thres_path[MAX_SENSOR_PATH_LEN] = {0};
    if (fillPaths(prox_path, prox_enable_path, prox_thres_path))
    {
        sensor_t *sensor_def = &ssensor_list[curIndex];
        sensor_def->name = "Lite On Technology Corp LTR-558 Proximity sensor";
        sensor_def->vendor = "Lite-On Technology";
        sensor_def->version = 1;
        sensor_def->handle = ID_P;
        sensor_def->type = SENSOR_TYPE_PROXIMITY;
        sensor_def->maxRange = 1.0f;
        sensor_def->resolution = 1.0f;
        sensor_def->power = 0.40f;
        curIndex++;
    }
}
