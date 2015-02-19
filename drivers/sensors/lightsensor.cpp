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
#include <cutils/properties.h>
#include <stdlib.h>
#include <linux/input.h>
#include <hardware/sensors.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include "SensorUtil.h"
#include "lightsensor.h"

using namespace std;

/*****************************LIGHT SENSOR BASE********************************/

/* member functions */

LightSensorBase::LightSensorBase(const char *sysPath, int sid)
    : SensorBase(NULL, NULL),
      mEnabled(false),
      mLastValue(-1)
{
    name = NULL;
    vendor = NULL;
    minDelay = 0;
    maxRange = 0;
    resolution = 0;
    power = 0;

    char pathBuffer[MAX_SENSOR_PATH];
    int size = getPath(pathBuffer, sysPath, sid, RAW);
    mSysRawPath = new char[size];
    strcpy(mSysRawPath, pathBuffer);
    memset(pathBuffer, 0, size);

    size = getPath(pathBuffer, sysPath, sid, ENABLE);
    if (access(pathBuffer, F_OK) != -1) {
        mSysEnablePath = new char[size];
        strcpy(mSysEnablePath, pathBuffer);
    }
    memset(pathBuffer, 0, size);

    size = getPath(pathBuffer, sysPath, sid, REGULATOR_ENABLE);
    if (access(pathBuffer, F_OK) != -1) {
        mSysRegulatorEnablePath = new char[size];
        strcpy(mSysRegulatorEnablePath, pathBuffer);
    }
    memset(pathBuffer, 0, size);

    getPath(pathBuffer, sysPath, sid, INTEGRATION_TIME);
    unsigned int int_time = 0;
    int ret = readIntFromFile(pathBuffer, &int_time);
    ALOGE_IF(ret != 1, "cant get %s", pathBuffer);
    if (ret == 1) {
        mIntegrationTime = int_time; /* micro secs */
        mIntegrationTime = mIntegrationTime * 1000; /* nano sec */
    }
}

LightSensorBase::~LightSensorBase() {
    delete[] name;
    delete[] vendor;
    delete[] mSysRawPath;
    delete[] mSysEnablePath;
    delete[] mSysRegulatorEnablePath;
}

void LightSensorBase::fillSensorDef(sensor_t &sensor_def, int sid, int type) {
    sensor_def.name = name;
    sensor_def.vendor = vendor;
    sensor_def.version = 1;
    sensor_def.maxRange = maxRange;
    sensor_def.handle = sid;
    sensor_def.type = type;
    sensor_def.resolution = resolution;
    sensor_def.power = power;
    sensor_def.minDelay = minDelay;
}

/* returns 1 on success else 0 */
int LightSensorBase::getSensorDetails(int sid)
{
    char sysPath[MAX_SENSOR_PATH];
    int size = getSensorPath(sysPath, sid);

    if (!size)
        return 0;

/* once we are here => all required sysfs are present and you can access them.
 * The only reason of failure would be data type mismatch */

    char pathBuffer[MAX_SENSOR_PATH];
    int ret;
    size = 0;

    if (!name) {
        size = getPath(pathBuffer, sysPath, sid, NAME);
        char temp_name[MAX_PROP_SIZE];
        ret = readStringFromFile(pathBuffer, temp_name);
        ALOGE_IF(ret != 1, "did not get string from %s", pathBuffer);
        if (ret == 1) {
            name = new char[strlen(temp_name) + 1];
            strcpy(name, temp_name);
        }
    }
    memset(pathBuffer, 0, size);

    if (!vendor) {
        size = getPath(pathBuffer, sysPath, sid, VENDOR);
        char temp_vendor[MAX_PROP_SIZE];
        ret = readStringFromFile(pathBuffer, temp_vendor);
        ALOGE_IF(ret != 1, "did not get string from %s", pathBuffer);
        if (ret == 1) {
            vendor = new char[strlen(temp_vendor) + 1];
            strcpy(vendor, temp_vendor);
        }
    }
    memset(pathBuffer, 0, size);

    if (!maxRange) {
        size = getPath(pathBuffer, sysPath, sid, MAX_RANGE);
        ret = readFloatFromFile(pathBuffer, &maxRange);
        ALOGE_IF(ret != 1, "did not get float %s", pathBuffer);
        if (ret != 1) maxRange = 0;
    }
    memset(pathBuffer, 0, size);

    if (!resolution) {
        if (sid == ID_L) {
            size = getPath(pathBuffer, sysPath, sid, RESOLUTION);
            ret = readFloatFromFile(pathBuffer, &resolution);
            ALOGE_IF(ret != 1, "did not get float from %s", pathBuffer);
            if (ret != 1) resolution = 0;
	    resolution = resolution / 1000; /* lux */
        } else if (sid ==ID_P)
            resolution = 1;
    }
    memset(pathBuffer, 0, size);

    if (!power) {
        size = getPath(pathBuffer, sysPath, sid, POWER_CONSUMED);
        ret = readFloatFromFile(pathBuffer, &power);
        if (ret != 1) power = 0;
        ALOGE_IF(ret != 1, "did not get float from %s", pathBuffer);
	power = power / 1000; /* mA */
    }
    memset(pathBuffer, 0, size);

    if (!minDelay) {
        size = getPath(pathBuffer, sysPath, sid, INTEGRATION_TIME);
        ret = readIntFromFile(pathBuffer, &minDelay);
        ALOGE_IF(ret != 1, "did not get integer from %s", pathBuffer);
        if (ret != 1) minDelay = 0;
    }
    memset(pathBuffer, 0, size);

    if (name && vendor && maxRange && resolution && power && minDelay)
        return 1;
    return 0;
}

/* virtual functions */

/* To allow driver implementations based on only _raw sysfs
 * check if it is writable, then write
 * returns 0 on success else non zero */
int LightSensorBase::enable(int32_t handle, int en) {
    if ((en != 0) && (en != 1))
        return -EINVAL;

    if (en == mEnabled)
        return 0;

    int ret = 1;
    if (en) {
        if (mSysRegulatorEnablePath)
            ret &= writeIntToFile(mSysRegulatorEnablePath, en);

        if (mSysEnablePath)
            ret &= writeIntToFile(mSysEnablePath, en);

        mLastValue = -1;
        mLastns = getTimestamp();
    } else {
        if (mSysEnablePath)
            ret &= writeIntToFile(mSysEnablePath, en);

        if (mSysRegulatorEnablePath)
            ret &= writeIntToFile(mSysRegulatorEnablePath, en);
    }

    /* F_OK | R_OK access is already checked in getInstance */
    if (!mSysRegulatorEnablePath && !mSysEnablePath &&
        access(mSysRawPath, W_OK) != -1)
        ret &= writeIntToFile(mSysRawPath, en);

    if (ret == 1)
        mEnabled = en;

    return ret != 1;
}

bool LightSensorBase::equals(int64_t val1, int64_t val2) {
    /*
     * Auto-brightness algorithm needs replaying events from light sensor
     * even if they are same as it uses weighted average principle.
     * While sensors which have low resolution detect small changes in illuminance
     * and essentially provide ample inputs for the algorithm to work, sensors with
     * high resolutions provide a very bad quality auto-brightness user experience.
     * Ignoring comparision of events here can help such sensors vastly.
     * While little benefit would be derived for low resolution sensors, it is also
     * verified that this will not cause any power regression for such cases.
     */
    return false;
}

bool LightSensorBase::hasPendingEvents() const {
    if (!mEnabled)
        return false;

    if ((getTimestamp() - mLastns) < mPollingDelay)
        return false;

    return true;
}

int LightSensorBase::readEvents(sensors_event_t* data, int count) {
    unsigned int value = 0;
    int ret = 0;

    if (count < 1 || data == NULL || !mEnabled) {
        ALOGV("Will not work on zero count(%i) or null pointer\n", count);
        return 0;
    }

    if (!hasPendingEvents())
        return 0;

    ret = readIntFromFile(mSysRawPath, &value);
    if (ret <= 0) {
        ALOGV("read from %s failed", mSysRawPath);
        return 0;
    }

    mLastns = getTimestamp();
    if (equals(value, mLastValue))
        return 0;

    mLastValue = value;
    toEvent(*data, value);
    return 1;
}

int LightSensorBase::setDelay(int32_t handle, int64_t ns) {
    mPollingDelay = ns < mIntegrationTime ? mIntegrationTime : ns;
    return 0;
}

/* static variables  */

AmbientLightSensor *LightSensorBase::als = NULL;
ProximitySensor *LightSensorBase::proximity = NULL;

/* static functions */

/*
 * Returns number of valid sysfs for a sensor sid.
 * If this value equals N_REQUIRED_SYSFS,
 * then a sensor sid (i.e., eithe ID_L for ALS/ID_P for PS) exists in filePath.
 *
 * A valid sysfs meets below criteria:
 * 1. If sysfsRequirement for sysfs is COMPULSORY this sysfs should be present
 * else the sysfs may not be present but
 * 2. If a sysfs is present, then sensorservice needs to have atleast the access
 * rights specified by minAccessMode, irrespective of whether sysfs is OPTIONAL.
 */
int LightSensorBase::num_valid_sysfs(char *filePath, int sid) {
    int nvalid = 0;
    for (int sysfs = 0; sysfs < N_REQUIRED_SYSFS; sysfs++) {
        char tempPath[MAX_SENSOR_PATH];
        int size = getPath(tempPath, filePath, sid, sysfs);
        if (size) {
            bool fileNotPresent = (access(tempPath, F_OK) == -1);
            bool fileNotAccessable = (access(tempPath,
                                                minAccessMode[sysfs]) == -1);
            if (fileNotPresent) {
                if (IS_COMPULSORY(sysfs)) {
                    ALOGV("%s light sensor: sysfs %s not exposed",
                    (sid == ID_L ? "ambient" :
                    (sid == ID_P ? "proximity" : "invalid")), tempPath);
                    continue;
                }
             } else if (fileNotAccessable) { /* filePresent */
                 ALOGV("%s light sensor: sysfs %s permission is not proper",
                (sid == ID_L ? "ambient" :
                (sid == ID_P ? "proximity" : "invalid")), tempPath);
                continue;
             }
        }
        nvalid++;
    }
    return nvalid;
}

void LightSensorBase::fillSensorDef(sensor_t* ssensor_list, int &curIndex)
{
    AmbientLightSensor::fillSensorDef(ssensor_list, curIndex);
    ProximitySensor::fillSensorDef(ssensor_list, curIndex);
}

SensorBase* LightSensorBase::getInstance(int sensor_id, int proxThreshold)
{
    char sysPath[MAX_SENSOR_PATH];
    int size = getSensorPath(sysPath, sensor_id);
    if (!size)
        return NULL;

    switch (sensor_id) {
    case ID_L:
        if (als == NULL)
            als = new AmbientLightSensor(sysPath);
        return als;
    case ID_P:
        if (proximity == NULL) {
            proximity = new ProximitySensor(sysPath, proxThreshold);
        } else if (proxThreshold != -1) {
            proximity->setProxThreshold(proxThreshold);
        }
        return proximity;
    };

    return NULL;
}

int LightSensorBase::getPath(char* filePath, const char *sysPath, int sid,
                                int pIndex)
{
    int channel = sid == ID_L ? 0 : 1;
    if (strlen(sysfsLookupTable[channel][pIndex]) == 0)
        return 0;
    strcpy(filePath, sysPath);
    strcat(filePath, sysfsLookupTable[channel][pIndex]);
    return strlen(filePath) + 1;
}

/* Assumes that the device has only a single sensor of type sensorId */
int LightSensorBase::getSensorPath(char* snsrPath, int sensorId)
{
    DIR *dir;
    struct dirent *ent, *entFile;
    char pathBuffer[MAX_SENSOR_PATH];
    int curIndex = 0;

    dir = opendir(PARENT_DIR);
    if (dir == NULL)
        return 0;

    if (!is_sensor_id_valid(sensorId))
        return 0;

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type != DT_LNK)
            continue;

        strcpy(pathBuffer, PARENT_DIR);
        strcat(pathBuffer, ent->d_name);
        strcat(pathBuffer, "/");

        if (num_valid_sysfs(pathBuffer, sensorId) == N_REQUIRED_SYSFS) {
            closedir(dir);
            strcpy(snsrPath, pathBuffer);
            return strlen(pathBuffer) + 1;
        }
    }

    closedir(dir);
    return 0;
}

/**************************AMBIENT LIGHT SENSOR********************************/

/* member functions */

AmbientLightSensor::AmbientLightSensor(const char *sysPath)
    : LightSensorBase(sysPath, ID_L)
{
    char pathBuffer[MAX_SENSOR_PATH];
    getPath(pathBuffer, sysPath, ID_L, RESOLUTION);
    int ret = readFloatFromFile(pathBuffer, &raw2LuxConvFactor);
    /* convert milli lux to lux */
    if (ret == 1)
        raw2LuxConvFactor /= 1000;
    ALOGE_IF(ret != 1, "AmbientLightSensor: sysfs for resolution not found");
}

AmbientLightSensor::~AmbientLightSensor() {
}

void AmbientLightSensor::fillSensorDef(sensor_t& sensor_def) {
    LightSensorBase::fillSensorDef(sensor_def, ID_L, SENSOR_TYPE_LIGHT);
}

int AmbientLightSensor::getSensorDetails()
{
    return LightSensorBase::getSensorDetails(ID_L);
}

void AmbientLightSensor::toEvent(sensors_event_t &evt, int value) {
    evt.version = sizeof(sensors_event_t);
    evt.sensor = ID_L;
    evt.type = SENSOR_TYPE_LIGHT;
    evt.light = (float)value * raw2LuxConvFactor;
    evt.timestamp = getTimestamp();
    ALOGV("AmbientLightSensor: time is %llu", evt.timestamp );
    ALOGV("AmbientLightSensor: value is %i", value );
    ALOGV("AmbientLightSensor: light_value is %f", evt.light );
}

/* static functions */

void AmbientLightSensor::fillSensorDef(sensor_t *ssensor_list, int &curIndex)
{
    LightSensorBase::getInstance(ID_L);
    if (!als)
        return;

    int ret = als->getSensorDetails();
    if (ret != 1)
        return;

    als->fillSensorDef(ssensor_list[curIndex]);
    curIndex++;
}

/***************************PROXIMITY SENSOR***********************************/

/* member functions */
ProximitySensor::ProximitySensor(const char *sysPath,
                                 unsigned int proxThreshold)
    : LightSensorBase(sysPath, ID_P),
    mProxThreshold(proxThreshold)
{
    int ok;
    char bbGpioPath[PROPERTY_VALUE_MAX];

    /* retrieve GPIO device path from Android property */
    ok = property_get(PROXIMITY_GPIO_ANDROID_PROP,
                      bbGpioPath,
                      "");

    ALOGD("%s: found property %d %s\n",
          __FUNCTION__,
          ok,
          bbGpioPath);

    /* open GPIO device file */
    mBbFd = open(bbGpioPath, O_WRONLY);
    if(mBbFd < 0)
    {
        ALOGE("%s: failed to open %s. %s",
               __FUNCTION__,
               bbGpioPath,
               strerror(errno));
    }
}

int ProximitySensor::readEvents(sensors_event_t* data, int count) {
    int nbEvents;
    int i;
    const char *val;

    /* call inherited function */
    nbEvents = LightSensorBase::readEvents(data, count);

    /* do we have a path to the GPIO device? */
    if (mBbFd >= 0)
    {
        /* go through the list of events and notify baseband */
        for (i=0; i<nbEvents; i++)
        {
            /* convert distance to binary value - a non-zero
               distance maps to "no presence". A zero value means
               a proximity event was detected. The baseband now
               expects an active low signal:
                0=>presence, 1=>no presence
             */

            val = data[i].distance > 0 ? "1" : "0";

            if(write(mBbFd, val, strlen(val)) != (int)strlen(val))
            {
                ALOGE("%s: failed to write to file. %s\n",
                      __FUNCTION__,
                      strerror(errno));
            }
        }
    }
exit:
    return nbEvents;
}

ProximitySensor::~ProximitySensor() {
    /* close file (if applicable) */
    if (mBbFd >= 0)
        close(mBbFd);
}

void ProximitySensor::fillSensorDef(sensor_t &sensor_def) {
    LightSensorBase::fillSensorDef(sensor_def, ID_P, SENSOR_TYPE_PROXIMITY);
}

int ProximitySensor::getSensorDetails()
{
    return LightSensorBase::getSensorDetails(ID_P);
}

void ProximitySensor::setProxThreshold(unsigned int proxThreshold) {
    mProxThreshold = proxThreshold;
}

void ProximitySensor::toEvent(sensors_event_t &evt, int value) {
    evt.version = sizeof(sensors_event_t);
    evt.sensor = ID_P;
    evt.type = SENSOR_TYPE_PROXIMITY;
    if (mProxThreshold == IGNORE_PROX_THRESH) {
        evt.distance = value;
    } else {
        /* 0 = near; 1 = far. Based on how android expects */
        evt.distance = (unsigned int)value > mProxThreshold ? 0 : 1;
    }
    evt.timestamp = getTimestamp();
    ALOGV("ProximitySensor: value is %i", value );
    ALOGV("ProximitySensor: time is %llu", evt.timestamp );
    ALOGV("ProximitySensor: distance_value is %f", evt.distance );
}

/* virtual functions */

bool ProximitySensor::equals (int64_t val1, int64_t val2) {
    if ((val1 < 0) || (val2 < 0))
        return false;
    if (mProxThreshold == IGNORE_PROX_THRESH)
        return val1 == val2;
    val1 = (unsigned int)val1 > mProxThreshold ? 0 : 1;
    val2 = (unsigned int)val2 > mProxThreshold ? 0 : 1;
    return val1 == val2;
}

/* static functions */

void ProximitySensor::fillSensorDef(sensor_t *ssensor_list, int &curIndex)
{
    getInstance(ID_P);
    if (!proximity)
        return;
    int ret = proximity->getSensorDetails();
    if (ret != 1)
        return;

    proximity->fillSensorDef(ssensor_list[curIndex]);
    curIndex++;
}
