/* Copyright (C) 2012 The Android Open Source Project
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

#include "Nvs.h"
#include "NvsAccel.h"
#include "NvsAnglvel.h"
#include "NvsIlluminance.h"
#include "NvsMagn.h"
#include "NvsPressure.h"
#include "NvsProximity.h"
#include "NvsTemp.h"
#include "MPLSensor.h"

/* possible SENSOR_COUNT_MAX =
 * SENSOR_TYPE_ACCELEROMETER
 * SENSOR_TYPE_GEOMAGNETIC_FIELD
 * SENSOR_TYPE_ORIENTATION
 * SENSOR_TYPE_GYROSCOPE
 * SENSOR_TYPE_LIGHT
 * SENSOR_TYPE_PRESSURE
 * SENSOR_TYPE_PROXIMITY
 * SENSOR_TYPE_GRAVITY
 * SENSOR_TYPE_LINEAR_ACCELERATION
 * SENSOR_TYPE_ROTATION_VECTOR
 * SENSOR_TYPE_AMBIENT_TEMPERATURE (FROM BMPX80)
 * SENSOR_TYPE_AMBIENT_TEMPERATURE (FROM MPU FOR GYRO)
 * SENSOR_TYPE_GAME_ROTATION_VECTOR
 * SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED
 * SENSOR_TYPE_GYROSCOPE_UNCALIBRATED
 * SENSOR_TYPE_SIGNIFICANT_MOTION
 * SENSOR_TYPE_STEP_DETECTOR
 * SENSOR_TYPE_STEP_COUNTER
 * SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR
 */
#define SENSOR_COUNT_MAX                19
#define SENSOR_PLATFORM_NAME            "Ardbeg sensors module"
#define SENSOR_PLATFORM_AUTHOR          "NVIDIA CORP"
#define SENSORS_DEVICE_API_VERSION      0//SENSORS_DEVICE_API_VERSION_1_1

static struct sensor_t nvspSensorList[SENSOR_COUNT_MAX] = {
};

static NvsIlluminance *mNvsIlluminance = NULL;
static NvsTemp *mNvsLightTemp = NULL;
static NvsAccel *mNvsAccel = NULL;
static NvsAnglvel *mNvsAnglvel = NULL;
static NvsTemp *mNvsGyroTemp = NULL;
static NvsPressure *mNvsPressure = NULL;
static NvsMagn *mNvsMagn = NULL;

static SensorBase *newIlluminance(int devNum)
{
    mNvsIlluminance = new NvsIlluminance(devNum);
    return mNvsIlluminance;
}

static SensorBase *newProximity(int devNum)
{
    return new NvsProximity(devNum, mNvsIlluminance);
}

static SensorBase *newAccel(int devNum)
{
    mNvsAccel = new NvsAccel(devNum);
    return mNvsAccel;
}

static SensorBase *newAnglvel(int devNum)
{
    mNvsAnglvel = new NvsAnglvel(devNum, mNvsAccel);
    return mNvsAnglvel;
}

static SensorBase *newGyroTemp(int devNum)
{
    mNvsGyroTemp = new NvsTemp(devNum, mNvsAnglvel);
    return mNvsGyroTemp;
}

static SensorBase *newPressure(int devNum)
{
    mNvsPressure = new NvsPressure(devNum);
    return mNvsPressure;
}

static SensorBase *newPresTemp(int devNum)
{
    return new NvsTemp(devNum, mNvsPressure);
}

static SensorBase *newMagn(int devNum)
{
    mNvsMagn = new NvsMagn(devNum);
    return mNvsMagn;
}

static SensorBase *newMpl(int devNum)
{
    if (mNvsAccel) {
        if (!mNvsAccel->getSensorList(NULL, 0, 0))
            mNvsAccel = NULL;
    }
    if (mNvsAnglvel) {
        if (!mNvsAnglvel->getSensorList(NULL, 0, 0))
            mNvsAnglvel = NULL;
    }
    if (mNvsGyroTemp) {
        if (!mNvsGyroTemp->getSensorList(NULL, 0, 0))
            mNvsGyroTemp = NULL;
    }
    if (mNvsMagn) {
        if (!mNvsMagn->getSensorList(NULL, 0, 0))
        mNvsMagn = NULL;
    }
    return new MPLSensor(mNvsAccel, mNvsAnglvel, mNvsGyroTemp, mNvsMagn);
}

static struct NvspDriver nvspDriverList[] = {
    {
        .devName                = "cm3217",
        .iio                    = true,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = &newIlluminance,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "cm3218x",
        .iio                    = true,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = &newIlluminance,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "max4400x",
        .iio                    = true,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = &newIlluminance,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "max4400x",
        .iio                    = true,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = &newProximity,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "bmpX80",
        .iio                    = true,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = &newPressure,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "bmpX80",
        .iio                    = true,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = false,
        .newDriver              = &newPresTemp,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "ak89xx",
        .iio                    = true,
        .noList                 = false,
        .fusionClient           = true,
        .fusionDriver           = false,
        .newDriver              = &newMagn,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "mpu6xxx",
        .iio                    = true,
        .noList                 = false,
        .fusionClient           = true,
        .fusionDriver           = false,
        .newDriver              = &newAccel,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "mpu6xxx",
        .iio                    = true,
        .noList                 = false,
        .fusionClient           = true,
        .fusionDriver           = false,
        .newDriver              = &newAnglvel,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    {
        .devName                = "mpu6xxx",
        .iio                    = true,
        .noList                 = true,
        .fusionClient           = true,
        .fusionDriver           = false,
        .newDriver              = &newGyroTemp,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
    /* the fusion driver must be last to link to fusion devices */
    {
        .devName                = NULL, /* fusion driver loads regardless */
        .iio                    = true,
        .noList                 = false,
        .fusionClient           = false,
        .fusionDriver           = true,
        .newDriver              = &newMpl,
        .staticHandle           = 0,
        .numStaticHandle        = 0,
    },
};

#include "Nvsp.h"

