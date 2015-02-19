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

#ifndef ANDROID_LIGHTSENSOR_H
#define ANDROID_LIGHTSENSOR_H

#include "sensors.h"
#include "SensorBase.h"

/* maximum num of char in sysfs node name*/
#define MAX_PROP_SIZE 64
/* maximum num of char in sysfs node absolute path*/
#define MAX_SENSOR_PATH 128
/* name of Android property that contains path to
   SAR GPIO device (if applicable) */
#define PROXIMITY_GPIO_ANDROID_PROP "gsm.modem.SARdet.device"

/* Parent directory for all light devices */
#define PARENT_DIR "/sys/bus/iio/devices/"

#define NUM_OF_CHANNELS 2 /* ALS and Proximity */

#define is_sensor_id_valid(sensor_id) ((sensor_id == ID_L) || (sensor_id == ID_P))

#define IS_COMPULSORY(sysfs) (sysfsRequirement[sysfs] == COMPULSORY)
#define IS_OPTIONAL(sysfs) (sysfsRequirement[sysfs] == OPTIONAL)

#define NOT_APPLICABLE ""

#define IGNORE_PROX_THRESH UINT_MAX

/* sysfs enumerated in the following order in sysfsLookupTable */
enum sysfs_enum {
    NAME,
    VENDOR,
    ENABLE,
    INTEGRATION_TIME,
    MAX_RANGE,
    RAW,
    REGULATOR_ENABLE,
    RESOLUTION,
    POWER_CONSUMED,
    N_REQUIRED_SYSFS
};

/* entries in "" are dont care for that sensor id
 * => for proximity, LightSensor does not use sysfs _resolution */
static const char *sysfsLookupTable[NUM_OF_CHANNELS][N_REQUIRED_SYSFS] =
{
    {
        "name",
        "vendor",
        "in_illuminance_enable",
        "in_illuminance_integration_time",
        "in_illuminance_max_range",
        "in_illuminance_raw",
        "in_illuminance_regulator_enable",
        "in_illuminance_resolution",
        "in_illuminance_power_consumed"
    },
    {
        "name",
        "vendor",
        "in_proximity_enable",
        "in_proximity_integration_time",
        "in_proximity_max_range",
        "in_proximity_raw",
        "in_proximity_regulator_enable",
        NOT_APPLICABLE,
        "in_proximity_power_consumed",
    }
};

/* If sysfsLookTells if a sysfs should always be present */
enum sysfs_presence {
    COMPULSORY,
    OPTIONAL
};

/* To allow driver implementation based on only _raw sysfs,
 * the following table makes in_<channel>_enable and
 * in_<channel>_regulator_enable as optional */
static const bool sysfsRequirement[N_REQUIRED_SYSFS] =
{
    COMPULSORY, /* name */
    COMPULSORY, /* vendor */
    OPTIONAL,   /* _enable */
    COMPULSORY, /* _integration_time */
    COMPULSORY, /* _max_range */
    COMPULSORY, /* _raw */
    OPTIONAL,   /* _regulator_enable */
    COMPULSORY, /* _resolution */
    COMPULSORY  /*_power_consumed */
};

/* If a sysfs is present,
 * then what are the minimum access rights on these sysfs
 * that LightSensor needs */
static const int minAccessMode[N_REQUIRED_SYSFS] =
{
    R_OK,
    R_OK,
    W_OK,
    R_OK,
    R_OK,
    R_OK,
    W_OK,
    R_OK,
    R_OK,
};

class AmbientLightSensor;
class ProximitySensor;

/*****************************LIGHT SENSOR BASE********************************/

/* mIntegrationTime is minimum polling delay that device can allow
 * => two successive poll to device can not be less than mIntegrationTime */
class LightSensorBase: public SensorBase {
    bool mEnabled;
    int mLastValue;

    int64_t mLastns;
    int64_t mPollingDelay;
    int64_t mIntegrationTime;

    char *mSysRawPath;
    char *mSysEnablePath;
    char *mSysRegulatorEnablePath;

protected:
    char *name, *vendor;
    unsigned int minDelay;
    float maxRange, resolution, power;

public:
    LightSensorBase(const char *sysPath, int sid);
    virtual ~LightSensorBase();

    void fillSensorDef(sensor_t &ssensor_def, int sid, int type);
    int getSensorDetails(int sid);

    virtual int enable(int32_t handle, int enabled);
    virtual bool equals(int64_t, int64_t);
    virtual bool hasPendingEvents() const;
    virtual int readEvents(sensors_event_t *data, int count);
    virtual int setDelay(int32_t handle, int64_t ns);
    virtual void toEvent(sensors_event_t &evt, int value) = 0;

    static AmbientLightSensor* als;
    static ProximitySensor* proximity;

    static int num_valid_sysfs(char *path, int sid);
    static void fillSensorDef(sensor_t *ssensor_list, int &curIndex);
    static SensorBase* getInstance(int sensor_id, int proxThreshold = -1);
    static int getPath(char * filePath, const char *sysPath, int sid, int pIndex);
    static int getSensorPath(char* snsrPath, int sensorId);
};

/**************************AMBIENT LIGHT SENSOR********************************/

class AmbientLightSensor : public LightSensorBase {
    float raw2LuxConvFactor;
public:
             AmbientLightSensor(const char *sysPath);
    virtual ~AmbientLightSensor();

    void fillSensorDef(sensor_t &ssensor_t);
    int getSensorDetails();
    void toEvent(sensors_event_t &evt, int value);

    static void fillSensorDef(sensor_t *ssensor_list, int &curIndex);
};

/***************************PROXIMITY SENSOR***********************************/

class ProximitySensor : public LightSensorBase {
    unsigned int mProxThreshold;
    int mBbFd;
public:
             ProximitySensor(const char *sysPath, unsigned int proxThreshold);
    virtual ~ProximitySensor();

    void fillSensorDef(sensor_t &ssensor_t);
    int getSensorDetails();
    void setProxThreshold(unsigned int proxThreshold);
    void toEvent(sensors_event_t &evt, int value);
    int readEvents(sensors_event_t *data, int count);

    virtual bool equals(int64_t, int64_t);

    static void fillSensorDef(sensor_t *ssensor_list, int &curIndex);
};

#endif /* ANDROID_LIGHTSENSOR_H */
