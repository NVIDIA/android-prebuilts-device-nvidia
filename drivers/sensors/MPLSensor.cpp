/*
* Copyright (C) 2012 Invensense, Inc.
* Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#define LOG_NDEBUG 0

//see also the EXTRA_VERBOSE define in the MPLSensor.h header file

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include <pthread.h>
#include <cutils/log.h>
#include <utils/KeyedVector.h>
#include <utils/String8.h>
#include <string.h>
#include <linux/input.h>
#include <utils/Atomic.h>

#include "Nvsb.h"

#include "MPLSensor.h"
#include "MPLSupport.h"

#include "invensense.h"
#include "invensense_adv.h"
#include "ml_stored_data.h"
#include "ml_load_dmp.h"
#include "ml_sysfs_helper.h"

#define FIFO_RESERVED_EVENT_COUNT       (64)
#define FIFO_MAX_EVENT_COUNT            (1024)


#define ENABLE_MULTI_RATE
// #define TESTING
#define USE_LPQ_AT_FASTEST

#ifdef THIRD_PARTY_ACCEL
#pragma message("HAL:build third party accel support")
#define USE_THIRD_PARTY_ACCEL (1)
#else
#define USE_THIRD_PARTY_ACCEL (0)
#endif

#define MAX_SYSFS_ATTRB (sizeof(struct sysfs_attrbs) / sizeof(char*))

/******************************************************************************/
/*  MPL Interface                                                             */
/******************************************************************************/

//#define INV_PLAYBACK_DBG
#ifdef INV_PLAYBACK_DBG
static FILE *logfile = NULL;
#endif

static signed char matrixDisable[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

/*******************************************************************************
 * MPLSensor class implementation
 ******************************************************************************/

static struct timespec mt_pre;

// following extended initializer list would only be available with -std=c++11
//  or -std=gnu+11
MPLSensor::MPLSensor(NvsAccel *accel,
                     NvsAnglvel *anglvel,
                     NvsTemp *gyroTemp,
                     NvsMagn *magn,
                     int (*m_pt2AccelCalLoadFunc)(long *))
                       : SensorBase(NULL, NULL),
                         mAccelSensor(accel),
                         mGyroSensor(anglvel),
                         mGyroSensorList(NULL),
                         mGyroTempSensor(gyroTemp),
                         mCompassSensor(magn),
                         mCompassSensorList(NULL),
                         mPressureSensor(NULL),
                         mPressureSensorList(NULL),
                         mMasterSensorMask(INV_ALL_SENSORS),
                         mLocalSensorMask(0),
                         mPollTime(-1),
                         mStepCountPollTime(-1),
                         mHaveGoodMpuCal(0),
                         mGyroAccuracy(0),
                         mAccelAccuracy(0),
                         mCompassAccuracy(0),
                         dmp_sign_motion_fd(-1),
                         mDmpSignificantMotionEnabled(0),
                         dmp_pedometer_fd(-1),
                         mDmpPedometerEnabled(0),
                         mDmpStepCountEnabled(0),
                         mEnabled(0),
                         mBatchEnabled(0),
                         mOldBatchEnabledMask(0),
                         mHasPendingEvent(false),
                         mTempScale(0),
                         mTempOffset(0),
                         mTempCurrentTime(0),
                         mAccelScale(2),
                         mAccelSelfTestScale(2),
                         mGyroScale(2000),
                         mGyroSelfTestScale(250),
                         mCompassScale(0),
                         mFactoryGyroBiasAvailable(false),
                         mGyroBiasAvailable(false),
                         mGyroBiasApplied(false),
                         mFactoryAccelBiasAvailable(false),
                         mAccelBiasAvailable(false),
                         mAccelBiasApplied(false),
                         mPendingMask(0),
                         mSensorMask(0),
                         mMplFeatureActiveMask(0),
                         mFeatureActiveMask(0),
                         mDmpOn(0),
                         mDmpLoaded(0),
                         mPedUpdate(0),
                         mPressureUpdate(0),
                         mQuatSensorTimestamp(0),
                         mStepSensorTimestamp(0),
                         mLastStepCount(0),
                         mLeftOverBufferSize(0),
                         mInitial6QuatValueAvailable(0),
                         mSkipReadEvents(0),
                         mDataMarkerDetected(0),
                         mEmptyDataMarkerDetected(0)
{
    VFUNC_LOG;

    inv_error_t rv;
    int i, fd;
    char path[MAX_SYSFS_NAME_LEN];
    char *ver_str;
    unsigned long mSensorMask;
    int res;
    FILE *fptr;

    ALOGV_IF(EXTRA_VERBOSE,
             "HAL:MPLSensor constructor : MpuNumSensors = %d", MpuNumSensors);

    ALOGI_IF(ENG_VERBOSE, "%s\nmAccelSensor=%p\nmGyroSensor=%p\nmGyroTempSensor=%p\n"
             "mCompassSensor=%p\nmPressureSensor=%p\n",
             __func__, mAccelSensor, mGyroSensor, mGyroTempSensor,
             mCompassSensor, mPressureSensor);

    pthread_mutex_init(&mMplMutex, NULL);
    pthread_mutex_init(&mHALMutex, NULL);
    memset(mGyroOrientation, 0, sizeof(mGyroOrientation));
    memset(mAccelOrientation, 0, sizeof(mAccelOrientation));
    memset(mInitial6QuatValue, 0, sizeof(mInitial6QuatValue));
    /* initalize SENSOR_TYPE_META_DATA */
    memset(&mMetaDataEvent, 0, sizeof(mMetaDataEvent));
    mMetaDataEvent.version = META_DATA_VERSION;
    mMetaDataEvent.type = SENSOR_TYPE_META_DATA;

    /* setup sysfs paths */
    inv_init_sysfs_attributes();
    /* get chip name */
    sprintf(path, "%s/part", mSysfsPath);
    res = sysfsStrRead(path, chip_ID);
    if (res > 0)
        ALOGI("%s %s Chip ID = %s\n", __func__, path, chip_ID);
    else
        ALOGE("%s ERR: %d: Failed to get chip ID from %s\n",
              __func__, res, path);

    /* Load DMP image if capable, ie. MPU6515 */
    loadDMP();

    if (mGyroSensor) {
        /* read gyro FSR to calculate accel scale later */
        int range = mGyroSensor->getPeakRaw(-1);
        if (range > 30)
            mGyroScale = 2000;
        else if (range > 15)
            mGyroScale = 1000;
        else if (range > 7)
            mGyroScale = 500;
        else if (range > 3)
            mGyroScale = 250;
        ALOGV_IF(EXTRA_VERBOSE, "%s: Gyro FSR used %d", __func__, mGyroScale);
    }

    /* mFactoryGyBias contains bias values that will be used for device offset */
    memset(mFactoryGyroBias, 0, sizeof(mFactoryGyroBias));

    /* open Gyro Bias fd */
    /* mGyroBias contains bias values that will be used for framework */
    /* mGyroChipBias contains bias values that will be used for dmp */
    memset(mGyroBias, 0, sizeof(mGyroBias));
    memset(mGyroChipBias, 0, sizeof(mGyroChipBias));
#ifdef NVI_DMP
    ALOGV_IF(EXTRA_VERBOSE, "HAL: gyro x dmp bias path: %s", mpu.in_gyro_x_dmp_bias);
    ALOGV_IF(EXTRA_VERBOSE, "HAL: gyro y dmp bias path: %s", mpu.in_gyro_y_dmp_bias);
    ALOGV_IF(EXTRA_VERBOSE, "HAL: gyro z dmp bias path: %s", mpu.in_gyro_z_dmp_bias);
    gyro_x_dmp_bias_fd = open(mpu.in_gyro_x_dmp_bias, O_RDWR);
    gyro_y_dmp_bias_fd = open(mpu.in_gyro_y_dmp_bias, O_RDWR);
    gyro_z_dmp_bias_fd = open(mpu.in_gyro_z_dmp_bias, O_RDWR);
    if (gyro_x_dmp_bias_fd == -1 ||
             gyro_y_dmp_bias_fd == -1 || gyro_z_dmp_bias_fd == -1) {
        ALOGE("HAL:could not open gyro DMP calibrated bias");
    } else {
        ALOGV_IF(EXTRA_VERBOSE,
                 "HAL:gyro_dmp_bias opened");
    }
#endif // NVI_DMP

    if (mAccelSensor) {
        /* read accel FSR to calculate accel scale later */
        mAccelSensorList = mAccelSensor->getSensorListPtr();
        if (mAccelSensorList) {
            if (mAccelSensorList->version > 2) {
                int range = mAccelSensor->getPeakRaw(-1);
                if (range > 120)
                    mAccelScale = 16;
                else if (range > 60)
                    mAccelScale = 8;
                else if (range > 30)
                    mAccelScale = 4;
                else if (range > 15)
                    mAccelScale = 2;
            }
        }
        ALOGV_IF(EXTRA_VERBOSE, "%s: Accel FSR used %d", __func__, mAccelScale);
    }
    /* read accel self test scale used to calculate factory cal bias later */
    if (!strcmp(chip_ID, "mpu6050"))
        /* initialized to 2 as MPU65xx default*/
        mAccelSelfTestScale = 8;

    /* mFactoryAccelBias contains bias values that will be used for device offset */
    memset(mFactoryAccelBias, 0, sizeof(mFactoryAccelBias));
    /* open Accel Bias fd */
    /* mAccelBias contains bias that will be used for dmp */
    memset(mAccelBias, 0, sizeof(mAccelBias));
#ifdef NVI_DMP
    ALOGV_IF(EXTRA_VERBOSE, "HAL:accel x dmp bias path: %s", mpu.in_accel_x_dmp_bias);
    ALOGV_IF(EXTRA_VERBOSE, "HAL:accel y dmp bias path: %s", mpu.in_accel_y_dmp_bias);
    ALOGV_IF(EXTRA_VERBOSE, "HAL:accel z dmp bias path: %s", mpu.in_accel_z_dmp_bias);
    accel_x_dmp_bias_fd = open(mpu.in_accel_x_dmp_bias, O_RDWR);
    accel_y_dmp_bias_fd = open(mpu.in_accel_y_dmp_bias, O_RDWR);
    accel_z_dmp_bias_fd = open(mpu.in_accel_z_dmp_bias, O_RDWR);
    if (accel_x_dmp_bias_fd == -1 ||
             accel_y_dmp_bias_fd == -1 || accel_z_dmp_bias_fd == -1) {
        ALOGE("HAL:could not open accel DMP calibrated bias");
    } else {
        ALOGV_IF(EXTRA_VERBOSE,
                 "HAL:accel_dmp_bias opened");
    }

    dmp_sign_motion_fd = open(mpu.event_smd, O_RDONLY | O_NONBLOCK);
    if (dmp_sign_motion_fd < 0) {
        ALOGE("HAL:ERR couldn't open dmp_sign_motion node");
    } else {
        ALOGV_IF(ENG_VERBOSE,
                 "HAL:dmp_sign_motion_fd opened : %d", dmp_sign_motion_fd);
    }
    /* the following threshold can be modified for SMD sensitivity */
    int motionThreshold = 3000;
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
             motionThreshold, mpu.smd_threshold, getTimestamp());
    res = write_sysfs_int(mpu.smd_threshold, motionThreshold);
    dmp_pedometer_fd = open(mpu.event_pedometer, O_RDONLY | O_NONBLOCK);
    if (dmp_pedometer_fd < 0) {
        ALOGE("HAL:ERR couldn't open dmp_pedometer node");
    } else {
        ALOGV_IF(ENG_VERBOSE,
                 "HAL:dmp_pedometer_fd opened : %d", dmp_pedometer_fd);
    }
#endif // NVI_DMP

    initBias();

    (void)inv_get_version(&ver_str);
    ALOGI("%s\n", ver_str);

    /* setup MPL */
    inv_constructor_init();

#ifdef INV_PLAYBACK_DBG
    ALOGV_IF(PROCESS_VERBOSE, "HAL:inv_turn_on_data_logging");
    logfile = fopen("/data/playback.bin", "w+");
    if (logfile)
        inv_turn_on_data_logging(logfile);
#endif

    /* setup orientation matrix and scale */
    inv_set_device_properties();

    /* initialize sensor data */
    memset(mPendingEvents, 0, sizeof(mPendingEvents));
    for (i = 0; i < MpuNumSensors; i++)
        mPendingEvents[i].version = sizeof(sensors_event_t);

    mPendingEvents[RotationVector].type = SENSOR_TYPE_ROTATION_VECTOR;
    mPendingEvents[RotationVector].acceleration.status
            = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[GameRotationVector].type = SENSOR_TYPE_GAME_ROTATION_VECTOR;
    mPendingEvents[GameRotationVector].acceleration.status
            = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[LinearAccel].type = SENSOR_TYPE_LINEAR_ACCELERATION;
    mPendingEvents[LinearAccel].acceleration.status
            = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[Gravity].type = SENSOR_TYPE_GRAVITY;
    mPendingEvents[Gravity].acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[Gyro].type = SENSOR_TYPE_GYROSCOPE;
    mPendingEvents[Gyro].gyro.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[RawGyro].type = SENSOR_TYPE_GYROSCOPE_UNCALIBRATED;
    mPendingEvents[RawGyro].gyro.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[Accelerometer].type = SENSOR_TYPE_ACCELEROMETER;
    mPendingEvents[Accelerometer].acceleration.status
            = SENSOR_STATUS_ACCURACY_HIGH;

    /* Invensense compass calibration */
    mPendingEvents[MagneticField].type = SENSOR_TYPE_MAGNETIC_FIELD;
    mPendingEvents[MagneticField].magnetic.status =
        SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[RawMagneticField].type = SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED;
    mPendingEvents[RawMagneticField].magnetic.status =
        SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[Pressure].type = SENSOR_TYPE_PRESSURE;
    mPendingEvents[Pressure].magnetic.status =
        SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[Orientation].type = SENSOR_TYPE_ORIENTATION;
    mPendingEvents[Orientation].orientation.status
            = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[GeomagneticRotationVector].type
            = SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR;
    mPendingEvents[GeomagneticRotationVector].acceleration.status
            = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[SignificantMotion].type = SENSOR_TYPE_SIGNIFICANT_MOTION;
    mPendingEvents[SignificantMotion].acceleration.status = SENSOR_STATUS_UNRELIABLE;

    mPendingEvents[StepDetector].type = SENSOR_TYPE_STEP_DETECTOR;
    mPendingEvents[StepDetector].acceleration.status = SENSOR_STATUS_UNRELIABLE;

    mPendingEvents[StepCounter].type = SENSOR_TYPE_STEP_COUNTER;
    mPendingEvents[StepCounter].acceleration.status = SENSOR_STATUS_UNRELIABLE;

    /* Event Handlers for HW and Virtual Sensors */
    mHandlers[RotationVector] = &MPLSensor::rvHandler;
    mHandlers[GameRotationVector] = &MPLSensor::grvHandler;
    mHandlers[LinearAccel] = &MPLSensor::laHandler;
    mHandlers[Gravity] = &MPLSensor::gravHandler;
    mHandlers[Gyro] = &MPLSensor::gyroHandler;
    mHandlers[RawGyro] = &MPLSensor::rawGyroHandler;
    mHandlers[Accelerometer] = &MPLSensor::accelHandler;
    mHandlers[MagneticField] = &MPLSensor::compassHandler;
    mHandlers[RawMagneticField] = &MPLSensor::rawCompassHandler;
    mHandlers[Orientation] = &MPLSensor::orienHandler;
    mHandlers[GeomagneticRotationVector] = &MPLSensor::gmHandler;
    mHandlers[Pressure] = &MPLSensor::psHandler;

    /* initialize delays to reasonable values */
    for (int i = 0; i < MpuNumSensors; i++) {
        mDelays[i] = 1000000000LL;
        mBatchDelays[i] = 1000000000LL;
        mBatchTimeouts[i] = 100000000000LL;
    }

    /* initialize Compass Bias */
    memset(mCompassBias, 0, sizeof(mCompassBias));

    /* initialize Factory Accel Bias */
    memset(mFactoryAccelBias, 0, sizeof(mFactoryAccelBias));

    /* initialize Gyro Bias */
    memset(mGyroBias, 0, sizeof(mGyroBias));
    memset(mGyroChipBias, 0, sizeof(mGyroChipBias));

    /* load calibration file from /data/inv_cal_data.bin */
    rv = inv_load_calibration();
    if(rv == INV_SUCCESS) {
        ALOGV_IF(PROCESS_VERBOSE, "HAL:Calibration file successfully loaded");
        /* Get initial values */
        getCompassBias();
        getGyroBias();
        if (mGyroBiasAvailable) {
            setGyroBias();
        }
        getAccelBias();
        getFactoryGyroBias();
        if (mFactoryGyroBiasAvailable) {
            setFactoryGyroBias();
        }
        getFactoryAccelBias();
        if (mFactoryAccelBiasAvailable) {
            setFactoryAccelBias();
        }
    }
    else
        ALOGE("HAL:Could not open or load MPL calibration file (%d)", rv);

    /* takes external accel calibration load workflow */
    if( m_pt2AccelCalLoadFunc != NULL) {
        long accel_offset[3];
        long tmp_offset[3];
        int result = m_pt2AccelCalLoadFunc(accel_offset);
        if(result)
            ALOGW("HAL:Vendor accelerometer calibration file load failed %d\n",
                  result);
        else {
            ALOGW("HAL:Vendor accelerometer calibration file successfully "
                  "loaded");
            inv_get_mpl_accel_bias(tmp_offset, NULL);
            ALOGV_IF(PROCESS_VERBOSE,
                     "HAL:Original accel offset, %ld, %ld, %ld\n",
                     tmp_offset[0], tmp_offset[1], tmp_offset[2]);
            inv_set_accel_bias_mask(accel_offset, mAccelAccuracy,4);
            inv_get_mpl_accel_bias(tmp_offset, NULL);
            ALOGV_IF(PROCESS_VERBOSE, "HAL:Set accel offset, %ld, %ld, %ld\n",
                     tmp_offset[0], tmp_offset[1], tmp_offset[2]);
        }
    }
    /* end of external accel calibration load workflow */

    /* disable all sensors and features */
    enableGyro(0);
    enableAccel(0);
    enableCompass(0,0);
    enablePressure(0);
    enableBatch(0);

    if (isLowPowerQuatEnabled()) {
        enableLPQuaternion(0);
    }
}

int MPLSensor::inv_constructor_init(void)
{
    VFUNC_LOG;

    inv_error_t result = inv_init_mpl();
    if (result) {
        ALOGE("HAL:inv_init_mpl() failed");
        return result;
    }
    result = inv_constructor_default_enable();
    result = inv_start_mpl();
    if (result) {
        ALOGE("HAL:inv_start_mpl() failed");
        LOG_RESULT_LOCATION(result);
        return result;
    }

    return result;
}

int MPLSensor::inv_constructor_default_enable(void)
{
    VFUNC_LOG;

    inv_error_t result;

/*******************************************************************************

********************************************************************************

The InvenSense binary file (libmplmpu.so) is subject to Google's standard terms
and conditions as accepted in the click-through agreement required to download
this library.
The library includes, but is not limited to the following function calls:
inv_enable_quaternion().

ANY VIOLATION OF SUCH TERMS AND CONDITIONS WILL BE STRICTLY ENFORCED.

********************************************************************************

*******************************************************************************/

    result = inv_enable_quaternion();
    if (result) {
        ALOGE("HAL:Cannot enable quaternion\n");
        return result;
    }

    result = inv_enable_in_use_auto_calibration();
    if (result) {
        return result;
    }

    result = inv_enable_fast_nomot();
    if (result) {
        return result;
    }

    result = inv_enable_gyro_tc();
    if (result) {
        return result;
    }

    result = inv_enable_hal_outputs();
    if (result) {
        return result;
    }

    if (mCompassSensor != NULL) {
        /* Invensense compass calibration */
        ALOGV_IF(ENG_VERBOSE, "HAL:Invensense vector compass cal enabled");
        result = inv_enable_vector_compass_cal();
        if (result) {
            LOG_RESULT_LOCATION(result);
            return result;
        } else {
            mMplFeatureActiveMask |= INV_COMPASS_CAL;
        }
        // specify MPL's trust weight, used by compass algorithms
        inv_vector_compass_cal_sensitivity(3);

        /* disabled by default
        result = inv_enable_compass_bias_w_gyro();
        if (result) {
            LOG_RESULT_LOCATION(result);
            return result;
        }
        */

        result = inv_enable_heading_from_gyro();
        if (result) {
            LOG_RESULT_LOCATION(result);
            return result;
        }

        result = inv_enable_magnetic_disturbance();
        if (result) {
            LOG_RESULT_LOCATION(result);
            return result;
        }
        //inv_enable_magnetic_disturbance_logging();
    }

    result = inv_enable_9x_sensor_fusion();
    if (result) {
        LOG_RESULT_LOCATION(result);
        return result;
    } else {
        // 9x sensor fusion enables Compass fit
        mMplFeatureActiveMask |= INV_COMPASS_FIT;
    }

    result = inv_enable_no_gyro_fusion();
    if (result) {
        LOG_RESULT_LOCATION(result);
        return result;
    }

    return result;
}

/* TODO: create function pointers to calculate scale */
void MPLSensor::inv_set_device_properties(void)
{
    VFUNC_LOG;

    unsigned short orient;

    inv_get_sensors_orientation();

    inv_set_gyro_sample_rate(DEFAULT_MPL_GYRO_RATE);
    inv_set_compass_sample_rate(DEFAULT_MPL_COMPASS_RATE);

    /* gyro setup */
    orient = inv_orientation_matrix_to_scalar(mGyroOrientation);
    inv_set_gyro_orientation_and_scale(orient, mGyroScale << 15);
    ALOGI_IF(EXTRA_VERBOSE, "HAL: Set MPL Gyro Scale %ld", mGyroScale << 15);

    /* accel setup */
    orient = inv_orientation_matrix_to_scalar(mAccelOrientation);
    /* use for third party accel input subsystem driver
    inv_set_accel_orientation_and_scale(orient, 1LL << 22);
    */
    inv_set_accel_orientation_and_scale(orient, (long)mAccelScale << 15);
    ALOGI_IF(EXTRA_VERBOSE,
             "HAL: Set MPL Accel Scale %ld", (long)mAccelScale << 15);

    /* compass setup */
    if (mCompassSensor != NULL) {
        signed char orientMtx[9];
        mCompassSensor->getMatrix(0, orientMtx);
        /* disable NVS matrix once fusion has it */
        mCompassSensor->setMatrix(0, matrixDisable);
        orient = inv_orientation_matrix_to_scalar(orientMtx);
        long sensitivity;
        sensitivity = (long)(mCompassSensor->getScale(-1) * (1L << 30));
        inv_set_compass_orientation_and_scale(orient, sensitivity);
        mCompassScale = sensitivity;
        ALOGI_IF(EXTRA_VERBOSE,
                 "HAL: Set MPL Compass Scale %ld", mCompassScale);
    }
}

void MPLSensor::loadDMP(void)
{
    VFUNC_LOG;

    int res, fd;
    FILE *fptr;

    fptr = fopen(mpu.dmp_firmware, "w");
    if(fptr == NULL) {
        ALOGI_IF(PROCESS_VERBOSE, "%s %s: No DMP support",
                 __func__, mpu.dmp_firmware);
    } else {
        if (inv_load_dmp(fptr) < 0 || fclose(fptr) < 0) {
            ALOGI_IF(PROCESS_VERBOSE, "HAL:load DMP failed");
        } else {
            mDmpLoaded = true;
            ALOGI_IF(PROCESS_VERBOSE, "%s:DMP loaded\n", __func__);
        }
    }

    // onDmp(1);    //Can't enable here. See note onDmp()
}

void MPLSensor::inv_get_sensors_orientation(void)
{
    VFUNC_LOG;

    /* get gyro orientation */
    if (mGyroSensor != NULL) {
        mGyroSensor->getMatrix(0, mGyroOrientation);
        /* disable NVS matrix once fusion has it */
        mGyroSensor->setMatrix(0, matrixDisable);
    }

    /* get accel orientation */
    if (mAccelSensor != NULL) {
        mAccelSensor->getMatrix(0, mAccelOrientation);
        /* disable NVS matrix once fusion has it */
        mAccelSensor->setMatrix(0, matrixDisable);
    }
}

MPLSensor::~MPLSensor()
{
    VFUNC_LOG;

    /* Close open fds */
    if (sysfs_names_ptr)
        free(sysfs_names_ptr);

    if (accel_x_dmp_bias_fd > 0) {
        close(accel_x_dmp_bias_fd);
    }
    if (accel_y_dmp_bias_fd > 0) {
        close(accel_y_dmp_bias_fd);
    }
    if (accel_z_dmp_bias_fd > 0) {
        close(accel_z_dmp_bias_fd);
    }

    if (gyro_x_dmp_bias_fd > 0) {
        close(gyro_x_dmp_bias_fd);
    }
    if (gyro_y_dmp_bias_fd > 0) {
        close(gyro_y_dmp_bias_fd);
    }
    if (gyro_z_dmp_bias_fd > 0) {
        close(gyro_z_dmp_bias_fd);
    }

#ifdef INV_PLAYBACK_DBG
    inv_turn_off_data_logging();
    if (fclose(logfile) < 0) {
        ALOGE("cannot close debug log file");
    }
#endif
}

#define GY_ENABLED  ((1 << Gyro) & enabled_sensors)
#define RGY_ENABLED ((1 << RawGyro) & enabled_sensors)
#define A_ENABLED   ((1 << Accelerometer)  & enabled_sensors)
#define M_ENABLED   ((1 << MagneticField) & enabled_sensors)
#define RM_ENABLED  ((1 << RawMagneticField) & enabled_sensors)
#define PS_ENABLED  ((1 << Pressure) & enabled_sensors)
#define O_ENABLED   ((1 << Orientation)  & enabled_sensors)
#define LA_ENABLED  ((1 << LinearAccel) & enabled_sensors)
#define GR_ENABLED  ((1 << Gravity) & enabled_sensors)
#define RV_ENABLED  ((1 << RotationVector) & enabled_sensors)
#define GRV_ENABLED ((1 << GameRotationVector) & enabled_sensors)
#define GMRV_ENABLED ((1 << GeomagneticRotationVector) & enabled_sensors)

int MPLSensor::onDmp(int en)
{
    VFUNC_LOG;

    int res = -1;
    int status;
    mDmpOn = en;

    //Sequence to enable DMP
    //1. Load DMP image if not already loaded
    //2. Either Gyro or Accel must be enabled/configured before next step
    //3. Enable DMP

    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:cat %s (%lld)",
             mpu.firmware_loaded, getTimestamp());
    if(read_sysfs_int(mpu.firmware_loaded, &status) < 0){
        ALOGE("HAL:ERR can't get firmware_loaded status");
    } else if (status == 1) {
        //Write only if curr DMP state <> request
        ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:cat %s (%lld)",
                 mpu.dmp_on, getTimestamp());
        if (read_sysfs_int(mpu.dmp_on, &status) < 0) {
            ALOGE("HAL:ERR can't read DMP state");
        } else if (status != en) {
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     en, mpu.dmp_on, getTimestamp());
            if (write_sysfs_int(mpu.dmp_on, en) < 0) {
                ALOGE("HAL:ERR can't write dmp_on");
            } else {
                mDmpOn = en;
                res = 0;    //Indicate write successful
                if(!en) {
                    setAccelBias();
                }
            }
            //Enable DMP interrupt
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                    en, mpu.dmp_int_on, getTimestamp());
            if (write_sysfs_int(mpu.dmp_int_on, en) < 0) {
                ALOGE("HAL:ERR can't en/dis DMP interrupt");
            }

            // disable DMP event interrupt if needed
            if (!en) {
                ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                         en, mpu.dmp_event_int_on, getTimestamp());
                if (write_sysfs_int(mpu.dmp_event_int_on, en) < 0) {
                    res = -1;
                    ALOGE("HAL:ERR can't enable DMP event interrupt");
                }
            }
        } else {
            mDmpOn = en;
            res = 0;    //DMP already set as requested
            if(!en) {
                setAccelBias();
            }
        }
    } else {
        ALOGE("HAL:ERR No DMP image");
    }
    return res;
}

int MPLSensor::setDmpFeature(int en)
{
    int res = 0;

    // set sensor engine and fifo
    if ((mFeatureActiveMask & DMP_FEATURE_MASK) || en) {
        if ((mFeatureActiveMask & INV_DMP_6AXIS_QUATERNION) ||
                (mFeatureActiveMask & INV_DMP_PED_QUATERNION) ||
                (mFeatureActiveMask & INV_DMP_QUATERNION)) {
            res = enableGyro(1);
            if (res < 0) {
                return res;
            }
            if (!(mLocalSensorMask & mMasterSensorMask & INV_THREE_AXIS_GYRO)) {
                res = turnOffGyroFifo();
                if (res < 0) {
                    return res;
                }
            }
        }
        res = enableAccel(1);
        if (res < 0) {
            return res;
        }
        if (!(mLocalSensorMask & mMasterSensorMask & INV_THREE_AXIS_ACCEL)) {
            res = turnOffAccelFifo();
            if (res < 0) {
                return res;
            }
        }
    } else {
        if (!(mLocalSensorMask & mMasterSensorMask & INV_THREE_AXIS_GYRO)) {
            res = enableGyro(0);
            if (res < 0) {
                return res;
            }
        }
        if (!(mLocalSensorMask & mMasterSensorMask & INV_THREE_AXIS_ACCEL)) {
            res = enableAccel(0);
            if (res < 0) {
                return res;
            }
        }
    }

    // set sensor data interrupt
    uint32_t dataInterrupt = (mEnabled || (mFeatureActiveMask & INV_DMP_BATCH_MODE));
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
             !dataInterrupt, mpu.dmp_event_int_on, getTimestamp());
    if (write_sysfs_int(mpu.dmp_event_int_on, !dataInterrupt) < 0) {
        res = -1;
        ALOGE("HAL:ERR can't enable DMP event interrupt");
    }
    return res;
}

int MPLSensor::computeAndSetDmpState()
{
    int res = 0;
    bool dmpState = 0;

    if (mFeatureActiveMask) {
        dmpState = 1;
        ALOGV_IF(PROCESS_VERBOSE, "HAL:computeAndSetDmpState() mFeatureActiveMask = 1");
    } else if ((mEnabled & VIRTUAL_SENSOR_9AXES_MASK)
                        || (mEnabled & VIRTUAL_SENSOR_GYRO_6AXES_MASK)) {
            if (checkLPQuaternion() && checkLPQRateSupported()) {
                dmpState = 1;
                ALOGV_IF(PROCESS_VERBOSE, "HAL:computeAndSetDmpState() Sensor Fusion = 1");
            }
    } /*else {
        unsigned long sensor = mLocalSensorMask & mMasterSensorMask;
        if (sensor & (INV_THREE_AXIS_ACCEL & INV_THREE_AXIS_GYRO)) {
            dmpState = 1;
            LOGV_IF(PROCESS_VERBOSE, "HAL:computeAndSetDmpState() accel and gyro = 1");
        }
    }*/

    // set Dmp state
    res = onDmp(dmpState);
    if (res < 0)
        return res;

    if (dmpState) {
        // set DMP rate to 200Hz
        ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                 200, mpu.accel_fifo_rate, getTimestamp());
        if (write_sysfs_int(mpu.accel_fifo_rate, 200) < 0) {
            res = -1;
            ALOGE("HAL:ERR can't set rate to 200Hz");
            return res;
        }
    }
    ALOGV_IF(PROCESS_VERBOSE, "HAL:DMP is set %s", (dmpState ? "on" : "off"));
    return dmpState;
}

/* called when batch and hw sensor enabled*/
int MPLSensor::enablePedIndicator(int en)
{
    VFUNC_LOG;

    int res = 0;
    if (en) {
        if (!(mFeatureActiveMask & INV_DMP_PED_QUATERNION)) {
            //Disable DMP Pedometer Interrupt
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     0, mpu.pedometer_int_on, getTimestamp());
            if (write_sysfs_int(mpu.pedometer_int_on, 0) < 0) {
               ALOGE("HAL:ERR can't enable Android Pedometer Interrupt");
               res = -1;   // indicate an err
               return res;
            }
        }
    }

    ALOGV_IF(ENG_VERBOSE, "HAL:Toggling step indicator to %d", en);
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                en, mpu.step_indicator_on, getTimestamp());
    if (write_sysfs_int(mpu.step_indicator_on, en) < 0) {
        res = -1;
        ALOGE("HAL:ERR can't write to DMP step_indicator_on");
    }
    return res;
}

int MPLSensor::checkPedStandaloneEnabled(void)
{
    VFUNC_LOG;
    return ((mFeatureActiveMask & INV_DMP_PED_STANDALONE)? 1:0);
}

/* This feature is only used in batch mode */
/* Stand-alone Step Detector */
int MPLSensor::enablePedStandalone(int en)
{
    VFUNC_LOG;

    if (!en) {
        enablePedStandaloneData(0);
        mFeatureActiveMask &= ~INV_DMP_PED_STANDALONE;
        if (mFeatureActiveMask == 0) {
            onDmp(0);
        } else if(mFeatureActiveMask & INV_DMP_PEDOMETER) {
             //Re-enable DMP Pedometer Interrupt
             ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                      1, mpu.pedometer_int_on, getTimestamp());
             if (write_sysfs_int(mpu.pedometer_int_on, 1) < 0) {
                 ALOGE("HAL:ERR can't enable Android Pedometer Interrupt");
                 return (-1);
             }
            //Disable data interrupt if no continuous data
            if (mEnabled == 0) {
                ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                         1, mpu.dmp_event_int_on, getTimestamp());
                if (write_sysfs_int(mpu.dmp_event_int_on, 1) < 0) {
                    ALOGE("HAL:ERR can't enable DMP event interrupt");
                    return (-1);
                }
            }
        }
        ALOGV_IF(ENG_VERBOSE, "HAL:Ped Standalone disabled");
    } else {
        if (enablePedStandaloneData(1) < 0 || onDmp(1) < 0) {
            ALOGE("HAL:ERR can't enable Ped Standalone");
        } else {
            mFeatureActiveMask |= INV_DMP_PED_STANDALONE;
            //Disable DMP Pedometer Interrupt
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                    0, mpu.pedometer_int_on, getTimestamp());
            if (write_sysfs_int(mpu.pedometer_int_on, 0) < 0) {
                ALOGE("HAL:ERR can't disable Android Pedometer Interrupt");
                return (-1);
            }
            //Enable Data Interrupt
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     0, mpu.dmp_event_int_on, getTimestamp());
            if (write_sysfs_int(mpu.dmp_event_int_on, 0) < 0) {
                ALOGE("HAL:ERR can't enable DMP event interrupt");
                return (-1);
            }
            ALOGV_IF(ENG_VERBOSE, "HAL:Ped Standalone enabled");
        }
    }
    return 0;
}

int MPLSensor:: enablePedStandaloneData(int en)
{
    VFUNC_LOG;

    int res = 0;

    // Set DMP Ped standalone
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
             en, mpu.step_detector_on, getTimestamp());
    if (write_sysfs_int(mpu.step_detector_on, en) < 0) {
        ALOGE("HAL:ERR can't write DMP step_detector_on");
        res = -1;   //Indicate an err
    }

    // Set DMP Step indicator
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
             en, mpu.step_indicator_on, getTimestamp());
    if (write_sysfs_int(mpu.step_indicator_on, en) < 0) {
        ALOGE("HAL:ERR can't write DMP step_indicator_on");
        res = -1;   //Indicate an err
    }

    if (!en) {
      ALOGV_IF(ENG_VERBOSE, "HAL:Disabling ped standalone");
      //Disable Accel if no sensor needs it
      if (!(mFeatureActiveMask & DMP_FEATURE_MASK)
                               && (!(mLocalSensorMask & mMasterSensorMask
                                                   & INV_THREE_AXIS_ACCEL))) {
          res = enableAccel(0);
          if (res < 0)
              return res;
      }
      if (!(mFeatureActiveMask & DMP_FEATURE_MASK)
                               && (!(mLocalSensorMask & mMasterSensorMask
                                                   & INV_THREE_AXIS_GYRO))) {
          res = enableGyro(0);
          if (res < 0)
              return res;
      }
    } else {
        ALOGV_IF(ENG_VERBOSE, "HAL:Enabling ped standalone");
        // enable accel engine
        res = enableAccel(1);
        if (res < 0)
            return res;
        ALOGV_IF(EXTRA_VERBOSE, "mLocalSensorMask=0x%lx", mLocalSensorMask);
        // disable accel FIFO
        if (!((mLocalSensorMask & mMasterSensorMask) & INV_THREE_AXIS_ACCEL)) {
            res = turnOffAccelFifo();
            if (res < 0)
                return res;
        }
    }

    return res;
}

int MPLSensor::checkPedQuatEnabled(void)
{
    VFUNC_LOG;
    return ((mFeatureActiveMask & INV_DMP_PED_QUATERNION)? 1:0);
}

/* This feature is only used in batch mode */
/* Step Detector && Game Rotation Vector */
int MPLSensor::enablePedQuaternion(int en)
{
    VFUNC_LOG;

    if (!en) {
        enablePedQuaternionData(0);
        mFeatureActiveMask &= ~INV_DMP_PED_QUATERNION;
        if (mFeatureActiveMask == 0) {
            onDmp(0);
        } else if(mFeatureActiveMask & INV_DMP_PEDOMETER) {
             //Re-enable DMP Pedometer Interrupt
             ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                      1, mpu.pedometer_int_on, getTimestamp());
             if (write_sysfs_int(mpu.pedometer_int_on, 1) < 0) {
                 ALOGE("HAL:ERR can't enable Android Pedometer Interrupt");
                 return (-1);
             }
            //Disable data interrupt if no continuous data
            if (mEnabled == 0) {
                ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                         1, mpu.dmp_event_int_on, getTimestamp());
                if (write_sysfs_int(mpu.dmp_event_int_on, en) < 0) {
                    ALOGE("HAL:ERR can't enable DMP event interrupt");
                    return (-1);
                }
            }
        }
        ALOGV_IF(ENG_VERBOSE, "HAL:Ped Quat disabled");
    } else {
        if (enablePedQuaternionData(1) < 0 || onDmp(1) < 0) {
            ALOGE("HAL:ERR can't enable Ped Quaternion");
        } else {
            mFeatureActiveMask |= INV_DMP_PED_QUATERNION;
            //Disable DMP Pedometer Interrupt
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     0, mpu.pedometer_int_on, getTimestamp());
            if (write_sysfs_int(mpu.pedometer_int_on, 0) < 0) {
                ALOGE("HAL:ERR can't disable Android Pedometer Interrupt");
                return (-1);
            }
            //Enable Data Interrupt
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     0, mpu.dmp_event_int_on, getTimestamp());
            if (write_sysfs_int(mpu.dmp_event_int_on, 0) < 0) {
                ALOGE("HAL:ERR can't enable DMP event interrupt");
                return (-1);
            }
            ALOGV_IF(ENG_VERBOSE, "HAL:Ped Quat enabled");
        }
    }
    return 0;
}

int MPLSensor::enablePedQuaternionData(int en)
{
    VFUNC_LOG;

    int res = 0;

    // Enable DMP quaternion
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
             en, mpu.ped_q_on, getTimestamp());
    if (write_sysfs_int(mpu.ped_q_on, en) < 0) {
        ALOGE("HAL:ERR can't write DMP ped_q_on");
        res = -1;   //Indicate an err
    }

    if (!en) {
        ALOGV_IF(ENG_VERBOSE, "HAL:Disabling ped quat");
        //Disable Accel if no sensor needs it
        if (!(mFeatureActiveMask & DMP_FEATURE_MASK)
                               && (!(mLocalSensorMask & mMasterSensorMask
                                                   & INV_THREE_AXIS_ACCEL))) {
          res = enableAccel(0);
          if (res < 0)
              return res;
        }
        if (!(mFeatureActiveMask & DMP_FEATURE_MASK)
                               && (!(mLocalSensorMask & mMasterSensorMask
                                                   & INV_THREE_AXIS_GYRO))) {
            res = enableGyro(0);
            if (res < 0)
                return res;
        }
        if (mFeatureActiveMask & INV_DMP_QUATERNION) {
            res = write_sysfs_int(mpu.gyro_fifo_enable, 1);
            res += write_sysfs_int(mpu.accel_fifo_enable, 1);
            if (res < 0)
                return res;
        }
        // ALOGV_IF(ENG_VERBOSE, "before mLocalSensorMask=0x%lx", mLocalSensorMask);
        // reset global mask for buildMpuEvent()
        if (mEnabled & (1 << GameRotationVector)) {
            mLocalSensorMask |= INV_THREE_AXIS_GYRO;
            mLocalSensorMask |= INV_THREE_AXIS_ACCEL;
        } else if (mEnabled & (1 << Accelerometer)) {
            mLocalSensorMask |= INV_THREE_AXIS_ACCEL;
        } else if ((mEnabled & ( 1 << Gyro)) ||
            (mEnabled & (1 << RawGyro))) {
            mLocalSensorMask |= INV_THREE_AXIS_GYRO;
        }
        //ALOGV_IF(ENG_VERBOSE, "after mLocalSensorMask=0x%lx", mLocalSensorMask);
    } else {
        ALOGV_IF(PROCESS_VERBOSE, "HAL:Enabling ped quat");
        // enable accel engine
        res = enableAccel(1);
        if (res < 0)
            return res;

        // enable gyro engine
        res = enableGyro(1);
        if (res < 0)
            return res;
        ALOGV_IF(EXTRA_VERBOSE, "mLocalSensorMask=0x%lx", mLocalSensorMask);
        // disable accel FIFO
        if ((!((mLocalSensorMask & mMasterSensorMask) & INV_THREE_AXIS_ACCEL)) ||
                !(mBatchEnabled & (1 << Accelerometer))) {
            res = turnOffAccelFifo();
            if (res < 0)
                return res;
            mLocalSensorMask &= ~INV_THREE_AXIS_ACCEL;
        }

        // disable gyro FIFO
        if ((!((mLocalSensorMask & mMasterSensorMask) & INV_THREE_AXIS_GYRO)) ||
                !((mBatchEnabled & (1 << Gyro)) || (mBatchEnabled & (1 << RawGyro)))) {
            res = turnOffGyroFifo();
            if (res < 0)
                return res;
            mLocalSensorMask &= ~INV_THREE_AXIS_GYRO;
        }
    }

    return res;
}

int MPLSensor::check6AxisQuatEnabled(void)
{
    VFUNC_LOG;
    return ((mFeatureActiveMask & INV_DMP_6AXIS_QUATERNION)? 1:0);
}

/* This is used for batch mode only */
/* GRV is batched but not along with ped */
int MPLSensor::enable6AxisQuaternion(int en)
{
    VFUNC_LOG;

    if (!en) {
        enable6AxisQuaternionData(0);
        mFeatureActiveMask &= ~INV_DMP_6AXIS_QUATERNION;
        if (mFeatureActiveMask == 0) {
            onDmp(0);
        }
        ALOGV_IF(ENG_VERBOSE, "HAL:6 Axis Quat disabled");
    } else {
        if (enable6AxisQuaternionData(1) < 0 || onDmp(1) < 0) {
            ALOGE("HAL:ERR can't enable 6 Axis Quaternion");
        } else {
            mFeatureActiveMask |= INV_DMP_6AXIS_QUATERNION;
            ALOGV_IF(PROCESS_VERBOSE, "HAL:6 Axis Quat enabled");
        }
    }
    return 0;
}

int MPLSensor::enable6AxisQuaternionData(int en)
{
    VFUNC_LOG;

    int res = 0;

    // Enable DMP quaternion
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
             en, mpu.six_axis_q_on, getTimestamp());
    if (write_sysfs_int(mpu.six_axis_q_on, en) < 0) {
        ALOGE("HAL:ERR can't write DMP six_axis_q_on");
        res = -1;   //Indicate an err
    }

    if (!en) {
        ALOGV_IF(EXTRA_VERBOSE, "HAL:DMP six axis quaternion data was turned off");
        inv_quaternion_sensor_was_turned_off();
        if (!(mFeatureActiveMask & DMP_FEATURE_MASK)
                                 && (!(mLocalSensorMask & mMasterSensorMask
                                                     & INV_THREE_AXIS_ACCEL))) {
            res = enableAccel(0);
            if (res < 0)
                return res;
        }
        if (!(mFeatureActiveMask & DMP_FEATURE_MASK)
                                 && (!(mLocalSensorMask & mMasterSensorMask
                                                     & INV_THREE_AXIS_GYRO))) {
            res = enableGyro(0);
            if (res < 0)
                return res;
        }
        if (mFeatureActiveMask & INV_DMP_QUATERNION) {
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     1, mpu.gyro_fifo_enable, getTimestamp());
            res = write_sysfs_int(mpu.gyro_fifo_enable, 1);
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     1, mpu.accel_fifo_enable, getTimestamp());
            res += write_sysfs_int(mpu.accel_fifo_enable, 1);
            if (res < 0)
                return res;
        }
        ALOGV_IF(ENG_VERBOSE, "  k=0x%lx", mLocalSensorMask);
        // reset global mask for buildMpuEvent()
        if (mEnabled & (1 << GameRotationVector)) {
            if (!(mFeatureActiveMask & INV_DMP_PED_QUATERNION)) {
                mLocalSensorMask |= INV_THREE_AXIS_GYRO;
                mLocalSensorMask |= INV_THREE_AXIS_ACCEL;
                res = write_sysfs_int(mpu.gyro_fifo_enable, 1);
                res += write_sysfs_int(mpu.accel_fifo_enable, 1);
                if (res < 0)
                    return res;
            }
        } else if (mEnabled & (1 << Accelerometer)) {
            mLocalSensorMask |= INV_THREE_AXIS_ACCEL;
        } else if ((mEnabled & ( 1 << Gyro)) ||
                (mEnabled & (1 << RawGyro))) {
            mLocalSensorMask |= INV_THREE_AXIS_GYRO;
        }
        ALOGV_IF(ENG_VERBOSE, "after mLocalSensorMask=0x%lx", mLocalSensorMask);
    } else {
        ALOGV_IF(PROCESS_VERBOSE, "HAL:Enabling six axis quat");
        if (mEnabled & ( 1 << GameRotationVector)) {
            // enable accel engine
            res = enableAccel(1);
            if (res < 0)
                return res;

            // enable gyro engine
            res = enableGyro(1);
            if (res < 0)
                return res;
            ALOGV_IF(EXTRA_VERBOSE, "before: mLocalSensorMask=0x%lx", mLocalSensorMask);
            if ((!(mLocalSensorMask & mMasterSensorMask & INV_THREE_AXIS_ACCEL)) ||
                   (!(mBatchEnabled & (1 << Accelerometer)) ||
                       (!(mEnabled & (1 << Accelerometer))))) {
                res = turnOffAccelFifo();
                if (res < 0)
                    return res;
                mLocalSensorMask &= ~INV_THREE_AXIS_ACCEL;
            }

            if ((!(mLocalSensorMask & mMasterSensorMask & INV_THREE_AXIS_GYRO)) ||
                    (!(mBatchEnabled & (1 << Gyro)) ||
                       (!(mEnabled & (1 << Gyro))))) {
                if (!(mBatchEnabled & (1 << RawGyro)) ||
                        (!(mEnabled & (1 << RawGyro)))) {
                    res = turnOffGyroFifo();
                    if (res < 0)
                        return res;
                     mLocalSensorMask &= ~INV_THREE_AXIS_GYRO;
                     }
            }
            ALOGV_IF(ENG_VERBOSE, "after: mLocalSensorMask=0x%lx", mLocalSensorMask);
        }
    }

    return res;
}

/* this is for batch  mode only */
int MPLSensor::checkLPQRateSupported(void)
{
    VFUNC_LOG;
#ifndef USE_LPQ_AT_FASTEST
    return ((mDelays[GameRotationVector] <= RATE_200HZ) ? 0 :1);
#else
    return 1;
#endif
}

int MPLSensor::checkLPQuaternion(void)
{
    VFUNC_LOG;
    return ((mFeatureActiveMask & INV_DMP_QUATERNION)? 1:0);
}

int MPLSensor::enableLPQuaternion(int en)
{
    VFUNC_LOG;

    if (!en) {
        enableQuaternionData(0);
        mFeatureActiveMask &= ~INV_DMP_QUATERNION;
        if (mFeatureActiveMask == 0) {
            onDmp(0);
        }
        ALOGV_IF(ENG_VERBOSE, "HAL:LP Quat disabled");
    } else {
        if (enableQuaternionData(1) < 0 || onDmp(1) < 0) {
            ALOGE("HAL:ERR can't enable LP Quaternion");
        } else {
            mFeatureActiveMask |= INV_DMP_QUATERNION;
            ALOGV_IF(ENG_VERBOSE, "HAL:LP Quat enabled");
        }
    }
    return 0;
}

int MPLSensor::enableQuaternionData(int en)
{
    VFUNC_LOG;

    int res = 0;

    // Enable DMP quaternion
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
             en, mpu.three_axis_q_on, getTimestamp());
    if (write_sysfs_int(mpu.three_axis_q_on, en) < 0) {
        ALOGE("HAL:ERR can't write DMP three_axis_q__on");
        res = -1;   //Indicates an err
    }

    if (!en) {
        ALOGV_IF(ENG_VERBOSE, "HAL:DMP quaternion data was turned off");
        inv_quaternion_sensor_was_turned_off();
    } else {
        ALOGV_IF(ENG_VERBOSE, "HAL:Enabling three axis quat");
    }

    return res;
}

int MPLSensor::enableDmpPedometer(int en, int interruptMode)
{
    VFUNC_LOG;
    int res = 0;
    int enabled_sensors = mEnabled;

    if (en == 1) {
        //Enable DMP Pedometer Function
        ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                 en, mpu.pedometer_on, getTimestamp());
        if (write_sysfs_int(mpu.pedometer_on, en) < 0) {
            ALOGE("HAL:ERR can't enable Android Pedometer");
            res = -1;   // indicate an err
            return res;
        }

        if (interruptMode || (mFeatureActiveMask & INV_DMP_PEDOMETER)) {
            //Enable DMP Pedometer Interrupt
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     en, mpu.pedometer_int_on, getTimestamp());
            if (write_sysfs_int(mpu.pedometer_int_on, en) < 0) {
                ALOGE("HAL:ERR can't enable Android Pedometer Interrupt");
                res = -1;   // indicate an err
                return res;
            }
        }

        if (interruptMode) {
            mFeatureActiveMask |= INV_DMP_PEDOMETER;
        }
        else {
            mFeatureActiveMask |= INV_DMP_PEDOMETER_STEP;
            mStepCountPollTime = 100000000LL;
        }

        clock_gettime(CLOCK_MONOTONIC, &mt_pre);
    } else {
        if (interruptMode) {
            mFeatureActiveMask &= ~INV_DMP_PEDOMETER;
        }
        else {
            mFeatureActiveMask &= ~INV_DMP_PEDOMETER_STEP;
            mStepCountPollTime = -1;
        }

        /* if neither step detector or step count is on */
        if (!(mFeatureActiveMask & (INV_DMP_PEDOMETER | INV_DMP_PEDOMETER_STEP))) {
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     en, mpu.pedometer_on, getTimestamp());
            if (write_sysfs_int(mpu.pedometer_on, en) < 0) {
                ALOGE("HAL:ERR can't enable Android Pedometer");
                res = -1;
                return res;
            }
        }

        /* if feature is not step detector */
        if (!(mFeatureActiveMask & INV_DMP_PEDOMETER)) {
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     en, mpu.pedometer_int_on, getTimestamp());
            if (write_sysfs_int(mpu.pedometer_int_on, en) < 0) {
                ALOGE("HAL:ERR can't enable Android Pedometer Interrupt");
                res = -1;
                return res;
            }
        }
    }

    if ((res = setDmpFeature(en)) < 0)
        return res;

    if ((res = computeAndSetDmpState()) < 0)
        return res;

    if (resetDataRates() < 0)
        return res;

    return res;
}

int MPLSensor::enableGyro(int en)
{
    VFUNC_LOG;
    int res;

    if (mGyroSensor == NULL)
        return 0;

    res = mGyroSensor->enable(0, en);
    if ((!en) || (res != 0)) {
        ALOGV_IF(EXTRA_VERBOSE, "HAL:MPL:inv_gyro_was_turned_off");
        inv_gyro_was_turned_off();
    }

    /* also enable/disable the temperature for gyro calculations */
    if (mGyroTempSensor != NULL)
        res = mGyroTempSensor->enable(0, en);

    return res;
}

int MPLSensor::enableAccel(int en)
{
    VFUNC_LOG;
    int res;

    if (mAccelSensor == NULL)
        return 0;

    res = mAccelSensor->enable(0, en);
    if ((!en) || (res != 0)) {
        ALOGV_IF(EXTRA_VERBOSE, "HAL:MPL:inv_accel_was_turned_off");
        inv_accel_was_turned_off();
    }
    return res;
}

int MPLSensor::enableCompass(int en, int rawSensorRequested)
{
    VFUNC_LOG;
    int res;

    if (mCompassSensor == NULL)
        return 0;

    res = mCompassSensor->enable(0, en);
    if (en == 0 || res != 0) {
        ALOGV_IF(EXTRA_VERBOSE, "HAL:MPL:inv_compass_was_turned_off %d", res);
        inv_compass_was_turned_off();
    }
    return res;
}

int MPLSensor::enablePressure(int en)
{
    VFUNC_LOG;
    int res;

    if (mPressureSensor == NULL)
        return 0;

    res = mPressureSensor->enable(0, en);
    return res;
}

/* use this function for initialization */
int MPLSensor::enableBatch(int64_t timeout)
{
    VFUNC_LOG;

    int res = 0;

    res = write_sysfs_int(mpu.batchmode_timeout, timeout);
    if (timeout == 0) {
        res = write_sysfs_int(mpu.six_axis_q_on, 0);
        res = write_sysfs_int(mpu.ped_q_on, 0);
        res = write_sysfs_int(mpu.step_detector_on, 0);
        res = write_sysfs_int(mpu.step_indicator_on, 0);
    }

    if (timeout == 0) {
        ALOGV_IF(EXTRA_VERBOSE, "HAL:MPL:batchmode timeout is zero");
    }

    return res;
}

void MPLSensor::computeLocalSensorMask(int enabled_sensors)
{
    VFUNC_LOG;

    do {
        /* Invensense Pressure on secondary bus */
        if (PS_ENABLED) {
            ALOGV_IF(ENG_VERBOSE, "PS ENABLED");
            mLocalSensorMask |= INV_ONE_AXIS_PRESSURE;
        } else {
            ALOGV_IF(ENG_VERBOSE, "PS DISABLED");
            mLocalSensorMask &= ~INV_ONE_AXIS_PRESSURE;
        }

        if (LA_ENABLED || GR_ENABLED || RV_ENABLED || O_ENABLED
                       || (GRV_ENABLED && GMRV_ENABLED)) {
            ALOGV_IF(ENG_VERBOSE, "FUSION ENABLED");
            mLocalSensorMask |= ALL_MPL_SENSORS_NP;
            break;
        }

        if (GRV_ENABLED) {
            if (!(mFeatureActiveMask & INV_DMP_BATCH_MODE) ||
                !(mBatchEnabled & (1 << GameRotationVector))) {
                ALOGV_IF(ENG_VERBOSE, "6 Axis Fusion ENABLED");
                mLocalSensorMask |= INV_THREE_AXIS_GYRO;
                mLocalSensorMask |= INV_THREE_AXIS_ACCEL;
            } else {
                if (GY_ENABLED || RGY_ENABLED) {
                    ALOGV_IF(ENG_VERBOSE, "G ENABLED");
                    mLocalSensorMask |= INV_THREE_AXIS_GYRO;
                } else {
                    ALOGV_IF(ENG_VERBOSE, "G DISABLED");
                    mLocalSensorMask &= ~INV_THREE_AXIS_GYRO;
                }
                if (A_ENABLED) {
                    ALOGV_IF(ENG_VERBOSE, "A ENABLED");
                    mLocalSensorMask |= INV_THREE_AXIS_ACCEL;
                } else {
                    ALOGV_IF(ENG_VERBOSE, "A DISABLED");
                    mLocalSensorMask &= ~INV_THREE_AXIS_ACCEL;
                }
            }
            /* takes care of MAG case */
            if (M_ENABLED || RM_ENABLED) {
                ALOGV_IF(1, "M ENABLED");
                mLocalSensorMask |= INV_THREE_AXIS_COMPASS;
            } else {
                ALOGV_IF(1, "M DISABLED");
                mLocalSensorMask &= ~INV_THREE_AXIS_COMPASS;
            }
            break;
        }

        if (GMRV_ENABLED) {
            ALOGV_IF(ENG_VERBOSE, "6 Axis Geomagnetic Fusion ENABLED");
            mLocalSensorMask |= INV_THREE_AXIS_ACCEL;
            mLocalSensorMask |= INV_THREE_AXIS_COMPASS;

            /* takes care of Gyro case */
            if (GY_ENABLED || RGY_ENABLED) {
                ALOGV_IF(ENG_VERBOSE, "G ENABLED");
                mLocalSensorMask |= INV_THREE_AXIS_GYRO;
            } else {
                ALOGV_IF(ENG_VERBOSE, "G DISABLED");
                mLocalSensorMask &= ~INV_THREE_AXIS_GYRO;
            }
            break;
        }

        if(!A_ENABLED && !M_ENABLED && !RM_ENABLED &&
               !GRV_ENABLED && !GMRV_ENABLED && !GY_ENABLED && !RGY_ENABLED &&
               !PS_ENABLED) {
            /* Invensense compass cal */
            ALOGV_IF(ENG_VERBOSE, "ALL DISABLED");
            mLocalSensorMask = 0;
            break;
        }

        if (GY_ENABLED || RGY_ENABLED) {
            ALOGV_IF(ENG_VERBOSE, "G ENABLED");
            mLocalSensorMask |= INV_THREE_AXIS_GYRO;
        } else {
            ALOGV_IF(ENG_VERBOSE, "G DISABLED");
            mLocalSensorMask &= ~INV_THREE_AXIS_GYRO;
        }

        if (A_ENABLED) {
            ALOGV_IF(ENG_VERBOSE, "A ENABLED");
            mLocalSensorMask |= INV_THREE_AXIS_ACCEL;
        } else {
            ALOGV_IF(ENG_VERBOSE, "A DISABLED");
            mLocalSensorMask &= ~INV_THREE_AXIS_ACCEL;
        }

        /* Invensense compass calibration */
        if (M_ENABLED || RM_ENABLED) {
            ALOGV_IF(ENG_VERBOSE, "M ENABLED");
            mLocalSensorMask |= INV_THREE_AXIS_COMPASS;
        } else {
            ALOGV_IF(ENG_VERBOSE, "M DISABLED");
            mLocalSensorMask &= ~INV_THREE_AXIS_COMPASS;
        }
    } while (0);
}

int MPLSensor::enableSensors(unsigned long sensors, int en, uint32_t changed)
{
    VFUNC_LOG;

    inv_error_t res = -1;
    uint32_t mask;
    int on = 1;
    int off = 0;
    int cal_stored = 0;

    // Sequence to enable or disable a sensor
    // 1. reset master enable (=0)
    // 2. enable or disable a sensor
    // 3. set master enable (=1)

    mask = ((1 << Gyro) | (1 << RawGyro) | (1 << Accelerometer));
    if (mCompassSensor != NULL)
        mask |= ((1 << MagneticField) | (1 << RawMagneticField));
    if (mPressureSensor != NULL)
        mask |= (1 << Pressure);

    ALOGV_IF(ENG_VERBOSE, "HAL:enableSensors - sensors: 0x%0x",
             (unsigned int)sensors);

    if (changed & ((1 << Gyro) | (1 << RawGyro))) {
        ALOGV_IF(ENG_VERBOSE, "HAL:enableSensors - gyro %s",
                 (sensors & INV_THREE_AXIS_GYRO? "enable": "disable"));
        res = enableGyro(!!(sensors & INV_THREE_AXIS_GYRO));
        if(res < 0) {
            return res;
        }

        if (!cal_stored && (!en && (changed & (1 << Gyro)))) {
            storeCalibration();
            cal_stored = 1;
        }
    }

    if (changed & (1 << Accelerometer)) {
        ALOGV_IF(ENG_VERBOSE, "HAL:enableSensors - accel %s",
                 (sensors & INV_THREE_AXIS_ACCEL? "enable": "disable"));
        res = enableAccel(!!(sensors & INV_THREE_AXIS_ACCEL));
        if(res < 0) {
            return res;
        }

        if (!(sensors & INV_THREE_AXIS_ACCEL) && !cal_stored) {
            storeCalibration();
            cal_stored = 1;
        }
    }

    if (changed & ((1 << MagneticField) | (1 << RawMagneticField))) {
        ALOGV_IF(ENG_VERBOSE, "HAL:enableSensors - compass %s",
                 (sensors & INV_THREE_AXIS_COMPASS? "enable": "disable"));
        res = enableCompass(!!(sensors & INV_THREE_AXIS_COMPASS), changed & (1 << RawMagneticField));
        if(res < 0) {
            return res;
        }

        if (!cal_stored && (!en && (changed & (1 << MagneticField)))) {
            storeCalibration();
            cal_stored = 1;
        }
    }

     if (changed & (1 << Pressure)) {
        ALOGV_IF(ENG_VERBOSE, "HAL:enableSensors - pressure %s",
                 (sensors & INV_ONE_AXIS_PRESSURE? "enable": "disable"));
        res = enablePressure(!!(sensors & INV_ONE_AXIS_PRESSURE));
        if(res < 0) {
            return res;
        }
    }

    if (isLowPowerQuatEnabled()) {
        // Enable LP Quat
        if ((mEnabled & VIRTUAL_SENSOR_9AXES_MASK)
                      || (mEnabled & VIRTUAL_SENSOR_GYRO_6AXES_MASK)) {
            ALOGV_IF(ENG_VERBOSE, "HAL: 9 axis or game rot enabled");
            if (!checkLPQuaternion()) {
                enableLPQuaternion(1);
            } else {
                ALOGV_IF(ENG_VERBOSE, "HAL:LP Quat already enabled");
            }
        } else if (checkLPQuaternion()) {
            enableLPQuaternion(0);
        }
    }

    /* apply accel/gyro bias to DMP bias                        */
    /* precondition: masterEnable(0), mGyroBiasAvailable=true   */
    /* postcondition: bias is applied upon masterEnable(1)      */
    if(!(sensors & INV_THREE_AXIS_GYRO)) {
        setGyroBias();
    }
    if(!(sensors & INV_THREE_AXIS_ACCEL)) {
        setAccelBias();
    }

    /* to batch or not to batch */
    int batchMode = computeBatchSensorMask(mEnabled, mBatchEnabled);
    /* skip setBatch if there is no need to */
    if(((int)mOldBatchEnabledMask != batchMode) || batchMode) {
        setBatch(batchMode, 0);
    }
    mOldBatchEnabledMask = batchMode;

    /* check for invn hardware sensors change */
    if (changed & ((1 << Gyro) | (1 << RawGyro) | (1 << Accelerometer) |
                   (1 << MagneticField) | (1 << RawMagneticField))) {
        ALOGV_IF(ENG_VERBOSE,
                 "HAL DEBUG: Gyro, Accel, Compass, Pressure changes");
        if ((checkSmdSupport() == 1) || (checkPedometerSupport() == 1) || (sensors &
            (INV_THREE_AXIS_GYRO
                | INV_THREE_AXIS_ACCEL
                | INV_THREE_AXIS_COMPASS))) {
            ALOGV_IF(ENG_VERBOSE, "SMD or Hardware sensors enabled");
            ALOGV_IF(ENG_VERBOSE,
                     "mFeatureActiveMask=0x%llx", mFeatureActiveMask);
                ALOGV_IF(ENG_VERBOSE, "HAL DEBUG: LPQ, SMD, SO enabled");
                // disable DMP event interrupt only (w/ data interrupt)
                ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                         0, mpu.dmp_event_int_on, getTimestamp());
                if (write_sysfs_int(mpu.dmp_event_int_on, 0) < 0) {
                    res = -1;
                    ALOGE("HAL:ERR can't disable DMP event interrupt");
                    return res;
                }
            ALOGV_IF(ENG_VERBOSE, "mFeatureActiveMask=%lld", mFeatureActiveMask);
            ALOGV_IF(ENG_VERBOSE, "DMP_FEATURE_MASK=%d", DMP_FEATURE_MASK);
        if ((mFeatureActiveMask & (long long)DMP_FEATURE_MASK) &&
                    !((mFeatureActiveMask & INV_DMP_6AXIS_QUATERNION) ||
                     (mFeatureActiveMask & INV_DMP_PED_STANDALONE) ||
                     (mFeatureActiveMask & INV_DMP_PED_QUATERNION) ||
                     (mFeatureActiveMask & INV_DMP_BATCH_MODE))) {
                // enable DMP
                onDmp(1);
                res = enableAccel(on);
                if(res < 0) {
                    return res;
                }
                ALOGV_IF(ENG_VERBOSE, "mLocalSensorMask=0x%lx", mLocalSensorMask);
                if (((sensors | mLocalSensorMask) & INV_THREE_AXIS_ACCEL) == 0) {
                    res = turnOffAccelFifo();
                }
                if(res < 0) {
                    return res;
                }
            }
        } else { // all sensors idle
            ALOGV_IF(ENG_VERBOSE, "HAL DEBUG: not SMD or Hardware sensors");
            if (!cal_stored) {
                storeCalibration();
                cal_stored = 1;
            }
        }
    } else if (//(changed &
//                    ((!mCompassSensor->isIntegrated()) << MagneticField) ||
//                    ((!mCompassSensor->isIntegrated()) << RawMagneticField))
//                    &&
              !(sensors & (INV_THREE_AXIS_GYRO | INV_THREE_AXIS_ACCEL
                | INV_THREE_AXIS_COMPASS))
    ) {
        ALOGV_IF(ENG_VERBOSE, "HAL DEBUG: Gyro, Accel, Compass no change");
        if (!cal_stored) {
            storeCalibration();
            cal_stored = 1;
        }
    } /*else {
      ALOGV_IF(ENG_VERBOSE, "HAL DEBUG: mEnabled");
    }*/

    if (!batchMode && (resetDataRates() < 0)) {
        ALOGE("HAL:ERR can't reset output rate back to original setting");
    }
    return res;
}

/* check if batch mode should be turned on or not */
int MPLSensor::computeBatchSensorMask(int enableSensors, int tempBatchSensor)
{
    VFUNC_LOG;
    int batchMode = 1;
    mFeatureActiveMask &= ~INV_DMP_BATCH_MODE;

    ALOGV_IF(ENG_VERBOSE,
             "HAL:computeBatchSensorMask: enableSensors=%d tempBatchSensor=%d",
             enableSensors, tempBatchSensor);

    // handle initialization case
    if (enableSensors == 0 && tempBatchSensor == 0)
        return 0;

    // check for possible continuous data mode
    for(int i = 0; i <= Pressure; i++) {
        // if any one of the hardware sensor is in continuous data mode
        // turn off batch mode.
        if ((enableSensors & (1 << i)) && !(tempBatchSensor & (1 << i))) {
            ALOGV_IF(ENG_VERBOSE, "HAL:computeBatchSensorMask: "
                     "hardware sensor on continuous mode:%d", i);
            return 0;
        }
        if ((enableSensors & (1 << i)) && (tempBatchSensor & (1 << i))) {
            ALOGV_IF(ENG_VERBOSE,
                     "HAL:computeBatchSensorMask: hardware sensor is batch:%d",
                    i);
            // if hardware sensor is batched, check if virtual sensor is also batched
            if ((enableSensors & (1 << GameRotationVector))
                            && !(tempBatchSensor & (1 << GameRotationVector))) {
            ALOGV_IF(ENG_VERBOSE,
                     "HAL:computeBatchSensorMask: but virtual sensor is not:%d",
                    i);
                return 0;
            }
        }
    }

    // if virtual sensors are on but not batched, turn off batch mode.
    for(int i = Orientation; i <= GeomagneticRotationVector; i++) {
        if ((enableSensors & (1 << i)) && !(tempBatchSensor & (1 << i))) {
             ALOGV_IF(ENG_VERBOSE, "HAL:computeBatchSensorMask: "
                      "composite sensor on continuous mode:%d", i);
            return 0;
        }
    }

    if ((mFeatureActiveMask & INV_DMP_PEDOMETER) && !(tempBatchSensor & (1 << StepDetector))) {
        ALOGV("HAL:computeBatchSensorMask: step detector on continuous mode.");
        return 0;
    }

    mFeatureActiveMask |= INV_DMP_BATCH_MODE;
    ALOGV_IF(EXTRA_VERBOSE,
             "HAL:computeBatchSensorMask: batchMode=%d, mBatchEnabled=%0x",
             batchMode, tempBatchSensor);
    return (batchMode && tempBatchSensor);
}

/* This function is called by enable() */
int MPLSensor::setBatch(int en, int toggleEnable)
{
    VFUNC_LOG;

    int res = 0;
    int64_t wanted = 1000000000LL;
    int64_t timeout = 0;
    int timeoutInMs = 0;
    int featureMask = computeBatchDataOutput();

    if (en) {
        PROCESS_VERBOSE = true;
        EXTRA_VERBOSE = true;
        SYSFS_VERBOSE = true;
        FUNC_ENTRY = true;
        HANDLER_ENTRY = true;
        ENG_VERBOSE = true;
        INPUT_DATA = true;
        HANDLER_DATA = true;
        DEBUG_BATCHING = true;

        /* take the minimum batchmode timeout */
        int64_t timeout = 100000000000LL;
        int64_t ns;
        for (int i = 0; i < MpuNumSensors; i++) {
            ALOGV_IF(0, "mFeatureActiveMask=0x%016llx, mEnabled=0x%01x, mBatchEnabled=0x%x",
                     mFeatureActiveMask, mEnabled, mBatchEnabled);
            if (((mEnabled & (1 << i)) && (mBatchEnabled & (1 << i))) ||
                    (((featureMask & INV_DMP_PED_STANDALONE) && (mBatchEnabled & (1 << StepDetector))))) {
                ALOGV_IF(ENG_VERBOSE, "sensor=%d, timeout=%lld", i, mBatchTimeouts[i]);
                ns = mBatchTimeouts[i];
                timeout = (ns < timeout) ? ns : timeout;
            }
        }
        /* Convert ns to millisecond */
        timeoutInMs = timeout / 1000000;
    } else {
        PROCESS_VERBOSE = false;
        EXTRA_VERBOSE = false;
        SYSFS_VERBOSE = false;
        FUNC_ENTRY = false;
        HANDLER_ENTRY = false;
        ENG_VERBOSE = false;
        INPUT_DATA = false;
        HANDLER_DATA = false;
        DEBUG_BATCHING = false;

        timeoutInMs = 0;
    }

    ALOGV_IF(ENG_VERBOSE, "HAL: batch timeout set to %dms", timeoutInMs);

    /* step detector is enabled and */
    /* batch mode is standalone */
    if (en && (mFeatureActiveMask & INV_DMP_PEDOMETER) &&
            (featureMask & INV_DMP_PED_STANDALONE)) {
        ALOGV_IF(ENG_VERBOSE, "ID_SD only = 0x%x", mBatchEnabled);
        enablePedStandalone(1);
    } else {
        enablePedStandalone(0);
    }

    /* step detector and GRV are enabled and */
    /* batch mode is ped q */
    if (en && (mFeatureActiveMask & INV_DMP_PEDOMETER) &&
            (mEnabled & (1 << GameRotationVector)) &&
            (featureMask & INV_DMP_PED_QUATERNION)) {
        ALOGV_IF(ENG_VERBOSE, "ID_SD and GRV or ALL = 0x%x", mBatchEnabled);
        ALOGV_IF(ENG_VERBOSE, "ID_SD is enabled for batching, "
                "PED quat will be automatically enabled");
        enableLPQuaternion(0);
        enablePedQuaternion(1);
    } else if (!(featureMask & INV_DMP_PED_STANDALONE)){
        enablePedQuaternion(0);
    } else {
        enablePedQuaternion(0);
    }

    /* step detector and hardware sensors enabled */
    if (en && (featureMask & INV_DMP_PED_INDICATOR) &&
            ((mEnabled) ||
            (mFeatureActiveMask & INV_DMP_PED_STANDALONE))) {
        enablePedIndicator(1);
    } else {
        enablePedIndicator(0);
    }

    /* GRV is enabled and */
    /* batch mode is 6axis q */
    if (en && (mEnabled & (1 << GameRotationVector)) &&
            (featureMask & INV_DMP_6AXIS_QUATERNION)) {
        ALOGV_IF(ENG_VERBOSE, "GRV = 0x%x", mBatchEnabled);
        enableLPQuaternion(0);
        enable6AxisQuaternion(1);
        setInitial6QuatValue();
    } else if (!(featureMask & INV_DMP_PED_QUATERNION)){
        ALOGV_IF(ENG_VERBOSE, "Toggle back to normal 6 axis");
        if (mEnabled & (1 << GameRotationVector)) {
            enableLPQuaternion(checkLPQRateSupported());
        }
        enable6AxisQuaternion(0);
    } else {
        enable6AxisQuaternion(0);
    }

    /* write required timeout to sysfs */
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
             timeoutInMs, mpu.batchmode_timeout, getTimestamp());
    if (write_sysfs_int(mpu.batchmode_timeout, timeoutInMs) < 0) {
        ALOGE("HAL:ERR can't write batchmode_timeout");
    }

    if (en) {
        // enable DMP
        res = onDmp(1);
        if (res < 0) {
            return res;
        }

        // set batch rates
        if (setBatchDataRates() < 0) {
            ALOGE("HAL:ERR can't set batch data rates");
        }

        // default fifo rate to 200Hz
        ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                 200, mpu.gyro_fifo_rate, getTimestamp());
        if (write_sysfs_int(mpu.gyro_fifo_rate, 200) < 0) {
            res = -1;
            ALOGE("HAL:ERR can't set rate to 200Hz");
            return res;
        }
    } else {
        if (mFeatureActiveMask == 0) {
            // disable DMP
            res = onDmp(0);
            if (res < 0) {
                return res;
            }
            /* reset sensor rate */
            if (resetDataRates() < 0) {
                ALOGE("HAL:ERR can't reset output rate back to original setting");
            }
        }
    }

    // set sensor data interrupt
    uint32_t dataInterrupt = (mEnabled || (mFeatureActiveMask & INV_DMP_BATCH_MODE));
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
             !dataInterrupt, mpu.dmp_event_int_on, getTimestamp());
    if (write_sysfs_int(mpu.dmp_event_int_on, !dataInterrupt) < 0) {
        res = -1;
        ALOGE("HAL:ERR can't enable DMP event interrupt");
    }
    return res;
}

/* Store calibration file */
void MPLSensor::storeCalibration(void)
{
    VFUNC_LOG;

    if(mHaveGoodMpuCal == true
        || mAccelAccuracy >= 2
        || mCompassAccuracy >= 3) {
       int res = inv_store_calibration();
       if (res) {
           ALOGE("HAL:Cannot store calibration on file");
       } else {
           ALOGV_IF(PROCESS_VERBOSE, "HAL:Cal file updated");
       }
    }
}

/*  these handlers transform mpl data into one of the Android sensor types */
int MPLSensor::gyroHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int update;
    inv_time_t timestamp;
    update = inv_get_sensor_type_gyroscope(s->gyro.v, &s->gyro.status,
                                           &timestamp);
    s->timestamp = timestamp;
    ALOGV_IF(HANDLER_DATA, "HAL:gyro data : %+f %+f %+f -- %lld - %d",
             s->gyro.v[0], s->gyro.v[1], s->gyro.v[2], s->timestamp, update);
    return update;
}

int MPLSensor::rawGyroHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int update;
    inv_time_t timestamp;
    update = inv_get_sensor_type_gyroscope_raw(s->uncalibrated_gyro.uncalib,
                                               &s->gyro.status, &timestamp);
    s->timestamp = timestamp;
    if(update) {
        memcpy(s->uncalibrated_gyro.bias, mGyroBias, sizeof(mGyroBias));
        ALOGV_IF(HANDLER_DATA,"HAL:gyro bias data : %+f %+f %+f -- %lld - %d",
                 s->uncalibrated_gyro.bias[0], s->uncalibrated_gyro.bias[1],
                 s->uncalibrated_gyro.bias[2], s->timestamp, update);
    }
    s->gyro.status = SENSOR_STATUS_UNRELIABLE;
    ALOGV_IF(HANDLER_DATA, "HAL:raw gyro data : %+f %+f %+f -- %lld - %d",
             s->uncalibrated_gyro.uncalib[0], s->uncalibrated_gyro.uncalib[1],
             s->uncalibrated_gyro.uncalib[2], s->timestamp, update);
    return update;
}

int MPLSensor::accelHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int update;
    inv_time_t timestamp;
    update = inv_get_sensor_type_accelerometer(
        s->acceleration.v, &s->acceleration.status, &timestamp);
    s->timestamp = timestamp;
    ALOGV_IF(HANDLER_DATA, "HAL:accel data : %+f %+f %+f -- %lld - %d",
             s->acceleration.v[0], s->acceleration.v[1], s->acceleration.v[2],
             s->timestamp, update);
    mAccelAccuracy = s->acceleration.status;
    return update;
}

int MPLSensor::compassHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int update;
    inv_time_t timestamp;

    update = inv_get_sensor_type_magnetic_field(s->magnetic.v,
                                                &s->magnetic.status,
                                                &timestamp);
    s->timestamp = timestamp;
    ALOGV_IF(HANDLER_DATA, "HAL:compass data: %+f %+f %+f -- %lld - %d",
             s->magnetic.v[0], s->magnetic.v[1], s->magnetic.v[2],
             s->timestamp, update);
    mCompassAccuracy = s->magnetic.status;
    return update;
}

int MPLSensor::rawCompassHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int update;
    inv_time_t timestamp;

    if (mCompassSensor == NULL)
        return 0;

    //TODO: need to handle uncalib data and bias for 3rd party compass
//    if(mCompassSensor->providesCalibration()) {
//        update = mCompassSensor->readRawSample(s->uncalibrated_magnetic.uncalib,
//                                               &s->timestamp);
//    } else {
        update = inv_get_sensor_type_magnetic_field_raw(s->uncalibrated_magnetic.uncalib,
                                                        &s->magnetic.status,
                                                        &timestamp);
//    }
    s->timestamp = timestamp;
    if(update) {
        memcpy(s->uncalibrated_magnetic.bias, mCompassBias, sizeof(mCompassBias));
        ALOGV_IF(HANDLER_DATA, "HAL:compass bias data: %+f %+f %+f -- %lld - %d",
                 s->uncalibrated_magnetic.bias[0], s->uncalibrated_magnetic.bias[1],
                 s->uncalibrated_magnetic.bias[2], s->timestamp, update);
    }
    s->magnetic.status = SENSOR_STATUS_UNRELIABLE;
    ALOGV_IF(HANDLER_DATA, "HAL:compass raw data: %+f %+f %+f %d -- %lld - %d",
             s->uncalibrated_magnetic.uncalib[0],
             s->uncalibrated_magnetic.uncalib[1],
             s->uncalibrated_magnetic.uncalib[2],
             s->magnetic.status, s->timestamp, update);
    return update;
}

/*
    Rotation Vector handler.
    NOTE: rotation vector does not have an accuracy or status
*/
int MPLSensor::rvHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int8_t status;
    int update;
    inv_time_t timestamp;
    update = inv_get_sensor_type_rotation_vector(s->data, &status,
                                                 &timestamp);
    s->timestamp = timestamp;
    update |= isCompassDisabled();
    ALOGV_IF(HANDLER_DATA, "HAL:rv data: %+f %+f %+f %+f %+f- %+lld - %d",
             s->data[0], s->data[1], s->data[2], s->data[3], s->data[4],
             s->timestamp, update);

    return update;
}

/*
    Game Rotation Vector handler.
    NOTE: rotation vector does not have an accuracy or status
*/
int MPLSensor::grvHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int8_t status;
    int update;
    inv_time_t timestamp;
    update = inv_get_sensor_type_rotation_vector_6_axis(s->data, &status,
                                                        &timestamp);
    s->timestamp = timestamp;
    ALOGV_IF(HANDLER_DATA, "HAL:grv data: %+f %+f %+f %+f %+f - %+lld - %d",
             s->data[0], s->data[1], s->data[2], s->data[3], s->data[4],
             s->timestamp, update);
    return update;
}

int MPLSensor::laHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int update;
    inv_time_t timestamp;
    update = inv_get_sensor_type_linear_acceleration(
            s->gyro.v, &s->gyro.status, &timestamp);
    s->timestamp = timestamp;
    update |= isCompassDisabled();
    ALOGV_IF(HANDLER_DATA, "HAL:la data: %+f %+f %+f - %lld - %d",
             s->gyro.v[0], s->gyro.v[1], s->gyro.v[2], s->timestamp, update);
    return update;
}

int MPLSensor::gravHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int update;
    inv_time_t timestamp;
    update = inv_get_sensor_type_gravity(s->gyro.v, &s->gyro.status,
                                         &timestamp);
    s->timestamp = timestamp;
    update |= isCompassDisabled();
    ALOGV_IF(HANDLER_DATA, "HAL:gr data: %+f %+f %+f - %lld - %d",
             s->gyro.v[0], s->gyro.v[1], s->gyro.v[2], s->timestamp, update);
    return update;
}

int MPLSensor::orienHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int update;
    inv_time_t timestamp;
    update = inv_get_sensor_type_orientation(
            s->orientation.v, &s->orientation.status, &timestamp);
    s->timestamp = timestamp;
    update |= isCompassDisabled();
    ALOGV_IF(HANDLER_DATA, "HAL:or data: %f %f %f - %lld - %d",
             s->orientation.v[0], s->orientation.v[1], s->orientation.v[2],
             s->timestamp, update);
    return update;
}

int MPLSensor::smHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int update = 1;

    /* When event is triggered, set data to 1 */
    s->data[0] = 1.f;
    s->data[1] = 0.f;
    s->data[2] = 0.f;

    /* Capture timestamp in HAL */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    s->timestamp = (int64_t) ts.tv_sec * 1000000000 + ts.tv_nsec;

    ALOGV_IF(HANDLER_DATA, "HAL:sm data: %f - %lld - %d",
             s->data[0], s->timestamp, update);
    return update;
}

int MPLSensor::gmHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int8_t status;
    int update = 0;
    inv_time_t timestamp;
    update = inv_get_sensor_type_geomagnetic_rotation_vector(s->data, &status,
                                                             &timestamp);
    s->timestamp = timestamp;

    ALOGV_IF(HANDLER_DATA, "HAL:gm data: %+f %+f %+f %+f %+f- %+lld - %d",
             s->data[0], s->data[1], s->data[2], s->data[3], s->data[4], s->timestamp, update);
    return update < 1 ? 0 :1;

}

int MPLSensor::psHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int8_t status;
    int update = 0;

    s->pressure = mCachedPressureData / 100.f; //hpa (millibar)
    s->data[1] = 0;
    s->data[2] = 0;
    s->timestamp = mPressureTimestamp;
    s->magnetic.status = 0;
    update = mPressureUpdate;
    mPressureUpdate = 0;

    ALOGV_IF(HANDLER_DATA, "HAL:ps data: %+f %+f %+f %+f- %+lld - %d",
             s->data[0], s->data[1], s->data[2], s->data[3], s->timestamp, update);
    return update < 1 ? 0 :1;

}

int MPLSensor::sdHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int update = 1;

    /* When event is triggered, set data to 1 */
    s->data[0] = 1;
    s->data[1] = 0.f;
    s->data[2] = 0.f;

    /* get current timestamp */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts) ;
    s->timestamp = (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;

    ALOGV_IF(HANDLER_DATA, "HAL:sd data: %f - %lld - %d",
             s->data[0], s->timestamp, update);
    return update;
}

int MPLSensor::scHandler(sensors_event_t* s)
{
    VHANDLER_LOG;
    int update = 1;

    /* Set step count */
#if 1
    s->u64.step_counter = mLastStepCount;
    ALOGV_IF(HANDLER_DATA, "HAL:sc data: %lld - %lld - %d",
             s->u64.step_counter, s->timestamp, update);
#else
    s->step_counter = mLastStepCount;
    LOGV_IF(HANDLER_DATA, "HAL:sc data: %lld - %lld - %d",
            s->step_counter, s->timestamp, update);
#endif
    return update;
}

int MPLSensor::enable(int32_t handle, int en, int channel)
{
    VFUNC_LOG;

    android::String8 sname;
    int what = -1, err = 0;
    int batchMode = 0;

    ALOGV_IF(ENG_VERBOSE, "HAL:handle = %d", handle);
    getHandle(handle, what, sname);
    if (uint32_t(what) >= MpuNumSensors)
        return -EINVAL;

    switch (what) {
    case StepCounter:
        if (!en)
            mBatchEnabled &= ~(1 << what);
        ALOGV_IF(PROCESS_VERBOSE, "HAL:enable - sensor %s (handle %d) %s -> %s",
                 sname.string(), what,
                 (mDmpStepCountEnabled? "en": "dis"),
                 (en? "en" : "dis"));
        enableDmpPedometer(en, 0);
        mDmpStepCountEnabled = !!en;
        return 0;

    case StepDetector:
        if (!en)
            mBatchEnabled &= ~(1 << what);
        ALOGV_IF(PROCESS_VERBOSE, "HAL:enable - sensor %s (handle %d) %s -> %s",
                 sname.string(), what,
                 (mDmpPedometerEnabled? "en": "dis"),
                 (en? "en" : "dis"));
        enableDmpPedometer(en, 1);
        mDmpPedometerEnabled = !!en;
        batchMode = computeBatchSensorMask(mEnabled, mBatchEnabled);
        /* skip setBatch if there is no need to */
        if(((int)mOldBatchEnabledMask != batchMode) || batchMode) {
            setBatch(batchMode, 1);
        }
        mOldBatchEnabledMask = batchMode;
        return 0;

    case SignificantMotion:
        if (!en)
            mBatchEnabled &= ~(1 << what);
        ALOGV_IF(PROCESS_VERBOSE, "HAL:enable - sensor %s (handle %d) %s -> %s",
                 sname.string(), what,
                 (mDmpSignificantMotionEnabled? "en": "dis"),
                 (en? "en" : "dis"));
        enableDmpSignificantMotion(en);
        mDmpSignificantMotionEnabled = !!en;
        return 0;

    default:
        break;
    }

    if (!en)
        mBatchEnabled &= ~(1 << what);
    int newState = en ? 1 : 0;
    unsigned long sen_mask;

//    ALOGV_IF(PROCESS_VERBOSE, "HAL:enable - sensor %s (handle %d) %s -> %s",
    ALOGV_IF(1, "HAL:enable - sensor %s (handle %d) %s -> %s",
             sname.string(), handle,
             ((mEnabled & (1 << what)) ? "en" : "dis"),
             ((uint32_t(newState) << what) ? "en" : "dis"));
    ALOGV_IF(ENG_VERBOSE,
             "HAL:%s sensor state change what=%d", sname.string(), what);

    // pthread_mutex_lock(&mMplMutex);
    // pthread_mutex_lock(&mHALMutex);

    if ((uint32_t(newState) << what) != (mEnabled & (1 << what))) {
        uint32_t sensor_type;
        short flags = newState;
        uint32_t lastEnabled = mEnabled, changed = 0;

        mEnabled &= ~(1 << what);
        mEnabled |= (uint32_t(flags) << what);

        ALOGV_IF(ENG_VERBOSE, "HAL:flags = %d", flags);
        computeLocalSensorMask(mEnabled);
        ALOGV_IF(ENG_VERBOSE, "HAL:enable : mEnabled = %d", mEnabled);
        ALOGV_IF(ENG_VERBOSE, "HAL:last enable : lastEnabled = %d", lastEnabled);
        sen_mask = mLocalSensorMask & mMasterSensorMask;
        mSensorMask = sen_mask;
        ALOGV_IF(ENG_VERBOSE, "HAL:sen_mask= 0x%0lx", sen_mask);

        switch (what) {
            case Gyro:
            case RawGyro:
            case Accelerometer:
                if ((!(mEnabled & VIRTUAL_SENSOR_GYRO_6AXES_MASK) &&
                    !(mEnabled & VIRTUAL_SENSOR_9AXES_MASK)) &&
                    ((lastEnabled & (1 << what)) != (mEnabled & (1 << what)))) {
                    changed |= (1 << what);
                }
                if (mFeatureActiveMask & INV_DMP_6AXIS_QUATERNION) {
                     changed |= (1 << what);
                }
                break;
            case MagneticField:
            case RawMagneticField:
                if (!(mEnabled & VIRTUAL_SENSOR_9AXES_MASK) &&
                    ((lastEnabled & (1 << what)) != (mEnabled & (1 << what)))) {
                    changed |= (1 << what);
                }
                break;
            case Pressure:
                if ((lastEnabled & (1 << what)) != (mEnabled & (1 << what))) {
                    changed |= (1 << what);
                }
                break;
            case GameRotationVector:
                if (!en)
                    storeCalibration();
                if ((en && !(lastEnabled & VIRTUAL_SENSOR_ALL_MASK))
                         ||
                    (en && !(lastEnabled & VIRTUAL_SENSOR_9AXES_MASK))
                         ||
                    (!en && !(mEnabled & VIRTUAL_SENSOR_ALL_MASK))
                         ||
                    (!en && (mEnabled & VIRTUAL_SENSOR_MAG_6AXES_MASK))) {
                    for (int i = Gyro; i <= RawMagneticField; i++) {
                        if (!(mEnabled & (1 << i))) {
                            changed |= (1 << i);
                        }
                    }
                }
                break;

            case Orientation:
            case RotationVector:
            case LinearAccel:
            case Gravity:
                if (!en)
                    storeCalibration();
                if ((en && !(lastEnabled & VIRTUAL_SENSOR_9AXES_MASK))
                         ||
                    (!en && !(mEnabled & VIRTUAL_SENSOR_9AXES_MASK))) {
                    for (int i = Gyro; i <= RawMagneticField; i++) {
                        if (!(mEnabled & (1 << i))) {
                            changed |= (1 << i);
                        }
                    }
                }
                break;
            case GeomagneticRotationVector:
                if (!en)
                    storeCalibration();
                if ((en && !(lastEnabled & VIRTUAL_SENSOR_ALL_MASK))
                        ||
                    (en && !(lastEnabled & VIRTUAL_SENSOR_9AXES_MASK))
                         ||
                   (!en && !(mEnabled & VIRTUAL_SENSOR_ALL_MASK))
                         ||
                   (!en && (mEnabled & VIRTUAL_SENSOR_GYRO_6AXES_MASK))) {
                   for (int i = Accelerometer; i <= RawMagneticField; i++) {
                       if (!(mEnabled & (1<<i))) {
                          changed |= (1 << i);
                       }
                   }
                }
                break;
        }
        ALOGV_IF(ENG_VERBOSE, "HAL:changed = %d", changed);
        enableSensors(sen_mask, flags, changed);
    }

    // pthread_mutex_unlock(&mMplMutex);
    // pthread_mutex_unlock(&mHALMutex);

#ifdef INV_PLAYBACK_DBG
    /* apparently the logging needs to go through this sequence
       to properly flush the log file */
    inv_turn_off_data_logging();
    if (fclose(logfile) < 0) {
        ALOGE("cannot close debug log file");
    }
    logfile = fopen("/data/playback.bin", "ab");
    if (logfile)
        inv_turn_on_data_logging(logfile);
#endif

    return err;
}

void MPLSensor::getHandle(int32_t handle, int &what, android::String8 &sname)
{
    VFUNC_LOG;
    unsigned int i;
    what = -1;

    for (i = 0; i < MpuNumSensors; i++) {
        if (mPendingEvents[i].sensor == handle)
            break;
    }
    if (i >= MpuNumSensors) {
        sname = "Others";
        return;
    }

    switch (mPendingEvents[i].type) {
    case SENSOR_TYPE_STEP_DETECTOR:
        what = StepDetector;
        sname = "StepDetector";
        break;

    case SENSOR_TYPE_STEP_COUNTER:
        what = StepCounter;
        sname = "StepCounter";
        break;

    case SENSOR_TYPE_SIGNIFICANT_MOTION:
        what = SignificantMotion;
        sname = "SignificantMotion";
        break;

    case SENSOR_TYPE_ACCELEROMETER:
        what = Accelerometer;
        sname = "Accelerometer";
        break;

    case SENSOR_TYPE_MAGNETIC_FIELD:
        what = MagneticField;
        sname = "MagneticField";
        break;

    case SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED:
        what = RawMagneticField;
        sname = "MagneticField Uncalibrated";
        break;

    case SENSOR_TYPE_ORIENTATION:
        what = Orientation;
        sname = "Orientation";
        break;

    case SENSOR_TYPE_GYROSCOPE:
        what = Gyro;
        sname = "Gyro";
        break;

    case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:
        what = RawGyro;
        sname = "Gyro Uncalibrated";
        break;

    case SENSOR_TYPE_GRAVITY:
        what = Gravity;
        sname = "Gravity";
        break;

    case SENSOR_TYPE_ROTATION_VECTOR:
        what = RotationVector;
        sname = "RotationVector";
        break;

    case SENSOR_TYPE_GAME_ROTATION_VECTOR:
        what = GameRotationVector;
        sname = "GameRotationVector";
        break;

    case SENSOR_TYPE_LINEAR_ACCELERATION:
        what = LinearAccel;
        sname = "LinearAccel";
        break;

    case SENSOR_TYPE_PRESSURE:
        what = Pressure;
        sname = "Pressure";
        break;

    case SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR:
        what = GeomagneticRotationVector;
        sname = "Geomagnetic RV";
        break;

   default: // this takes care of all the gestures
        what = -1;
        sname = "ERROR";
        break;
    }

    ALOGI_IF(EXTRA_VERBOSE, "HAL:getHandle - what=%d, sname=%s", what, sname.string());
    return;
}

int MPLSensor::setDelay(int32_t handle, int64_t ns, int channel)
{
    VFUNC_LOG;

    android::String8 sname;
    int what = -1;

    getHandle(handle, what, sname);
    if (uint32_t(what) >= MpuNumSensors)
        return -EINVAL;

    if (ns < 0)
        return -EINVAL;

    ALOGV_IF(PROCESS_VERBOSE,
             "setDelay : %llu ns, (%.2f Hz)", ns, 1000000000.f / ns);

    // limit all rates to reasonable ones */
    if (ns < 5000000LL) {
        ns = 5000000LL;
    }

    /* store request rate to mDelays arrary for each sensor */
    int64_t previousDelay = mDelays[what];
    mDelays[what] = ns;
    ALOGV_IF(ENG_VERBOSE, "storing mDelays[%d] = %lld, previousDelay = %lld", what, ns, previousDelay);

    switch (what) {
        case StepCounter:
            /* set limits of delivery rate of events */
            mStepCountPollTime = ns;
            ALOGV_IF(ENG_VERBOSE, "step count rate =%lld ns", ns);
            break;
        case StepDetector:
        case SignificantMotion:
            ALOGV_IF(ENG_VERBOSE, "Step Detect, SMD, SO rate=%lld ns", ns);
            break;
        case Gyro:
        case RawGyro:
        case Accelerometer:
            // need to update delay since they are different
            // resetDataRates was called earlier
            // ALOGV("what=%d mEnabled=%d mDelays[%d]=%lld previousDelay=%lld",
            //       what, mEnabled, what, mDelays[what], previousDelay);
            if ((mEnabled & (1 << what)) && (previousDelay != mDelays[what])) {
                ALOGV_IF(ENG_VERBOSE,
                         "HAL:need to update delay due to resetDataRates");
                break;
            }
            for (int i = Gyro;
                    i <= Accelerometer + 1; //mCompassSensor->isIntegrated();
                    i++) {
                if (i != what && (mEnabled & (1 << i)) && ns > mDelays[i]) {
                    ALOGV_IF(ENG_VERBOSE,
                             "HAL:ignore delay set due to sensor %d", i);
                    return 0;
                }
            }
            break;

        case MagneticField:
        case RawMagneticField:
            // need to update delay since they are different
            // resetDataRates was called earlier
            if ((mEnabled & (1 << what)) && (previousDelay != mDelays[what])) {
                ALOGV_IF(ENG_VERBOSE,
                         "HAL:need to update delay due to resetDataRates");
                break;
            }
            if ((((mEnabled & (1 << Gyro)) && ns > mDelays[Gyro]) ||
                    ((mEnabled & (1 << RawGyro)) && ns > mDelays[RawGyro]) ||
                    ((mEnabled & (1 << Accelerometer)) &&
                        ns > mDelays[Accelerometer])) &&
                        !checkBatchEnabled()) {
                 /* if request is slower rate, ignore request */
                 ALOGV_IF(ENG_VERBOSE,
                          "HAL:ignore delay set due to gyro/accel");
                 return 0;
            }
            break;

        case Orientation:
        case RotationVector:
        case GameRotationVector:
        case GeomagneticRotationVector:
        case LinearAccel:
        case Gravity:
            if (isLowPowerQuatEnabled()) {
                ALOGV_IF(ENG_VERBOSE,
                         "HAL:need to update delay due to LPQ");
                break;
            }

            for (int i = 0; i < MpuNumSensors; i++) {
                if (i != what && (mEnabled & (1 << i)) && ns > mDelays[i]) {
                    ALOGV_IF(ENG_VERBOSE,
                             "HAL:ignore delay set due to sensor %d", i);
                    return 0;
                }
            }
            break;
    }

    // pthread_mutex_lock(&mHALMutex);
    int res = update_delay();
    // pthread_mutex_unlock(&mHALMutex);
    return res;
}

int MPLSensor::update_delay(void)
{
    VFUNC_LOG;

    int res = 0;
    int64_t got;

    if (mEnabled) {
        int64_t wanted = 1000000000LL;
        int64_t wanted_3rd_party_sensor = 1000000000LL;

        // Sequence to change sensor's FIFO rate
        // 1. reset master enable
        // 2. Update delay
        // 3. set master enable

        int64_t gyroRate;
        int64_t accelRate;
        int64_t compassRate;
        int64_t pressureRate;

        int rateInus;
        int mplGyroRate;
        int mplAccelRate;
        int mplCompassRate;
        int tempRate = wanted;

        /* search the minimum delay requested across all enabled sensors */
        for (int i = 0; i < MpuNumSensors; i++) {
            if (mEnabled & (1 << i)) {
                int64_t ns = mDelays[i];
                wanted = wanted < ns ? wanted : ns;
            }
        }

        /* initialize rate for each sensor */
        gyroRate = wanted;
        accelRate = wanted;
        compassRate = wanted;
        pressureRate = wanted;
        // same delay for 3rd party Accel or Compass
        wanted_3rd_party_sensor = wanted;

        int enabled_sensors = mEnabled;
        int tempFd = -1;

        if(mFeatureActiveMask & INV_DMP_BATCH_MODE) {
            // set batch rates
            ALOGV_IF(ENG_VERBOSE, "HAL: mFeatureActiveMask=%016llx", mFeatureActiveMask);
            ALOGV("HAL: batch mode is set, set batch data rates");
            if (setBatchDataRates() < 0) {
                ALOGE("HAL:ERR can't set batch data rates");
            }
        } else {
            /* set master sampling frequency */
            int64_t tempWanted = wanted;
            getDmpRate(&tempWanted);
            /* driver only looks at sampling frequency if DMP is off */
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %.0f > %s (%lld)",
                     1000000000.f / tempWanted, mpu.gyro_fifo_rate, getTimestamp());
            tempFd = open(mpu.gyro_fifo_rate, O_RDWR);
            res = write_attribute_sensor(tempFd, 1000000000.f / tempWanted);
            ALOGE_IF(res < 0, "HAL:sampling frequency update delay error");

        if (LA_ENABLED || GR_ENABLED || RV_ENABLED
                       || GRV_ENABLED || O_ENABLED || GMRV_ENABLED) {
            rateInus = (int)wanted / 1000LL;

            /* set rate in MPL */
            /* compass can only do 100Hz max */
            inv_set_gyro_sample_rate(rateInus);
            inv_set_accel_sample_rate(rateInus);
            inv_set_compass_sample_rate(rateInus);
            inv_set_linear_acceleration_sample_rate(rateInus);
            inv_set_orientation_sample_rate(rateInus);
            inv_set_rotation_vector_sample_rate(rateInus);
            inv_set_gravity_sample_rate(rateInus);
            inv_set_orientation_geomagnetic_sample_rate(rateInus);
            inv_set_rotation_vector_6_axis_sample_rate(rateInus);
            inv_set_geomagnetic_rotation_vector_sample_rate(rateInus);

            ALOGV_IF(ENG_VERBOSE,
                     "HAL:MPL virtual sensor sample rate: (mpl)=%d us (mpu)=%.2f Hz",
                     rateInus, 1000000000.f / wanted);
            ALOGV_IF(ENG_VERBOSE,
                     "HAL:MPL gyro sample rate: (mpl)=%d us (mpu)=%.2f Hz",
                     rateInus, 1000000000.f / gyroRate);
            ALOGV_IF(ENG_VERBOSE,
                     "HAL:MPL accel sample rate: (mpl)=%d us (mpu)=%.2f Hz",
                     rateInus, 1000000000.f / accelRate);
            ALOGV_IF(ENG_VERBOSE,
                     "HAL:MPL compass sample rate: (mpl)=%d us (mpu)=%.2f Hz",
                     rateInus, 1000000000.f / compassRate);

            ALOGV_IF(ENG_VERBOSE,
                     "mFeatureActiveMask=%016llx", mFeatureActiveMask);
            if(mFeatureActiveMask & DMP_FEATURE_MASK) {
                bool setDMPrate= 0;
                gyroRate = wanted;
                accelRate = wanted;
                compassRate = wanted;
                // same delay for 3rd party Accel or Compass
                wanted_3rd_party_sensor = wanted;
                rateInus = (int)wanted / 1000LL;
                // Set LP Quaternion sample rate if enabled
                if (checkLPQuaternion()) {
                    if (wanted <= RATE_200HZ) {
#ifndef USE_LPQ_AT_FASTEST
                        enableLPQuaternion(0);
#endif
                    } else {
                        inv_set_quat_sample_rate(rateInus);
                        ALOGV_IF(ENG_VERBOSE, "HAL:MPL quat sample rate: "
                                 "(mpl)=%d us (mpu)=%.2f Hz",
                                 rateInus, 1000000000.f / wanted);
                        setDMPrate= 1;
                    }
                }
            }

            ALOGV_IF(EXTRA_VERBOSE, "HAL:setDelay - Fusion");
            //nsToHz
            if (mGyroSensor != NULL) {
                ALOGV_IF(SYSFS_VERBOSE, "%s gyro rate = %.0f (%lld)",
                         __func__, 1000000000.f / gyroRate, getTimestamp());
                res = mGyroSensor->setDelay(0, gyroRate);
                if(res < 0)
                    ALOGE("HAL:GYRO update delay error");
            }
            // mpu accel
            if (mAccelSensor != NULL) {
                ALOGV_IF(SYSFS_VERBOSE, "%s accel rate = %.0f (%lld)",
                         __func__, 1000000000.f / accelRate, getTimestamp());
                res = mAccelSensor->setDelay(0, accelRate);
                if(res < 0)
                    ALOGE("HAL:ACCEL update delay error");
            }

            if (mCompassSensor != NULL) {
//                if (!mCompassSensor->isIntegrated()) {
                    // stand-alone compass - if applicable
                    ALOGV_IF(ENG_VERBOSE,
                            "HAL:Ext compass delay %lld", wanted_3rd_party_sensor);
                    ALOGV_IF(ENG_VERBOSE, "HAL:Ext compass rate %.2f Hz",
                            1000000000.f / wanted_3rd_party_sensor);
//                    if (wanted_3rd_party_sensor <
//                            mCompassSensorList->minDelay * 1000LL) {
//                        wanted_3rd_party_sensor =
//                            mCompassSensorList->minDelay * 1000LL;
//                    }
//                    ALOGV_IF(ENG_VERBOSE,
//                            "HAL:Ext compass delay %lld", wanted_3rd_party_sensor);
//                    ALOGV_IF(ENG_VERBOSE, "HAL:Ext compass rate %.2f Hz",
//                            1000000000.f / wanted_3rd_party_sensor);
//                    mCompassSensor->setDelay(mCompassSensorList->handle, wanted_3rd_party_sensor);
//                    got = mCompassSensor->getDelay(mCompassSensorList->handle);
//                    inv_set_compass_sample_rate(got / 1000);
//                } else {
                    // compass on secondary bus
                    if (compassRate < mCompassSensorList->minDelay * 1000LL)
                        compassRate = mCompassSensorList->minDelay * 1000LL;
                    mCompassSensor->setDelay(mCompassSensorList->handle, compassRate);
//                }
            }
        } else {

            if (GY_ENABLED || RGY_ENABLED) {
                wanted = (mDelays[Gyro] <= mDelays[RawGyro]?
                    (mEnabled & (1 << Gyro)? mDelays[Gyro]: mDelays[RawGyro]):
                    (mEnabled & (1 << RawGyro)? mDelays[RawGyro]: mDelays[Gyro]));
                ALOGV_IF(ENG_VERBOSE, "mFeatureActiveMask=%016llx", mFeatureActiveMask);
                inv_set_gyro_sample_rate((int)wanted/1000LL);
                ALOGV_IF(ENG_VERBOSE,
                         "HAL:MPL gyro sample rate: (mpl)=%d us", int(wanted/1000LL));
                if (mGyroSensor != NULL) {
                    ALOGV_IF(SYSFS_VERBOSE, "%s gyro rate = %.0f (%lld)",
                             __func__, 1000000000.f / wanted, getTimestamp());
                    res = mGyroSensor->setDelay(0, wanted);
                    if(res < 0)
                        ALOGE("HAL:GYRO update delay error");
                }
            }

            if (A_ENABLED) { /* there is only 1 fifo rate for MPUxxxx */
                if (GY_ENABLED && mDelays[Gyro] < mDelays[Accelerometer]) {
                    wanted = mDelays[Gyro];
                } else if (RGY_ENABLED && mDelays[RawGyro]
                            < mDelays[Accelerometer]) {
                    wanted = mDelays[RawGyro];
                } else {
                    wanted = mDelays[Accelerometer];
                }
                ALOGV_IF(ENG_VERBOSE, "mFeatureActiveMask=%016llx", mFeatureActiveMask);
                inv_set_accel_sample_rate((int)wanted/1000LL);
                ALOGV_IF(ENG_VERBOSE, "HAL:MPL accel sample rate: (mpl)=%d us",
                         int(wanted/1000LL));
                if (mAccelSensor != NULL) {
                    ALOGV_IF(SYSFS_VERBOSE, "%s accel rate = %.0f (%lld)",
                             __func__, 1000000000.f / wanted, getTimestamp());
                    res = mAccelSensor->setDelay(0, wanted);
                    if(res < 0)
                        ALOGE("HAL:ACCEL update delay error");
                }
            }

            /* Invensense compass calibration */
            if (M_ENABLED || RM_ENABLED) {
                int64_t compassWanted = (mDelays[MagneticField] <= mDelays[RawMagneticField]?
                    (mEnabled & (1 << MagneticField)? mDelays[MagneticField]: mDelays[RawMagneticField]):
                    (mEnabled & (1 << RawMagneticField)? mDelays[RawMagneticField]: mDelays[MagneticField]));
//                if (!mCompassSensor->isIntegrated()) {
                    wanted = compassWanted;
#if 0 // EML: Compass should be separate regardless if integrated.  Kernel driver handles this.
                } else {
                    if (GY_ENABLED
                        && (mDelays[Gyro] < compassWanted)) {
                        wanted = mDelays[Gyro];
                    } else if (RGY_ENABLED
                               && mDelays[RawGyro] < compassWanted) {
                        wanted = mDelays[RawGyro];
                    } else if (A_ENABLED && mDelays[Accelerometer]
                                < compassWanted) {
                        wanted = mDelays[Accelerometer];
                    } else {
                        wanted = compassWanted;
                    }
                    ALOGV_IF(ENG_VERBOSE, "mFeatureActiveMask=%016llx", mFeatureActiveMask);
                }
#endif // 0
                if (mCompassSensor != NULL) {
                    mCompassSensor->setDelay(mCompassSensorList->handle, wanted);
                    got = mCompassSensor->getDelay(mCompassSensorList->handle);
                    inv_set_compass_sample_rate(got / 1000);
                    ALOGV_IF(ENG_VERBOSE, "HAL:MPL compass sample rate: (mpl)=%d us",
                             int(got/1000LL));
                }
            }

            if (PS_ENABLED) {
                int64_t pressureRate = mDelays[Pressure];
                ALOGV_IF(ENG_VERBOSE, "mFeatureActiveMask=%016llx", mFeatureActiveMask);
                if (mPressureSensor != NULL) {
                    mPressureSensor->setDelay(mPressureSensorList->handle, pressureRate);
                    ALOGE_IF(res < 0, "HAL:PRESSURE update delay error");
                }
            }
        }

        } //end of non batch mode

        unsigned long sensors = mLocalSensorMask & mMasterSensorMask;
        unsigned long mask = (INV_THREE_AXIS_GYRO | INV_THREE_AXIS_ACCEL);
        if (mCompassSensor != NULL)
            mask |= INV_THREE_AXIS_COMPASS;
        if (mPressureSensor != NULL)
            mask |= INV_ONE_AXIS_PRESSURE;
        if (sensors & mask) {
            ALOGV_IF(ENG_VERBOSE, "sensors=%lu", sensors);
        } else { // all sensors idle -> reduce power, unless DMP is needed
            ALOGV_IF(ENG_VERBOSE, "mFeatureActiveMask=%016llx", mFeatureActiveMask);
        }
    }

    return res;
}

/**
 *  Should be called after reading at least one of gyro
 *  compass or accel data. (Also okay for handling all of them).
 *  @returns 0, if successful, error number if not.
 */
int MPLSensor::readEvents(sensors_event_t *data, int count, int32_t handle)
{
    VHANDLER_LOG;
    android::String8 sname;
    int what = -1;
    int n = 0;
    int ret;

    if (mFlush) {
        for (int i = 0; count && mFlush && i <= MpuNumSensors; i++) {
            if (mFlush & (1 << i)) {
                /* if sensor i needs to send a flush event */
                mFlush &= ~(1 << i); /* clear flag */
                mMetaDataEvent.meta_data.sensor = mPendingEvents[i].sensor;
                mMetaDataEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
                *data++ = mMetaDataEvent;
                count--;
                n++;
                ALOGI_IF(SensorBase::INPUT_DATA || SensorBase::EXTRA_VERBOSE,
                         "%s sensors_meta_data_event_t version=%d type=%d\n"
                         "meta_data.what=%d meta_data.sensor=%d\n",
                         __func__,
                         mMetaDataEvent.version,
                         mMetaDataEvent.type,
                         mMetaDataEvent.meta_data.what,
                         mMetaDataEvent.meta_data.sensor);
            }
        }
    }
    if (!count)
        return n;

    if (handle > 0) {
        getHandle(handle, what, sname);
        switch (what) {
        case MagneticField:
        case RawMagneticField:
            ret = buildCompassEvent(data, count);
            if (ret > 0)
                n += ret;
            break;

        case SignificantMotion:
            ret = readDmpSignificantMotionEvents(data, count);
            if (ret > 0)
                n += ret;
            break;

        case StepDetector:
        case StepCounter:
            if (hasStepCountPendingEvents())
                handle = StepCounter;
            else
                handle = StepDetector;
            ret = readDmpPedometerEvents(data, count, handle, 0);
            if (ret > 0)
                n += ret;
            break;

        default:
            ret = buildMpuEvent(data, count);
            if (ret > 0)
                n += ret;
            break;
        }

        if (mFlushPSensor) {
            /* if there was a flush event from a physical sensor */
            for (int i = 0; i < MpuNumSensors; i++) {
                /* test which sensors needed a physical sensor flush */
                if (mFlushSensor[i]) {
                    /* this sensor is awaiting physical sensor(s) flush */
                    if (mFlushSensor[i] & mFlushPSensor) {
                        /* this sensor needs this physical sensor flush */
                        mFlushSensor[i] &= ~mFlushPSensor; /* remove flag */
                        if (!mFlushSensor[i])
                            /* don't need anymore physical sensor flushes */
                            mFlush |= (1 << i); /* flag to send event */
                    }
                }
            }
            mFlushPSensor = 0;
        }
        return n;
    }

    mHasPendingEvent = false;
    inv_execute_on_data();

    int numEventReceived = 0;

    long msg;
    msg = inv_get_message_level_0(1);
    if (msg) {
        if (msg & INV_MSG_MOTION_EVENT) {
            ALOGV_IF(PROCESS_VERBOSE, "HAL:**** Motion ****\n");
        }
        if (msg & INV_MSG_NO_MOTION_EVENT) {
            ALOGV_IF(PROCESS_VERBOSE, "HAL:***** No Motion *****\n");
            /* after the first no motion, the gyro should be
               calibrated well */
            mGyroAccuracy = SENSOR_STATUS_ACCURACY_HIGH;
            /* if gyros are on and we got a no motion, set a flag
               indicating that the cal file can be written. */
            mHaveGoodMpuCal = true;
        }
        if(msg & INV_MSG_NEW_AB_EVENT) {
            ALOGV_IF(EXTRA_VERBOSE, "HAL:***** New Accel Bias *****\n");
            getAccelBias();
            mAccelAccuracy = inv_get_accel_accuracy();
        }
        if(msg & INV_MSG_NEW_GB_EVENT) {
            ALOGV_IF(EXTRA_VERBOSE, "HAL:***** New Gyro Bias *****\n");
            getGyroBias();
            setGyroBias();
        }
        if(msg & INV_MSG_NEW_FGB_EVENT) {
            ALOGV_IF(EXTRA_VERBOSE, "HAL:***** New Factory Gyro Bias *****\n");
            getFactoryGyroBias();
        }
        if(msg & INV_MSG_NEW_FAB_EVENT) {
            ALOGV_IF(EXTRA_VERBOSE, "HAL:***** New Factory Accel Bias *****\n");
            getFactoryAccelBias();
        }
        if(msg & INV_MSG_NEW_CB_EVENT) {
            ALOGV_IF(EXTRA_VERBOSE, "HAL:***** New Compass Bias *****\n");
            getCompassBias();
            mCompassAccuracy = inv_get_mag_accuracy();
        }
    }

    // handle partial packet read and end marker
    // skip readEvents from hal_outputs
    if (mSkipReadEvents) {
        if(mDataMarkerDetected || mEmptyDataMarkerDetected) {
            if (!mEmptyDataMarkerDetected) {
                // turn off sensors in data_builder
                resetMplStates();
            }
            mEmptyDataMarkerDetected = 0;
            mDataMarkerDetected = 0;

            // handle flush complete event
//            if((count > 0) && mFlushBatchSet && mFlushSensorEnabled != -1)
//                numEventReceived = metaHandler(data, META_DATA_FLUSH_COMPLETE);
        }
        return numEventReceived;
    }

    for (int i = 0; i < MpuNumSensors; i++) {
        int update = 0;

        // handle step detector when ped_q is enabled
        if(mPedUpdate) {
            if (i == StepDetector) {
                update = readDmpPedometerEvents(data, count, StepDetector, 1);
                mPedUpdate = 0;
                if(update == 1 && count > 0) {
                    data->timestamp = mStepSensorTimestamp;
                    count--;
                    numEventReceived++;
                    continue;
                }
            } else {
                if (mPedUpdate == DATA_FORMAT_STEP) {
                    continue;
                }
            }
        }

        // load up virtual sensors
        if (mEnabled & (1 << i)) {
            update = CALL_MEMBER_FN(this, mHandlers[i])(mPendingEvents + i);
            mPendingMask |= (1 << i);

            if (update && (count > 0)) {
                *data++ = mPendingEvents[i];
                count--;
                numEventReceived++;
            }
        }
    }

    return numEventReceived;
}

int MPLSensor::mpuEvent(sensors_event_t *data, int count, Nvs *sensor)
{
    VHANDLER_LOG;
    int n;

    if ((sensor == NULL) || (count < 1))
        return -1;

    n = sensor->readEvents(&mPendingEvent, 1);
    if (n > 0) {
        if (mPendingEvent.type) {
             /* if sensor data */
            if (mPendingEvent.type == SENSOR_TYPE_ACCELEROMETER) {
                mCachedAccelData[0] = (long)mPendingEvent.acceleration.x;
                mCachedAccelData[1] = (long)mPendingEvent.acceleration.y;
                mCachedAccelData[2] = (long)mPendingEvent.acceleration.z;
                mPendingMask |= (1 << Accelerometer);
                inv_build_accel(mCachedAccelData, 0, mPendingEvent.timestamp);
                ALOGV_IF(INPUT_DATA,
                         "%s accel data: %+8ld %+8ld %+8ld - %lld", __func__,
                         mCachedAccelData[0], mCachedAccelData[1],
                         mCachedAccelData[2], mPendingEvent.timestamp);
                /* remember inital 6 axis quaternion */
                inv_time_t tempTimestamp;
                inv_get_6axis_quaternion(mInitial6QuatValue, &tempTimestamp);
                if (mInitial6QuatValue[0] != 0 && mInitial6QuatValue[1] != 0 &&
                    mInitial6QuatValue[2] != 0 && mInitial6QuatValue[3] != 0) {
                    mInitial6QuatValueAvailable = 1;
                    ALOGV_IF(INPUT_DATA && ENG_VERBOSE,
                             "%s 6q data: %+8ld %+8ld %+8ld %+8ld", __func__,
                             mInitial6QuatValue[0], mInitial6QuatValue[1],
                             mInitial6QuatValue[2], mInitial6QuatValue[3]);
                }
                n = 0; /* data not sent now but with mHasPendingEvent */
                mHasPendingEvent = true;
            } else if (mPendingEvent.type == SENSOR_TYPE_GYROSCOPE) {
                mCachedGyroData[0] = (short)mPendingEvent.gyro.x;
                mCachedGyroData[1] = (short)mPendingEvent.gyro.y;
                mCachedGyroData[2] = (short)mPendingEvent.gyro.z;
                mPendingMask |= (1 << Gyro);
                mPendingMask |= (1 << RawGyro);
                inv_build_gyro(mCachedGyroData, mPendingEvent.timestamp);
                ALOGV_IF(INPUT_DATA,
                         "%s_gyro data: %+8d %+8d %+8d - %lld", __func__,
                         mCachedGyroData[0], mCachedGyroData[1],
                         mCachedGyroData[2], mPendingEvent.timestamp);
                n = 0; /* data not sent now but with mHasPendingEvent */
                mHasPendingEvent = true;
            } else if (mPendingEvent.type == SENSOR_TYPE_AMBIENT_TEMPERATURE) {
                long temperature = (long)mPendingEvent.temperature;
                ALOGV_IF(INPUT_DATA,
                         "%s raw temperature data: %ld - %lld", __func__,
                         temperature, mPendingEvent.timestamp);
                temperature <<= 16;
                temperature *= mGyroTempSensor->getScale(-1);
                temperature += mGyroTempSensor->getOffset(-1);
                ALOGV_IF(INPUT_DATA,
                         "%s temperature data: %ld - %lld", __func__,
                         temperature, mPendingEvent.timestamp);
                inv_build_temp(temperature, mPendingEvent.timestamp);
                n = 0; /* data not sent since only used by fusion */
            } else {
// FIXME: Need to apply offset and scale to non-fusion-owned data with same FD
                /* anything else is not owned by fusion so forwarded on */
                *data = mPendingEvent;
            }
        } else { /* sensors_meta_data_event_t */
            if (mPendingEvent.meta_data.sensor ==
                                        mPendingEvents[Accelerometer].sensor) {
                mFlushPSensor |= (1 << Accelerometer);
                n = 0; /* data not sent now but with mHasPendingEvent */
                mHasPendingEvent = true;
            } else if (mPendingEvent.meta_data.sensor ==
                                                 mPendingEvents[Gyro].sensor) {
                mFlushPSensor |= (1 << Gyro);
                n = 0; /* data not sent now but with mHasPendingEvent */
                mHasPendingEvent = true;
            } else {
                /* anything else is not owned by fusion so forwarded on */
                *data = mPendingEvent;
            }
        }
    }
    return n;
}

// collect data for MPL (but NOT sensor service currently), from driver layer
int MPLSensor::buildMpuEvent(sensors_event_t *data, int count)
{
    VHANDLER_LOG;
    int n = 0;
    int ret;

    if (count) {
        ret = mpuEvent(data, count, mAccelSensor);
        if (ret > 0) {
            n += ret;
            count -= ret;
        }
    }
    if (count) {
        ret = mpuEvent(data, count, mGyroSensor);
        if (ret > 0) {
            n += ret;
            count -= ret;
        }
    }
    if (count) {
        ret = mpuEvent(data, count, mGyroTempSensor);
        if (ret > 0) {
            n += ret;
            count -= ret;
        }
    }
    return n;

    mSkipReadEvents = 0;
    int64_t mGyroSensorTimestamp=0, mAccelSensorTimestamp=0, latestTimestamp=0;
    int lp_quaternion_on = 0, sixAxis_quaternion_on = 0,
        ped_quaternion_on = 0, ped_standalone_on = 0;
    size_t nbyte;
    unsigned short data_format = 0;
    int i, mask = 0;
    int sensors;

    sensors = 0;
    if (mLocalSensorMask & INV_THREE_AXIS_GYRO)
        sensors++;
    if (mLocalSensorMask & INV_THREE_AXIS_ACCEL)
        sensors++;
    if (mLocalSensorMask & INV_THREE_AXIS_COMPASS)
        sensors++;
    if (mLocalSensorMask & INV_ONE_AXIS_PRESSURE)
        sensors++;

    //ALOGV("mLocalSensorMask=0x%lx", mLocalSensorMask);
    char *rdata = mIIOBuffer;
    ssize_t rsize = 0;
    ssize_t readCounter = 0;
    char *rdataP = NULL;
    bool doneFlag = 0;

    if (isLowPowerQuatEnabled()) {
        lp_quaternion_on = checkLPQuaternion();
    }
    sixAxis_quaternion_on = check6AxisQuatEnabled();
    ped_quaternion_on = checkPedQuatEnabled();
    ped_standalone_on = checkPedStandaloneEnabled();

    nbyte = MAX_READ_SIZE - mLeftOverBufferSize;

    /* check previous copied buffer */
    /* append with just read data */
    if (mLeftOverBufferSize > 0) {
        ALOGV_IF(0, "append old buffer size=%d", mLeftOverBufferSize);
        memset(rdata, 0, sizeof(rdata));
        memcpy(rdata, mLeftOverBuffer, mLeftOverBufferSize);
            ALOGV_IF(0,
            "HAL:input retrieve old buffer data=:%d, %d, %d, %d,%d, %d, %d, %d,%d, %d, "
            "%d, %d,%d, %d, %d, %d\n",
            rdata[0], rdata[1], rdata[2], rdata[3],
            rdata[4], rdata[5], rdata[6], rdata[7],
            rdata[8], rdata[9], rdata[10], rdata[11],
            rdata[12], rdata[13], rdata[14], rdata[15]);
    }
    rdataP = rdata + mLeftOverBufferSize;
/////////////////////////////////////////////////////////////////////////////////
#if 0
    /* read expected number of bytes */
    rsize = read(iio_fd, rdataP, nbyte);
    if(rsize < 0) {
        /* IIO buffer might have old data.
           Need to flush it if no sensor is on, to avoid infinite
           read loop.*/
        ALOGE("HAL:input data file descriptor not available - (%s)",
              strerror(errno));
        if (sensors == 0) {
            rsize = read(iio_fd, rdata, MAX_SUSPEND_BATCH_PACKET_SIZE);
            if(rsize > 0) {
                ALOGV_IF(ENG_VERBOSE, "HAL:input data flush rsize=%d", (int)rsize);
#ifdef TESTING
                ALOGV_IF(INPUT_DATA,
                         "HAL:input rdata:r=%d, n=%d,"
                         "%d, %d, %d, %d, %d, %d, %d, %d",
                         (int)rsize, nbyte,
                         rdataP[0], rdataP[1], rdataP[2], rdataP[3],
                         rdataP[4], rdataP[5], rdataP[6], rdataP[7]);
#endif
                mLeftOverBufferSize = 0;
            }
        }
        return;
    }
#else
    rsize = 0;
#endif //0


#ifdef TESTING
    ALOGV_IF(INPUT_DATA,
         "HAL:input just read rdataP:r=%d, n=%d,"
         "%d, %d, %d, %d,%d, %d, %d, %d,%d, %d, %d, %d,%d, %d, %d, %d,"
         "%d, %d, %d, %d,%d, %d, %d, %d\n",
         (int)rsize, nbyte,
         rdataP[0], rdataP[1], rdataP[2], rdataP[3],
         rdataP[4], rdataP[5], rdataP[6], rdataP[7],
         rdataP[8], rdataP[9], rdataP[10], rdataP[11],
         rdataP[12], rdataP[13], rdataP[14], rdataP[15],
         rdataP[16], rdataP[17], rdataP[18], rdataP[19],
         rdataP[20], rdataP[21], rdataP[22], rdataP[23]);

    ALOGV_IF(INPUT_DATA,
         "HAL:input rdata:r=%d, n=%d,"
         "%d, %d, %d, %d,%d, %d, %d, %d,%d, %d, %d, %d,%d, %d, %d, %d,"
         "%d, %d, %d, %d,%d, %d, %d, %d\n",
         (int)rsize, nbyte,
         rdata[0], rdata[1], rdata[2], rdata[3],
         rdata[4], rdata[5], rdata[6], rdata[7],
         rdata[8], rdata[9], rdata[10], rdata[11],
         rdata[12], rdata[13], rdata[14], rdata[15],
         rdata[16], rdata[17], rdata[18], rdata[19],
         rdata[20], rdata[21], rdata[22], rdata[23]);
#endif

    if(rsize + mLeftOverBufferSize < MAX_READ_SIZE) {
        /* store packet then return */
        mLeftOverBufferSize += rsize;
        memcpy(mLeftOverBuffer, rdata, mLeftOverBufferSize);

#ifdef TESTING
        ALOGV_IF(1, "HAL:input data has batched partial packet");
        ALOGV_IF(1, "HAL:input data batched mLeftOverBufferSize=%d", mLeftOverBufferSize);
        ALOGV_IF(1,
            "HAL:input catch up batched retrieve buffer=:%d, %d, %d, %d, %d, %d, %d, %d,"
            "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
            mLeftOverBuffer[0], mLeftOverBuffer[1], mLeftOverBuffer[2], mLeftOverBuffer[3],
            mLeftOverBuffer[4], mLeftOverBuffer[5], mLeftOverBuffer[6], mLeftOverBuffer[7],
            mLeftOverBuffer[8], mLeftOverBuffer[9], mLeftOverBuffer[10], mLeftOverBuffer[11],
            mLeftOverBuffer[12], mLeftOverBuffer[13], mLeftOverBuffer[14], mLeftOverBuffer[15]);
#endif
        mSkipReadEvents = 1;
        return -1;
    }

    /* reset data and count pointer */
    rdataP = rdata;
    readCounter = rsize + mLeftOverBufferSize;
    ALOGV_IF(0, "HAL:input readCounter set=%d", (int)readCounter);

    ALOGV_IF(INPUT_DATA && ENG_VERBOSE,
             "HAL:input b=%d rdata= %d nbyte= %d rsize= %d readCounter= %d",
             checkBatchEnabled(), *((short *) rdata), nbyte, (int)rsize, (int)readCounter);
    ALOGV_IF(INPUT_DATA && ENG_VERBOSE,
             "HAL:input sensors= %d, lp_q_on= %d, 6axis_q_on= %d, "
             "ped_q_on= %d, ped_standalone_on= %d",
             sensors, lp_quaternion_on, sixAxis_quaternion_on, ped_quaternion_on,
             ped_standalone_on);

    while (readCounter > 0) {
        // since copied buffer is already accounted for, reset left over size
        mLeftOverBufferSize = 0;
        // clear data format mask for parsing the next set of data
        mask = 0;
        data_format = *((short *)(rdata));
        ALOGV_IF(INPUT_DATA && ENG_VERBOSE,
                 "HAL:input data_format=%x", data_format);

        if(checkValidHeader(data_format) == 0) {
            ALOGE("HAL:input invalid data_format 0x%02X", data_format);
            return -1;
        }

        if (data_format & DATA_FORMAT_STEP) {
            if (data_format == DATA_FORMAT_STEP) {
                rdata += BYTES_PER_SENSOR;
                latestTimestamp = *((long long*) (rdata));
                ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "STEP DETECTED:0x%x - ts: %lld", data_format, latestTimestamp);
                // readCounter is decrement by 24 because DATA_FORMAT_STEP only applies in batch  mode
                readCounter -= BYTES_PER_SENSOR_PACKET;
            } else {
                ALOGV_IF(0, "STEP DETECTED:0x%x", data_format);
            }
            mPedUpdate |= data_format;
            mask |= DATA_FORMAT_STEP;
            // cancels step bit
            data_format &= (~DATA_FORMAT_STEP);
        }

        if (data_format & DATA_FORMAT_MARKER) {
            ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "MARKER DETECTED:0x%x", data_format);
            readCounter -= BYTES_PER_SENSOR;
//            if (mFlushSensorEnabled != -1) {
//                mFlushBatchSet = 1;
//            }
            mDataMarkerDetected = 1;
            mSkipReadEvents = 1;
        }
        else if (data_format & DATA_FORMAT_EMPTY_MARKER) {
            ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "EMPTY MARKER DETECTED:0x%x", data_format);
            readCounter -= BYTES_PER_SENSOR;
//            if (mFlushSensorEnabled != -1) {
//                mFlushBatchSet = 1;
//            }
            mEmptyDataMarkerDetected = 1;
            mSkipReadEvents = 1;
        }
        else if (data_format == DATA_FORMAT_QUAT) {
            ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "QUAT DETECTED:0x%x", data_format);
            if (readCounter >= BYTES_QUAT_DATA) {
                mCachedQuaternionData[0] = *((int *) (rdata + 4));
                mCachedQuaternionData[1] = *((int *) (rdata + 8));
                mCachedQuaternionData[2] = *((int *) (rdata + 12));
                rdata += QUAT_ONLY_LAST_PACKET_OFFSET;
                mQuatSensorTimestamp = *((long long*) (rdata));
                mask |= DATA_FORMAT_QUAT;
                readCounter -= BYTES_QUAT_DATA;
            }
            else {
                doneFlag = 1;
            }
        }
        else if (data_format == DATA_FORMAT_6_AXIS) {
            ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "6AXIS DETECTED:0x%x", data_format);
            if (readCounter >= BYTES_QUAT_DATA) {
                mCached6AxisQuaternionData[0] = *((int *) (rdata + 4));
                mCached6AxisQuaternionData[1] = *((int *) (rdata + 8));
                mCached6AxisQuaternionData[2] = *((int *) (rdata + 12));
                rdata += QUAT_ONLY_LAST_PACKET_OFFSET;
                mQuatSensorTimestamp = *((long long*) (rdata));
                mask |= DATA_FORMAT_6_AXIS;
                readCounter -= BYTES_QUAT_DATA;
            }
            else {
                doneFlag = 1;
            }
        }
        else if (data_format == DATA_FORMAT_PED_QUAT) {
            ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "PED QUAT DETECTED:0x%x", data_format);
            if (readCounter >= BYTES_PER_SENSOR_PACKET) {
                mCachedPedQuaternionData[0] = *((short *) (rdata + 2));
                mCachedPedQuaternionData[1] = *((short *) (rdata + 4));
                mCachedPedQuaternionData[2] = *((short *) (rdata + 6));
                rdata += BYTES_PER_SENSOR;
                mQuatSensorTimestamp = *((long long*) (rdata));
                mask |= DATA_FORMAT_PED_QUAT;
                readCounter -= BYTES_PER_SENSOR_PACKET;
            }
            else {
                doneFlag = 1;
            }
        }
        else if (data_format == DATA_FORMAT_PED_STANDALONE) {
            ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "STANDALONE STEP DETECTED:0x%x", data_format);
            if (readCounter >= BYTES_PER_SENSOR_PACKET) {
                rdata += BYTES_PER_SENSOR;
                mStepSensorTimestamp = *((long long*) (rdata));
                mask |= DATA_FORMAT_PED_STANDALONE;
                readCounter -= BYTES_PER_SENSOR_PACKET;
                mPedUpdate |= data_format;
            }
            else {
                doneFlag = 1;
            }
        }
        else if (data_format == DATA_FORMAT_GYRO) {
            ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "GYRO DETECTED:0x%x", data_format);
            if (readCounter >= BYTES_PER_SENSOR_PACKET) {
                mCachedGyroData[0] = *((short *) (rdata + 2));
                mCachedGyroData[1] = *((short *) (rdata + 4));
                mCachedGyroData[2] = *((short *) (rdata + 6));
                rdata += BYTES_PER_SENSOR;
                mGyroSensorTimestamp = *((long long*) (rdata));
                mask |= DATA_FORMAT_GYRO;
                readCounter -= BYTES_PER_SENSOR_PACKET;
            } else {
                doneFlag = 1;
            }
        }
        else if (data_format == DATA_FORMAT_ACCEL) {
            ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "ACCEL DETECTED:0x%x", data_format);
            if (readCounter >= BYTES_PER_SENSOR_PACKET) {
                mCachedAccelData[0] = *((short *) (rdata + 2));
                mCachedAccelData[1] = *((short *) (rdata + 4));
                mCachedAccelData[2] = *((short *) (rdata + 6));
                rdata += BYTES_PER_SENSOR;
                mAccelSensorTimestamp = *((long long*) (rdata));
                mask |= DATA_FORMAT_ACCEL;
                readCounter -= BYTES_PER_SENSOR_PACKET;
            }
            else {
                doneFlag = 1;
            }
        }
        else if (data_format == DATA_FORMAT_COMPASS) {
            ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "COMPASS DETECTED:0x%x", data_format);
            if (readCounter >= BYTES_PER_SENSOR_PACKET) {
                if (mCompassSensor != NULL) {
                    mCachedCompassData[0] = *((short *) (rdata + 2));
                    mCachedCompassData[1] = *((short *) (rdata + 4));
                    mCachedCompassData[2] = *((short *) (rdata + 6));
                    rdata += BYTES_PER_SENSOR;
                    mCompassTimestamp = *((long long*) (rdata));
                    mask |= DATA_FORMAT_COMPASS;
                    readCounter -= BYTES_PER_SENSOR_PACKET;
                }
            }
            else {
                doneFlag = 1;
            }
        }
        else if (data_format == DATA_FORMAT_PRESSURE) {
            ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "PRESSURE DETECTED:0x%x", data_format);
            if (readCounter >= BYTES_QUAT_DATA) {
                if (mPressureSensor != NULL) {
                    mCachedPressureData =
                        ((*((short *)(rdata + 4))) << 16) +
                        (*((unsigned short *) (rdata + 6)));
                    rdata += BYTES_PER_SENSOR;
                    mPressureTimestamp = *((long long*) (rdata));
                    if (mCachedPressureData != 0) {
                        mask |= DATA_FORMAT_PRESSURE;
                    }
                    readCounter -= BYTES_PER_SENSOR_PACKET;
                }
            } else {
                doneFlag = 1;
            }
        }

        if(doneFlag == 0) {
            rdata += BYTES_PER_SENSOR;
            ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "HAL: input data doneFlag is zero, readCounter=%d", (int)readCounter);
        }
        else {
            ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "HAL: input data doneFlag is set, readCounter=%d", (int)readCounter);
        }

        /* read ahead and store left over data if any */
        if (readCounter != 0) {
            int currentBufferCounter = 0;
            ALOGV_IF(0, "Not enough data readCounter=%d, expected nbyte=%d, rsize=%d", (int)readCounter, nbyte, (int)rsize);
            memset(mLeftOverBuffer, 0, sizeof(mLeftOverBuffer));
            memcpy(mLeftOverBuffer, rdata, readCounter);
            ALOGV_IF(0,
                     "HAL:input store rdata=:%d, %d, %d, %d,%d, %d, %d, %d,%d, "
                     "%d, %d, %d,%d, %d, %d, %d\n",
                     mLeftOverBuffer[0], mLeftOverBuffer[1], mLeftOverBuffer[2], mLeftOverBuffer[3],
                     mLeftOverBuffer[4], mLeftOverBuffer[5], mLeftOverBuffer[6], mLeftOverBuffer[7],
                     mLeftOverBuffer[8], mLeftOverBuffer[9], mLeftOverBuffer[10], mLeftOverBuffer[11],
                     mLeftOverBuffer[12],mLeftOverBuffer[13],mLeftOverBuffer[14], mLeftOverBuffer[15]);

            mLeftOverBufferSize = readCounter;
            readCounter = 0;
            ALOGV_IF(0, "Stored number of bytes:%d", mLeftOverBufferSize);
        } else {
            /* reset count since this is the last packet for the data set */
            readCounter = 0;
            mLeftOverBufferSize = 0;
        }

        /* handle data read */
        if (mask & DATA_FORMAT_GYRO) {
            /* batch mode does not batch temperature */
            /* disable temperature read */
            if (!(mFeatureActiveMask & INV_DMP_BATCH_MODE)) {
                // send down temperature every 0.5 seconds
                // with timestamp measured in "driver" layer
                if(mGyroSensorTimestamp - mTempCurrentTime >= 500000000LL) {
                    mTempCurrentTime = mGyroSensorTimestamp;
                    long long temperature[2];
                    if(inv_read_temperature(temperature) == 0) {
                        ALOGV_IF(INPUT_DATA,
                                 "HAL:input inv_read_temperature = %lld, timestamp= %lld",
                                 temperature[0], temperature[1]);
                        inv_build_temp(temperature[0], temperature[1]);
                    }
#ifdef TESTING
                    long bias[3], temp, temp_slope[3];
                    inv_get_mpl_gyro_bias(bias, &temp);
                    inv_get_gyro_ts(temp_slope);
                    ALOGI("T: %.3f "
                          "GB: %+13f %+13f %+13f "
                          "TS: %+13f %+13f %+13f "
                          "\n",
                          (float)temperature[0] / 65536.f,
                          (float)bias[0] / 65536.f / 16.384f,
                          (float)bias[1] / 65536.f / 16.384f,
                          (float)bias[2] / 65536.f / 16.384f,
                          temp_slope[0] / 65536.f,
                          temp_slope[1] / 65536.f,
                          temp_slope[2] / 65536.f);
#endif
                }
            }
            mPendingMask |= 1 << Gyro;
            mPendingMask |= 1 << RawGyro;

            inv_build_gyro(mCachedGyroData, mGyroSensorTimestamp);
            ALOGV_IF(INPUT_DATA,
                     "HAL:input inv_build_gyro: %+8d %+8d %+8d - %lld",
                     mCachedGyroData[0], mCachedGyroData[1],
                     mCachedGyroData[2], mGyroSensorTimestamp);
            latestTimestamp = mGyroSensorTimestamp;
        }

        if (mask & DATA_FORMAT_ACCEL) {
            mPendingMask |= 1 << Accelerometer;
            inv_build_accel(mCachedAccelData, 0, mAccelSensorTimestamp);
            ALOGV_IF(INPUT_DATA,
                     "HAL:input inv_build_accel: %+8ld %+8ld %+8ld - %lld",
                     mCachedAccelData[0], mCachedAccelData[1],
                     mCachedAccelData[2], mAccelSensorTimestamp);
                /* remember inital 6 axis quaternion */
                inv_time_t tempTimestamp;
                inv_get_6axis_quaternion(mInitial6QuatValue, &tempTimestamp);
                if (mInitial6QuatValue[0] != 0 && mInitial6QuatValue[1] != 0 &&
                        mInitial6QuatValue[2] != 0 && mInitial6QuatValue[3] != 0) {
                    mInitial6QuatValueAvailable = 1;
                    ALOGV_IF(INPUT_DATA && ENG_VERBOSE,
                             "HAL:input build 6q init: %+8ld %+8ld %+8ld %+8ld",
                             mInitial6QuatValue[0], mInitial6QuatValue[1],
                             mInitial6QuatValue[2], mInitial6QuatValue[3]);
                }
            latestTimestamp = mAccelSensorTimestamp;
        }

        if (mask & DATA_FORMAT_COMPASS) {
            if (mCompassSensor != NULL) {
                int status = 0;
                inv_build_compass(mCachedCompassData, status,
                                  mCompassTimestamp);
                ALOGV_IF(INPUT_DATA,
                         "HAL:input inv_build_compass: %+8ld %+8ld %+8ld - %lld",
                         mCachedCompassData[0], mCachedCompassData[1],
                         mCachedCompassData[2], mCompassTimestamp);
                latestTimestamp = mCompassTimestamp;
            }
        }

        if (mask & DATA_FORMAT_QUAT) {
            /* if bias was applied to DMP bias,
               set status bits to disable gyro bias cal */
            int status = 0;
            if (mGyroBiasApplied == true) {
                ALOGV_IF(INPUT_DATA && ENG_VERBOSE, "HAL:input dmp bias is used");
                status |= INV_BIAS_APPLIED;
            }
            status |= INV_CALIBRATED | INV_QUAT_3AXIS | INV_QUAT_3ELEMENT; /* default 32 (16/32bits) */
            inv_build_quat(mCachedQuaternionData,
                       status,
                       mQuatSensorTimestamp);
            ALOGV_IF(INPUT_DATA,
                     "HAL:input inv_build_quat-3x: %+8ld %+8ld %+8ld - %lld",
                     mCachedQuaternionData[0], mCachedQuaternionData[1],
                     mCachedQuaternionData[2],
                     mQuatSensorTimestamp);
            latestTimestamp = mQuatSensorTimestamp;
        }

        if (mask & DATA_FORMAT_6_AXIS) {
            /* if bias was applied to DMP bias,
               set status bits to disable gyro bias cal */
            int status = 0;
            if (mGyroBiasApplied == true) {
                ALOGV_IF(INPUT_DATA && ENG_VERBOSE, "HAL:input dmp bias is used");
                status |= INV_QUAT_6AXIS;
            }
            status |= INV_CALIBRATED | INV_QUAT_6AXIS | INV_QUAT_3ELEMENT; /* default 32 (16/32bits) */
            inv_build_quat(mCached6AxisQuaternionData,
                       status,
                       mQuatSensorTimestamp);
            ALOGV_IF(INPUT_DATA,
                     "HAL:input inv_build_quat-6x: %+8ld %+8ld %+8ld - %lld",
                     mCached6AxisQuaternionData[0], mCached6AxisQuaternionData[1],
                     mCached6AxisQuaternionData[2], mQuatSensorTimestamp);
            latestTimestamp = mQuatSensorTimestamp;
        }

        if (mask & DATA_FORMAT_PED_QUAT) {
            /* if bias was applied to DMP bias,
               set status bits to disable gyro bias cal */
            int status = 0;
            if (mGyroBiasApplied == true) {
                ALOGV_IF(INPUT_DATA && ENG_VERBOSE,
                         "HAL:input dmp bias is used");
                status |= INV_QUAT_6AXIS;
            }
            status |= INV_CALIBRATED | INV_QUAT_6AXIS | INV_QUAT_3ELEMENT; /* default 32 (16/32bits) */
            mCachedPedQuaternionData[0] = mCachedPedQuaternionData[0] << 16;
            mCachedPedQuaternionData[1] = mCachedPedQuaternionData[1] << 16;
            mCachedPedQuaternionData[2] = mCachedPedQuaternionData[2] << 16;
            inv_build_quat(mCachedPedQuaternionData,
                       status,
                       mQuatSensorTimestamp);

            ALOGV_IF(INPUT_DATA,
                     "HAL:HAL:input inv_build_quat-ped_6x: %+8ld %+8ld %+8ld - %lld",
                     mCachedPedQuaternionData[0], mCachedPedQuaternionData[1],
                     mCachedPedQuaternionData[2], mQuatSensorTimestamp);
            latestTimestamp = mQuatSensorTimestamp;
        }

        if (mask & DATA_FORMAT_PRESSURE) {
            if (mPressureSensor != NULL) {
                int status = 0;
                if (mLocalSensorMask & INV_ONE_AXIS_PRESSURE) {
                    latestTimestamp = mPressureTimestamp;
                    mPressureUpdate = 1;
                    inv_build_pressure(mCachedPressureData,
                                status,
                                mPressureTimestamp);
                    ALOGV_IF(INPUT_DATA,
                             "HAL:input inv_build_pressure: %+8ld - %lld",
                             mCachedPressureData, mPressureTimestamp);
                }
            }
        }

        /* take the latest timestamp */
        if (mask & DATA_FORMAT_STEP) {
        /* work around driver output duplicate step detector bit */
            if (latestTimestamp > mStepSensorTimestamp) {
                mStepSensorTimestamp = latestTimestamp;
                ALOGV_IF(INPUT_DATA,
                         "HAL:input build step: 1 - %lld", mStepSensorTimestamp);
            } else {
                mPedUpdate = 0;
            }
        }
    } //while end
    mHasPendingEvent = true;
    return 0;
}

bool MPLSensor::hasPendingEvents() {
    VFUNC_LOG;

    return mHasPendingEvent;
}

int MPLSensor::checkValidHeader(unsigned short data_format)
{
    if(data_format & DATA_FORMAT_STEP) {
        data_format &= (~DATA_FORMAT_STEP);
    }

    ALOGV_IF(ENG_VERBOSE && INPUT_DATA, "check data_format=%x", data_format);

    if ((data_format == DATA_FORMAT_PED_STANDALONE) ||
        (data_format == DATA_FORMAT_PED_QUAT) ||
        (data_format == DATA_FORMAT_6_AXIS) ||
        (data_format == DATA_FORMAT_QUAT) ||
        (data_format == DATA_FORMAT_COMPASS) ||
        (data_format == DATA_FORMAT_GYRO) ||
        (data_format == DATA_FORMAT_ACCEL) ||
        (data_format == DATA_FORMAT_PRESSURE) ||
        (data_format == DATA_FORMAT_EMPTY_MARKER) ||
        (data_format == DATA_FORMAT_MARKER))
            return 1;
     else {
        ALOGV_IF(ENG_VERBOSE, "bad data_format = %x", data_format);
        return 0;
    }
}

/* use for both MPUxxxx and third party compass */
int MPLSensor::buildCompassEvent(sensors_event_t *data, int count)
{
    VHANDLER_LOG;
    int n;

    if ((mCompassSensor == NULL) || (count < 1))
        return -1;

    n = mCompassSensor->readEvents(&mPendingEvent, 1);
    if (n > 0) {
        if (mPendingEvent.type) {
            if (mPendingEvent.type == SENSOR_TYPE_GEOMAGNETIC_FIELD) {
                mCachedCompassData[0] = (long)mPendingEvent.magnetic.x;
                mCachedCompassData[1] = (long)mPendingEvent.magnetic.y;
                mCachedCompassData[2] = (long)mPendingEvent.magnetic.z;
                mCompassTimestamp = mPendingEvent.timestamp;
                if (mLocalSensorMask & INV_THREE_AXIS_COMPASS) {
                    inv_build_compass(mCachedCompassData, 0,
                                      mCompassTimestamp);
                    ALOGV_IF(INPUT_DATA,
                             "HAL:input inv_build_compass: %+8ld %+8ld %+8ld - %lld",
                             mCachedCompassData[0], mCachedCompassData[1],
                             mCachedCompassData[2], mCompassTimestamp);
                    mHasPendingEvent = true;
                }
                n = 0; /* data not sent now but with mHasPendingEvent */
            } else {
// FIXME: Need to apply offset and scale to non-fusion-owned data with same FD
                /* anything else is not owned by fusion so forwarded on */
                *data = mPendingEvent;
            }
        } else { /* sensors_meta_data_event_t */
            if (mPendingEvent.meta_data.sensor ==
                                        mPendingEvents[MagneticField].sensor) {
                mFlushPSensor |= (1 << MagneticField);
                n = 0; /* data not sent now but with mHasPendingEvent */
                mHasPendingEvent = true;
            } else {
                /* anything else is not owned by fusion so forwarded on */
                *data = mPendingEvent;
            }
        }
    }
    return n;
}

int MPLSensor::resetCompass(void)
{
    VFUNC_LOG;

    //Reset compass cal if enabled
    if (mMplFeatureActiveMask & INV_COMPASS_CAL) {
       ALOGV_IF(EXTRA_VERBOSE, "HAL:Reset compass cal");
       inv_init_vector_compass_cal();
    }

    //Reset compass fit if enabled
    if (mMplFeatureActiveMask & INV_COMPASS_FIT) {
       ALOGV_IF(EXTRA_VERBOSE, "HAL:Reset compass fit");
       inv_init_compass_fit();
    }

    return 0;
}

int MPLSensor::getFd(int32_t handle)
{
    VFUNC_LOG;
    return -1;
}

int MPLSensor::turnOffAccelFifo(void)
{
    VFUNC_LOG;
    int i, res = 0, tempFd;
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
             0, mpu.accel_fifo_enable, getTimestamp());
    res += write_sysfs_int(mpu.accel_fifo_enable, 0);
    return res;
}

int MPLSensor::turnOffGyroFifo(void)
{
    VFUNC_LOG;
    int i, res = 0, tempFd;
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
             0, mpu.gyro_fifo_enable, getTimestamp());
    res += write_sysfs_int(mpu.gyro_fifo_enable, 0);
    return res;
}

int MPLSensor::getDmpRate(int64_t *wanted)
{
    VFUNC_LOG;

      // set DMP output rate to FIFO
      if(mDmpOn == 1) {
        ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                 int(1000000000.f / *wanted), mpu.three_axis_q_rate,
                 getTimestamp());
        write_sysfs_int(mpu.three_axis_q_rate, 1000000000.f / *wanted);
        ALOGV_IF(PROCESS_VERBOSE,
                 "HAL:DMP three axis rate %.2f Hz", 1000000000.f / *wanted);
        if (mFeatureActiveMask & INV_DMP_BATCH_MODE) {
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     int(1000000000.f / *wanted), mpu.six_axis_q_rate,
                     getTimestamp());
            write_sysfs_int(mpu.six_axis_q_rate, 1000000000.f / *wanted);
            ALOGV_IF(PROCESS_VERBOSE,
                     "HAL:DMP six axis rate %.2f Hz", 1000000000.f / *wanted);

            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     int(1000000000.f / *wanted), mpu.ped_q_rate,
                     getTimestamp());
            write_sysfs_int(mpu.ped_q_rate, 1000000000.f / *wanted);
            ALOGV_IF(PROCESS_VERBOSE,
                     "HAL:DMP ped quaternion rate %.2f Hz", 1000000000.f / *wanted);
        }
        // DMP running rate must be @ 200Hz
        *wanted= RATE_200HZ;
        ALOGV_IF(PROCESS_VERBOSE,
                 "HAL:DMP rate= %.2f Hz", 1000000000.f / *wanted);
    }
    return 0;
}

int MPLSensor::getStepCountPollTime(void)
{
    VFUNC_LOG;
    /* clamped to 1ms? as spec, still rather large */
    return 1000;
}

bool MPLSensor::hasStepCountPendingEvents(void)
{
    VFUNC_LOG;
    if (mDmpStepCountEnabled) {
        struct timespec t_now;
        int64_t interval = 0;

        clock_gettime(CLOCK_MONOTONIC, &t_now);
        interval = ((int64_t(t_now.tv_sec) * 1000000000LL + t_now.tv_nsec) -
                    (int64_t(mt_pre.tv_sec) * 1000000000LL + mt_pre.tv_nsec));

        if (interval < mStepCountPollTime) {
            ALOGV_IF(0,
                     "Step Count interval elapsed: %lld, triggered: %lld",
                     interval, mStepCountPollTime);
            return false;
        } else {
            clock_gettime(CLOCK_MONOTONIC, &mt_pre);
            ALOGV_IF(0, "Step Count previous time: %ld ms",
                     mt_pre.tv_nsec / 1000);
            return true;
        }
    }
    return false;
}

int MPLSensor::inv_read_temperature(long long *data)
{
    VHANDLER_LOG;
    int64_t timestamp;
    long temp;
    int raw;
    int ret;

    if (mGyroTempSensor == NULL)
        return -1;

    ret = mGyroTempSensor->readRaw(-1, &raw);
    if (!ret) {
        /* get current timestamp */
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts) ;
        timestamp = (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
        data[1] = timestamp;

        temp = raw;
        temp <<= 16;
        temp *= mGyroTempSensor->getScale(-1);
        temp += mGyroTempSensor->getOffset(-1);
        data[0] = temp;
        ALOGV_IF(ENG_VERBOSE && INPUT_DATA,
                 "%s raw = %d, temperature = %ld, timestamp = %lld",
                 __func__, raw, temp, timestamp);
    }
    return ret;
}

int MPLSensor::inv_read_dmp_state(int fd)
{
    VFUNC_LOG;

    if(fd < 0)
        return -1;

    int count = 0;
    char raw_buf[10];
    short raw = 0;

    memset(raw_buf, 0, sizeof(raw_buf));
    count = read_attribute_sensor(fd, raw_buf, sizeof(raw_buf));
    if(count < 1) {
        ALOGE("HAL:error reading dmp state");
        close(fd);
        return -1;
    }
    count = sscanf(raw_buf, "%hd", &raw);
    if(count < 0) {
        ALOGE("HAL:dmp state data is invalid");
        close(fd);
        return -1;
    }
    ALOGV_IF(EXTRA_VERBOSE, "HAL:dmp state = %d, count = %d", raw, count);
    close(fd);
    return (int)raw;
}

int MPLSensor::inv_read_sensor_bias(int fd, long *data)
{
    VFUNC_LOG;

    if(fd == -1)
        return -1;

    char buf[50];
    char x[15], y[15], z[15];

    memset(buf, 0, sizeof(buf));
    int count = read_attribute_sensor(fd, buf, sizeof(buf));
    if(count < 1) {
        ALOGE("HAL:Error reading gyro bias");
        return -1;
    }
    count = sscanf(buf, "%[^','],%[^','],%[^',']", x, y, z);
    if(count) {
        /* scale appropriately for MPL */
        ALOGV_IF(ENG_VERBOSE,
                 "HAL:pre-scaled bias: X:Y:Z (%ld, %ld, %ld)",
                 atol(x), atol(y), atol(z));

        data[0] = (long)(atol(x) / 10000 * (1L << 16));
        data[1] = (long)(atol(y) / 10000 * (1L << 16));
        data[2] = (long)(atol(z) / 10000 * (1L << 16));

        ALOGV_IF(ENG_VERBOSE,
                 "HAL:scaled bias: X:Y:Z (%ld, %ld, %ld)",
                 data[0], data[1], data[2]);
    }
    return 0;
}

int MPLSensor::getSensorList(struct sensor_t *list, int index, int limit)
{
    VFUNC_LOG;

    int index_start;
    int i;

    if (list == NULL) /* just get sensor count */
        return ARRAY_SIZE(sMplSensorList);

    mSensorList = list;
    if (mAccelSensor != NULL) {
        mAccelSensorList = mAccelSensor->getSensorListPtr();
        if (mAccelSensorList == NULL) {
            ALOGE("%s ERR: no accelerometer device in sensor list\n", __func__);
            mAccelSensor = NULL;
        } else {
            mPendingEvents[Accelerometer].sensor = mAccelSensorList->handle;
            if (mAccelSensorList->version < 3) {
                /* FIXME: REMOVE ME */
                mAccelSensorList->maxRange *= GRAVITY_EARTH;
                mAccelSensorList->resolution *= GRAVITY_EARTH;
            }
            if (!mDmpLoaded) {
                mAccelSensorList->fifoReservedEventCount = 0;
                mAccelSensorList->fifoMaxEventCount = 0;
            }
        }
    }
    if (mGyroSensor != NULL) {
        mGyroSensorList = mGyroSensor->getSensorListPtr();
        if (mGyroSensorList == NULL) {
            ALOGE("%s ERR: no gyro device in sensor list\n", __func__);
            mGyroSensor = NULL;
        } else {
            mPendingEvents[Gyro].sensor = mGyroSensorList->handle;
            if (!mDmpLoaded) {
                mGyroSensorList->fifoReservedEventCount = 0;
                mGyroSensorList->fifoMaxEventCount = 0;
            }
        }
    }
    if (mCompassSensor != NULL) {
        mCompassSensorList = mCompassSensor->getSensorListPtr();
        if (mCompassSensorList == NULL) {
            ALOGE("%s ERR: no compass device in sensor list\n", __func__);
            mCompassSensor = NULL;
        } else {
            mPendingEvents[MagneticField].sensor = mCompassSensorList->handle;
            if (!mDmpLoaded) {
                mCompassSensorList->fifoReservedEventCount = 0;
                mCompassSensorList->fifoMaxEventCount = 0;
            }
        }
    }

    index_start = index;
    if ((index - index_start) >= limit)
        return index - index_start;

    /* the following needs gyro */
    if (mGyroSensorList) {
        /* copy from list for the raw gyro sensor */
        memcpy(&list[index], mGyroSensorList, sizeof(struct sensor_t));
        list[index].type = SENSOR_TYPE_GYROSCOPE_UNCALIBRATED;
        list[index].handle = index + 1;
        mPendingEvents[RawGyro].sensor = list[index].handle;
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;
    }

    /* the following needs compass */
    if (mCompassSensorList) {
        /* copy from list for the raw compass sensor */
        memcpy(&list[index], mCompassSensorList, sizeof(struct sensor_t));
        list[index].type = SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED;
        list[index].handle = index + 1;
        mPendingEvents[RawMagneticField].sensor = list[index].handle;
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;
    }

    /* the virtual sensors need a combination of the physical sensors */
    if (mAccelSensorList && mGyroSensorList && mCompassSensorList) {

        memcpy(&list[index], &sMplSensorList[Orientation],
               sizeof(struct sensor_t));
        list[index].power = mGyroSensorList->power +
                            mAccelSensorList->power +
                            mCompassSensorList->power;
        list[index].handle = index + 1;
        mPendingEvents[Orientation].sensor = list[index].handle;
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;

        memcpy(&list[index], &sMplSensorList[RotationVector],
               sizeof(struct sensor_t));
        list[index].power = mGyroSensorList->power +
                            mAccelSensorList->power +
                            mCompassSensorList->power;
        list[index].handle = index + 1;
        mPendingEvents[RotationVector].sensor = list[index].handle;
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;

        memcpy(&list[index], &sMplSensorList[LinearAccel],
               sizeof(struct sensor_t));
        list[index].power = mGyroSensorList->power +
                            mAccelSensorList->power +
                            mCompassSensorList->power;
        list[index].resolution = mAccelSensorList->resolution;
        list[index].maxRange = mAccelSensorList->maxRange;
        list[index].handle = index + 1;
        mPendingEvents[LinearAccel].sensor = list[index].handle;
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;

        memcpy(&list[index], &sMplSensorList[Gravity],
               sizeof(struct sensor_t));
        list[index].power = mGyroSensorList->power +
                            mAccelSensorList->power +
                            mCompassSensorList->power;
        list[index].handle = index + 1;
        mPendingEvents[Gravity].sensor = list[index].handle;
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;
    }

    if (mAccelSensorList && mCompassSensorList) {
        memcpy(&list[index], &sMplSensorList[GeomagneticRotationVector],
               sizeof(struct sensor_t));
        list[index].power = mAccelSensorList->power +
                            mCompassSensorList->power;
        list[index].handle = index + 1;
        mPendingEvents[GeomagneticRotationVector].sensor = list[index].handle;
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;
    }

    if (mAccelSensorList && mGyroSensorList) {
        memcpy(&list[index], &sMplSensorList[GameRotationVector],
               sizeof(struct sensor_t));
        if (mDmpLoaded) {
            list[index].fifoReservedEventCount = FIFO_RESERVED_EVENT_COUNT;
            list[index].fifoMaxEventCount = FIFO_MAX_EVENT_COUNT;
        }
        list[index].power = mGyroSensorList->power +
                            mAccelSensorList->power;
        list[index].handle = index + 1;
        mPendingEvents[GameRotationVector].sensor = list[index].handle;
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;
    }

    if (mPressureSensor != NULL) {
        /* need to find pressure in the list */
        for (i = 0; i < index; i++) {
            if (list[i].type == SENSOR_TYPE_PRESSURE)
                break;
        }
        if (i == index) {
            mPressureSensorList = NULL;
            ALOGE("%s ERR: no pressure device in sensor list\n", __func__);
        } else {
            mPressureSensorList = &list[i];
            mPendingEvents[Pressure].sensor = mPressureSensorList->handle;
            if (!mDmpLoaded) {
                mPressureSensorList->fifoReservedEventCount = 0;
                mPressureSensorList->fifoMaxEventCount = 0;
            }
        }
    }

    if (mDmpLoaded) {
        memcpy(&list[index], &sMplSensorList[SignificantMotion],
               sizeof(struct sensor_t));
        list[index].handle = index + 1;
        mPendingEvents[SignificantMotion].sensor = list[index].handle;
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;

        memcpy(&list[index], &sMplSensorList[StepDetector],
               sizeof(struct sensor_t));
        list[index].handle = index + 1;
        mPendingEvents[StepDetector].sensor = list[index].handle;
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;

        memcpy(&list[index], &sMplSensorList[StepCounter],
               sizeof(struct sensor_t));
        list[index].handle = index + 1;
        mPendingEvents[StepCounter].sensor = list[index].handle;
        index++;
        if ((index - index_start) >= limit)
            return index - index_start;
    }

    if (PROCESS_VERBOSE) {
        for (i = 0; i < index; i++) {
            ALOGI("%s -------------- FUSION SENSOR %d --------------\n",
                  __func__, i);
            ALOGI("%s sensor_t[%d].name=%s",
                  __func__, i, list[i].name);
            ALOGI("%s sensor_t[%d].vendor=%s",
                  __func__, i, list[i].vendor);
            ALOGI("%s sensor_t[%d].version=%d\n",
                  __func__, i, list[i].version);
            ALOGI("%s sensor_t[%d].handle=%d\n",
                  __func__, i, list[i].handle);
            ALOGI("%s sensor_t[%d].type=%d\n",
                  __func__, i, list[i].type);
            ALOGI("%s sensor_t[%d].maxRange=%f\n",
                  __func__, i, list[i].maxRange);
            ALOGI("%s sensor_t[%d].resolution=%f\n",
                  __func__, i, list[i].resolution);
            ALOGI("%s sensor_t[%d].power=%f\n",
                  __func__, i, list[i].power);
            ALOGI("%s sensor_t[%d].minDelay=%d\n",
                  __func__, i, list[i].minDelay);
            ALOGI("%s sensor_t[%d].fifoReservedEventCount=%d\n",
                  __func__, i, list[i].fifoReservedEventCount);
            ALOGI("%s sensor_t[%d].fifoMaxEventCount=%d\n",
                  __func__, i, list[i].fifoMaxEventCount);
        }
        for (i = 0; i < MpuNumSensors; i++) {
            ALOGI("%s -------------- FUSION EVENT %d --------------\n",
                  __func__, i);
            ALOGI("%s sensors_event_t[%d].version=%d\n",
                  __func__, i, mPendingEvents[i].version);
            ALOGI("%s sensors_event_t[%d].sensor=%d\n",
                  __func__, i, mPendingEvents[i].sensor);
            ALOGI("%s sensors_event_t[%d].type=%d\n",
                  __func__, i, mPendingEvents[i].type);
        }
    }

    return index - index_start;
}

int MPLSensor::inv_init_sysfs_attributes(void)
{
    VFUNC_LOG;

    unsigned char i = 0;
    char sysfs_path[MAX_SYSFS_NAME_LEN];
    char tbuf[2];
    char *sptr;
    char **dptr;
    int num;

    memset(sysfs_path, 0, sizeof(sysfs_path));

    sysfs_names_ptr =
            (char*)malloc(sizeof(char[MAX_SYSFS_ATTRB][MAX_SYSFS_NAME_LEN]));
    sptr = sysfs_names_ptr;
    if (sptr != NULL) {
        dptr = (char**)&mpu;
        do {
            *dptr++ = sptr;
            memset(sptr, 0, sizeof(sptr));
            sptr += sizeof(char[MAX_SYSFS_NAME_LEN]);
        } while (++i < MAX_SYSFS_ATTRB);
    } else {
        ALOGE("HAL:couldn't alloc mem for sysfs paths");
        return -1;
    }

    // get proper (in absolute) IIO path & build MPU's sysfs paths
    inv_get_sysfs_path(sysfs_path);

    memcpy(mSysfsPath, sysfs_path, sizeof(sysfs_path));

    sprintf(mpu.dmp_firmware, "%s%s", mSysfsPath, "/dmp_firmware");
    sprintf(mpu.firmware_loaded, "%s%s", mSysfsPath, "/firmware_loaded");
    sprintf(mpu.dmp_on, "%s%s", mSysfsPath, "/dmp_on");
    sprintf(mpu.dmp_int_on, "%s%s", mSysfsPath, "/dmp_int_on");
    sprintf(mpu.dmp_event_int_on, "%s%s", mSysfsPath, "/dmp_event_int_on");

    sprintf(mpu.self_test, "%s%s", mSysfsPath, "/self_test");

    sprintf(mpu.gyro_fifo_rate, "%s%s", mSysfsPath, "/sampling_frequency");
    sprintf(mpu.gyro_fifo_enable, "%s%s", mSysfsPath, "/gyro_fifo_enable");
    sprintf(mpu.gyro_fifo_enable, "%s%s", mSysfsPath, "/gyro_fifo_enable");

    sprintf(mpu.accel_fifo_rate, "%s%s", mSysfsPath, "/sampling_frequency");
    sprintf(mpu.accel_fifo_enable, "%s%s", mSysfsPath, "/accel_fifo_enable");

#ifndef THIRD_PARTY_ACCEL //MPU3050
    // DMP uses these values
    sprintf(mpu.in_accel_x_dmp_bias, "%s%s", mSysfsPath, "/in_accel_x_dmp_bias");
    sprintf(mpu.in_accel_y_dmp_bias, "%s%s", mSysfsPath, "/in_accel_y_dmp_bias");
    sprintf(mpu.in_accel_z_dmp_bias, "%s%s", mSysfsPath, "/in_accel_z_dmp_bias");
#endif

    // DMP uses these bias values
    sprintf(mpu.in_gyro_x_dmp_bias, "%s%s", mSysfsPath, "/in_anglvel_x_dmp_bias");
    sprintf(mpu.in_gyro_y_dmp_bias, "%s%s", mSysfsPath, "/in_anglvel_y_dmp_bias");
    sprintf(mpu.in_gyro_z_dmp_bias, "%s%s", mSysfsPath, "/in_anglvel_z_dmp_bias");

    // MPU uses these bias values
    sprintf(mpu.three_axis_q_on, "%s%s", mSysfsPath, "/three_axes_q_on"); //formerly quaternion_on
    sprintf(mpu.three_axis_q_rate, "%s%s", mSysfsPath, "/three_axes_q_rate");

    sprintf(mpu.ped_q_on, "%s%s", mSysfsPath, "/ped_q_on");
    sprintf(mpu.ped_q_rate, "%s%s", mSysfsPath, "/ped_q_rate");

    sprintf(mpu.six_axis_q_on, "%s%s", mSysfsPath, "/six_axes_q_on");
    sprintf(mpu.six_axis_q_rate, "%s%s", mSysfsPath, "/six_axes_q_rate");

    sprintf(mpu.six_axis_q_value, "%s%s", mSysfsPath, "/six_axes_q_value");

    sprintf(mpu.step_detector_on, "%s%s", mSysfsPath, "/step_detector_on");
    sprintf(mpu.step_indicator_on, "%s%s", mSysfsPath, "/step_indicator_on");

    sprintf(mpu.event_smd, "%s%s", mSysfsPath,
            "/event_smd");
    sprintf(mpu.smd_enable, "%s%s", mSysfsPath,
            "/smd_enable");
    sprintf(mpu.smd_delay_threshold, "%s%s", mSysfsPath,
            "/smd_delay_threshold");
    sprintf(mpu.smd_delay_threshold2, "%s%s", mSysfsPath,
            "/smd_delay_threshold2");
    sprintf(mpu.smd_threshold, "%s%s", mSysfsPath,
            "/smd_threshold");
    sprintf(mpu.batchmode_timeout, "%s%s", mSysfsPath,
            "/batchmode_timeout");
    sprintf(mpu.pedometer_on, "%s%s", mSysfsPath,
            "/pedometer_on");
    sprintf(mpu.pedometer_int_on, "%s%s", mSysfsPath,
            "/pedometer_int_on");
    sprintf(mpu.event_pedometer, "%s%s", mSysfsPath,
            "/event_pedometer");
    sprintf(mpu.pedometer_steps, "%s%s", mSysfsPath,
            "/pedometer_steps");
    sprintf(mpu.pedometer_counter, "%s%s", mSysfsPath,
            "/pedometer_counter");
    return 0;
}

int MPLSensor::isLowPowerQuatEnabled(void)
{
    VFUNC_LOG;
#ifdef ENABLE_LP_QUAT_FEAT
    return mDmpLoaded;
#else
    return 0;
#endif
}

/* these functions can be consolidated
with inv_convert_to_body_with_scale */
void MPLSensor::getCompassBias()
{
    VFUNC_LOG;

    long bias[3];
    long compassBias[3];
    unsigned short orient;
    signed char orientMtx[9];

    if (mCompassSensor == NULL)
        return;

    mCompassSensor->getMatrix(0, orientMtx);
    /* disable NVS matrix once fusion has it */
    mCompassSensor->setMatrix(0, matrixDisable);
    orient = inv_orientation_matrix_to_scalar(orientMtx);

    /* Get Values from MPL */
    inv_get_compass_bias(bias);
    inv_convert_to_body(orient, bias, compassBias);
    ALOGV_IF(HANDLER_DATA, "Mpl Compass Bias (HW unit) %ld %ld %ld",
             bias[0], bias[1], bias[2]);
    ALOGV_IF(HANDLER_DATA, "Mpl Compass Bias (HW unit) (body) %ld %ld %ld",
             compassBias[0], compassBias[1], compassBias[2]);
    long compassSensitivity = inv_get_compass_sensitivity();
    if (compassSensitivity == 0) {
        compassSensitivity = mCompassScale;
    }
    for (int i = 0; i < 3; i++) {
        /* convert to uT */
        float temp = (float) compassSensitivity / (1L << 30);
        mCompassBias[i] =(float) (compassBias[i] * temp / 65536.f);
    }

    return;
}

void MPLSensor::getFactoryGyroBias()
{
    VFUNC_LOG;

    /* Get Values from MPL */
    inv_get_gyro_bias(mFactoryGyroBias);
    ALOGV_IF(ENG_VERBOSE, "Factory Gyro Bias %ld %ld %ld",
             mFactoryGyroBias[0], mFactoryGyroBias[1], mFactoryGyroBias[2]);
    mFactoryGyroBiasAvailable = true;

    return;
}

/* set bias from factory cal file to MPU offset (in chip frame)
   x = values store in cal file --> (v/1000 * 2^16 / (2000/250))
   offset = x/2^16 * (Gyro scale / self test scale used) * (-1) / offset scale
   i.e. self test default scale = 250
         gyro scale default to = 2000
         offset scale = 4 //as spec by hardware
         offset = x/2^16 * (8) * (-1) / (4)
*/
void MPLSensor::setFactoryGyroBias()
{
    VFUNC_LOG;
    bool err = false;
    int ret;

    if ((mGyroSensor == NULL) || (mFactoryGyroBiasAvailable == false))
        return;

    /* add scaling here - depends on self test parameters */
    int scaleRatio = mGyroScale / mGyroSelfTestScale;
    int offsetScale = 4;
    float tempBias;

    ALOGV_IF(ENG_VERBOSE, "%s: scaleRatio used =%d", __func__, scaleRatio);
    ALOGV_IF(ENG_VERBOSE, "%s: offsetScale used =%d", __func__, offsetScale);

    for (int i = 0; i < 3; i++) {
        tempBias = (((float)mFactoryGyroBias[i]) / 65536.f * scaleRatio) * -1 / offsetScale;
        ret = mGyroSensor->setOffset(-1, tempBias, i);
        if (ret) {
            ALOGE("%s ERR: writing gyro offset %f to channel %u",
                  __func__, tempBias, i);
            err = true;
        } else {
//            ALOGV_IF(SYSFS_VERBOSE, "%s channel %u = %f - (%lld)",
            ALOGV_IF(1, "%s channel %u = %f - (%lld)",
                     __func__, i, tempBias, getTimestamp());
        }
    }
    if (err)
        return;

    mFactoryGyroBiasAvailable = false;
    ALOGV_IF(EXTRA_VERBOSE, "%s: Factory gyro Calibrated Bias Applied",
             __func__);
}

/* these functions can be consolidated
with inv_convert_to_body_with_scale */
void MPLSensor::getGyroBias()
{
    VFUNC_LOG;

    long *temp = NULL;
    long chipBias[3];
    long bias[3];
    unsigned short orient;

    /* Get Values from MPL */
    inv_get_mpl_gyro_bias(mGyroChipBias, temp);
    orient = inv_orientation_matrix_to_scalar(mGyroOrientation);
    inv_convert_to_body(orient, mGyroChipBias, bias);
    ALOGV_IF(ENG_VERBOSE, "Mpl Gyro Bias (HW unit) %ld %ld %ld",
             mGyroChipBias[0], mGyroChipBias[1], mGyroChipBias[2]);
    ALOGV_IF(ENG_VERBOSE, "Mpl Gyro Bias (HW unit) (body) %ld %ld %ld",
             bias[0], bias[1], bias[2]);
    long gyroSensitivity = inv_get_gyro_sensitivity();
    if(gyroSensitivity == 0) {
        gyroSensitivity = mGyroScale;
    }

    /* scale and convert to rad */
    for(int i=0; i<3; i++) {
        float temp = (float) gyroSensitivity / (1L << 30);
        mGyroBias[i] = (float) (bias[i] * temp / (1<<16) / 180 * M_PI);
        if (mGyroBias[i] != 0)
            mGyroBiasAvailable = true;
    }

    return;
}

void MPLSensor::setGyroZeroBias()
{
    VFUNC_LOG;
    bool err = false;

    /* Write to Driver */
    ALOGV_IF(SYSFS_VERBOSE && INPUT_DATA, "HAL:sysfs:echo %d > %s (%lld)",
             0, mpu.in_gyro_x_dmp_bias, getTimestamp());
    if(write_attribute_sensor_continuous(gyro_x_dmp_bias_fd, 0) < 0) {
        ALOGE("HAL:Error writing to gyro_x_dmp_bias");
        err = true;
    }
    ALOGV_IF(SYSFS_VERBOSE && INPUT_DATA, "HAL:sysfs:echo %d > %s (%lld)",
             0, mpu.in_gyro_y_dmp_bias, getTimestamp());
    if(write_attribute_sensor_continuous(gyro_y_dmp_bias_fd, 0) < 0) {
        ALOGE("HAL:Error writing to gyro_y_dmp_bias");
        err = true;
    }
    ALOGV_IF(SYSFS_VERBOSE && INPUT_DATA, "HAL:sysfs:echo %d > %s (%lld)",
             0, mpu.in_gyro_z_dmp_bias, getTimestamp());
    if(write_attribute_sensor_continuous(gyro_z_dmp_bias_fd, 0) < 0) {
        ALOGE("HAL:Error writing to gyro_z_dmp_bias");
        err = true;
    }
    if (!err)
        ALOGV_IF(EXTRA_VERBOSE, "HAL:Zero Gyro DMP Calibrated Bias Applied");
}

void MPLSensor::setGyroBias()
{
    VFUNC_LOG;

    if(mGyroBiasAvailable == false)
        return;

    long bias[3];
    long gyroSensitivity = inv_get_gyro_sensitivity();

    if(gyroSensitivity == 0) {
        gyroSensitivity = mGyroScale;
    }

    inv_get_gyro_bias_dmp_units(bias);

    /* Write to Driver */
    ALOGV_IF(SYSFS_VERBOSE && INPUT_DATA, "HAL:sysfs:echo %ld > %s (%lld)",
             bias[0], mpu.in_gyro_x_dmp_bias, getTimestamp());
    if(write_attribute_sensor_continuous(gyro_x_dmp_bias_fd, bias[0]) < 0) {
        ALOGE("HAL:Error writing to gyro_x_dmp_bias");
        return;
    }
    ALOGV_IF(SYSFS_VERBOSE && INPUT_DATA, "HAL:sysfs:echo %ld > %s (%lld)",
             bias[1], mpu.in_gyro_y_dmp_bias, getTimestamp());
    if(write_attribute_sensor_continuous(gyro_y_dmp_bias_fd, bias[1]) < 0) {
        ALOGE("HAL:Error writing to gyro_y_dmp_bias");
        return;
    }
    ALOGV_IF(SYSFS_VERBOSE && INPUT_DATA, "HAL:sysfs:echo %ld > %s (%lld)",
             bias[2], mpu.in_gyro_z_dmp_bias, getTimestamp());
    if(write_attribute_sensor_continuous(gyro_z_dmp_bias_fd, bias[2]) < 0) {
        ALOGE("HAL:Error writing to gyro_z_dmp_bias");
        return;
    }
    mGyroBiasApplied = true;
    mGyroBiasAvailable = false;
    ALOGV_IF(EXTRA_VERBOSE, "HAL:Gyro DMP Calibrated Bias Applied");

    return;
}

void MPLSensor::getFactoryAccelBias()
{
    VFUNC_LOG;

    long temp;

    /* Get Values from MPL */
    inv_get_accel_bias(mFactoryAccelBias);
    ALOGV_IF(ENG_VERBOSE, "Factory Accel Bias (mg) %ld %ld %ld",
             mFactoryAccelBias[0], mFactoryAccelBias[1], mFactoryAccelBias[2]);
    mFactoryAccelBiasAvailable = true;

    return;
}

void MPLSensor::setFactoryAccelBias()
{
    VFUNC_LOG;
    bool err = false;
    unsigned int i;
    int ret;

    if ((mAccelSensor == NULL) || (mFactoryAccelBiasAvailable == false))
        return;

    /* add scaling here - depends on self test parameters */
    int scaleRatio = mAccelScale / mAccelSelfTestScale;
    int offsetScale = 16;
    float tempBias;

    ALOGV_IF(ENG_VERBOSE, "%s: scaleRatio used =%d", __func__, scaleRatio);
    ALOGV_IF(ENG_VERBOSE, "%s: offsetScale used =%d", __func__, offsetScale);

    for (i = 0; i < 3; i++) {
        tempBias = -mFactoryAccelBias[i] / 65536.f * scaleRatio / offsetScale;
        ret = mAccelSensor->setOffset(-1, tempBias, i);
        if (ret) {
            ALOGE("%s ERR: writing accel offset channel %u", __func__, i);
            err = true;
        } else {
//            ALOGV_IF(SYSFS_VERBOSE, "%s channel %u = %f - (%lld)",
            ALOGV_IF(1, "%s channel %u = %f - (%lld)",
                     __func__, i, tempBias, getTimestamp());
        }
    }
    if (err)
        return;

    mFactoryAccelBiasAvailable = false;
    ALOGV_IF(EXTRA_VERBOSE, "%s: Factory Accel Calibrated Bias Applied",
             __func__);
}

void MPLSensor::getAccelBias()
{
    VFUNC_LOG;
    long temp;

    /* Get Values from MPL */
    inv_get_mpl_accel_bias(mAccelBias, &temp);
    ALOGV_IF(ENG_VERBOSE, "Accel Bias (mg) %ld %ld %ld",
             mAccelBias[0], mAccelBias[1], mAccelBias[2]);
    mAccelBiasAvailable = true;

    return;
}

/*    set accel bias obtained from MPL
      bias is scaled by 65536 from MPL
      DMP expects: bias * 536870912 / 2^30 = bias / 2 (in body frame)
*/
void MPLSensor::setAccelBias()
{
    VFUNC_LOG;

    if(mAccelBiasAvailable == false) {
        ALOGV_IF(ENG_VERBOSE, "HAL: setAccelBias - accel bias not available");
        return;
    }

    /* write to driver */
    ALOGV_IF(SYSFS_VERBOSE && INPUT_DATA, "HAL:sysfs:echo %ld > %s (%lld)",
             (long) (mAccelBias[0] / 65536.f / 2),
             mpu.in_accel_x_dmp_bias, getTimestamp());
    if(write_attribute_sensor_continuous(
            accel_x_dmp_bias_fd, (long)(mAccelBias[0] / 65536.f / 2)) < 0) {
        ALOGE("HAL:Error writing to accel_x_dmp_bias");
        return;
    }
    ALOGV_IF(SYSFS_VERBOSE && INPUT_DATA, "HAL:sysfs:echo %ld > %s (%lld)",
             (long)(mAccelBias[1] / 65536.f / 2),
             mpu.in_accel_y_dmp_bias, getTimestamp());
    if(write_attribute_sensor_continuous(
            accel_y_dmp_bias_fd, (long)(mAccelBias[1] / 65536.f / 2)) < 0) {
        ALOGE("HAL:Error writing to accel_y_dmp_bias");
        return;
    }
    ALOGV_IF(SYSFS_VERBOSE && INPUT_DATA, "HAL:sysfs:echo %ld > %s (%lld)",
             (long)(mAccelBias[2] / 65536 / 2),
             mpu.in_accel_z_dmp_bias, getTimestamp());
    if(write_attribute_sensor_continuous(
            accel_z_dmp_bias_fd, (long)(mAccelBias[2] / 65536 / 2)) < 0) {
        ALOGE("HAL:Error writing to accel_z_dmp_bias");
        return;
    }

    mAccelBiasAvailable = false;
    mAccelBiasApplied = true;
    ALOGV_IF(EXTRA_VERBOSE, "HAL:Accel DMP Calibrated Bias Applied");

    return;
}

int MPLSensor::isCompassDisabled(void)
{
    VFUNC_LOG;
    int fd;
    int ret = 0;

    if (mCompassSensor == NULL)
        return 1;

    fd = mCompassSensor->getFd();
    if (fd < 0) {
        ALOGI_IF(EXTRA_VERBOSE, "HAL: Compass is disabled, Six-axis Sensor Fusion is used.");
        ret = 1;
    }
    return ret;
}

int MPLSensor::checkBatchEnabled(void)
{
    VFUNC_LOG;
    return ((mFeatureActiveMask & INV_DMP_BATCH_MODE)? 1:0);
}

/* precondition: framework disallows this case, ie enable continuous sensor, */
/* and enable batch sensor */
/* if one sensor is in continuous mode, HAL disallows enabling batch for this sensor */
/* or any other sensors */
int MPLSensor::batch(int handle, int flags, int64_t period_ns, int64_t timeout)
{
    VFUNC_LOG;
    android::String8 sname;
    int what = -1;

    getHandle(handle, what, sname);
    if (uint32_t(what) >= MpuNumSensors) {
        ALOGE("HAL:flush - what=%d is invalid", what);
        return -EINVAL;
    }

    switch (what) {
    case Accelerometer:
        if (mAccelSensor != NULL)
            return mAccelSensor->batch(handle, flags, period_ns, timeout);
        return -EINVAL;

    case Gyro:
    case RawGyro:
        if (mGyroSensor != NULL)
            return mGyroSensor->batch(handle, flags, period_ns, timeout);
        return -EINVAL;

    case MagneticField:
    case RawMagneticField:
        if (mCompassSensor != NULL)
            return mCompassSensor->batch(handle, flags, period_ns, timeout);
        return -EINVAL;

    case Pressure:
        if (mPressureSensor != NULL)
            return mPressureSensor->batch(handle, flags, period_ns, timeout);
        return -EINVAL;

    default:
        break;
    }

    int res = 0;

    if (!mDmpLoaded)
        return res;

    /* Enables batch mode and sets timeout for the given sensor */
    /* enum SENSORS_BATCH_DRY_RUN, SENSORS_BATCH_WAKE_UPON_FIFO_FULL */
    bool dryRun = false;
    int enabled_sensors = mEnabled;
    int batchMode = timeout > 0 ? 1 : 0;

    ALOGI_IF(DEBUG_BATCHING || ENG_VERBOSE,
             "HAL:batch called - handle=%d, flags=%d, period=%lld, timeout=%lld",
             handle, flags, period_ns, timeout);

    if(flags & SENSORS_BATCH_DRY_RUN) {
        dryRun = true;
        ALOGI_IF(PROCESS_VERBOSE,
                 "HAL:batch - dry run mode is set (%d)", SENSORS_BATCH_DRY_RUN);
    }

    /* check if we can support issuing interrupt before FIFO fills-up */
    /* in a given timeout.                                          */
    if (flags & SENSORS_BATCH_WAKE_UPON_FIFO_FULL) {
            ALOGE("HAL: batch SENSORS_BATCH_WAKE_UPON_FIFO_FULL is not supported");
            return -EINVAL;
    }

    ALOGV_IF(PROCESS_VERBOSE,
             "HAL:batch : %llu ns, (%.2f Hz)", period_ns, 1000000000.f / period_ns);

    // limit all rates to reasonable ones */
    if (period_ns < 5000000LL) {
        period_ns = 5000000LL;
    }

    switch (what) {
    case GameRotationVector:
    case StepDetector:
        ALOGV_IF(PROCESS_VERBOSE, "HAL: batch - select sensor (handle %d)", handle);
        break;
    default:
        if (timeout > 0) {
            ALOGE("sensor (handle %d) is not supported in batch mode", handle);
            return -EINVAL;
        }
    }

    if(dryRun == true) {
        ALOGI("HAL: batch Dry Run is complete");
        return 0;
    }

    int tempBatch = 0;
    if (timeout > 0) {
        tempBatch = mBatchEnabled | (1 << what);
    } else {
        tempBatch = mBatchEnabled & ~(1 << what);
    }

    if (!computeBatchSensorMask(mEnabled, tempBatch)) {
        batchMode = 0;
    } else {
        batchMode = 1;
    }

    /* get maximum possible bytes to batch per sample */
    /* get minimum delay for each requested sensor    */
    ssize_t nBytes = 0;
    int64_t wanted = 1000000000LL, ns = 0;
    int64_t timeoutInMs = 0;
    for (int i = 0; i < MpuNumSensors; i++) {
        if (batchMode == 1) {
            ns = mBatchDelays[i];
            ALOGV_IF(DEBUG_BATCHING && EXTRA_VERBOSE,
                     "HAL:batch - requested sensor=0x%01x, batch delay=%lld", mEnabled & (1 << i), ns);
            // take the min delay ==> max rate
            wanted = (ns < wanted) ? ns : wanted;
            if (i <= RawMagneticField) {
                nBytes += 8;
            }
            if (i == Pressure) {
                nBytes += 6;
            }
            if ((i == StepDetector) || (i == GameRotationVector)) {
                nBytes += 16;
            }
        }
    }

    /* starting from code below,  we will modify hardware */
    /* first edit global batch mode mask */

    if (!timeout) {
        mBatchEnabled &= ~(1 << what);
        mBatchDelays[what] = 1000000000L;
        mDelays[what] = period_ns;
        mBatchTimeouts[what] = 100000000000LL;
    } else {
        mBatchEnabled |= (1 << what);
        mBatchDelays[what] = period_ns;
        mDelays[what] = period_ns;
        mBatchTimeouts[what] = timeout;
    }

    if(((int)mOldBatchEnabledMask != batchMode) || batchMode) {
        /* remember batch mode that is set  */
        mOldBatchEnabledMask = batchMode;
        /* For these sensors, switch to different data output */
        int featureMask = computeBatchDataOutput();
        ALOGV_IF(ENG_VERBOSE, "batchMode =%d, featureMask=0x%x, mEnabled=%d",
                 batchMode, featureMask, mEnabled);
        if (DEBUG_BATCHING && EXTRA_VERBOSE) {
            ALOGV("HAL:batch - sensor=0x%01x", mBatchEnabled);
            for (int d = 0; d < MpuNumSensors; d++) {
                ALOGV("HAL:batch - sensor status=0x%01x batch status=0x%01x timeout=%lld delay=%lld",
                      mEnabled & (1 << d), (mBatchEnabled & (1 << d)), mBatchTimeouts[d],
                      mBatchDelays[d]);
            }
        }
        /* take the minimum batchmode timeout */
        if (batchMode == 1) {
            int64_t tempTimeout = 100000000000LL;
            for (int i = 0; i < MpuNumSensors; i++) {
                if ((mEnabled & (1 << i) && mBatchEnabled & (1 << i)) ||
                        (((featureMask & INV_DMP_PED_STANDALONE) && (mBatchEnabled & (1 << StepDetector))))) {
                    ALOGV_IF(ENG_VERBOSE, "i=%d, timeout=%lld", i, mBatchTimeouts[i]);
                    ns = mBatchTimeouts[i];
                    tempTimeout = (ns < tempTimeout) ? ns : tempTimeout;
                }
            }
            timeout = tempTimeout;
            /* Convert ns to millisecond */
            timeoutInMs = timeout / 1000000;

            /* remember last timeout value */
            mBatchTimeoutInMs = timeoutInMs;
        } else {
            timeoutInMs = 0;
        }
        ALOGV_IF(DEBUG_BATCHING || EXTRA_VERBOSE,
                 "HAL:batch - timeout - timeout=%lld ns, timeoutInMs=%lld, delay=%lld ns",
                 timeout, timeoutInMs, period_ns);
        /* case for Ped standalone */
        if ((batchMode == 1) && (featureMask & INV_DMP_PED_STANDALONE) &&
               (mFeatureActiveMask & INV_DMP_PEDOMETER)) {
            ALOGI("ID_SD only = 0x%x", mBatchEnabled);
            enablePedQuaternion(0);
            enablePedStandalone(1);
        } else {
            enablePedStandalone(0);
            if (featureMask & INV_DMP_PED_QUATERNION) {
                enableLPQuaternion(0);
                enablePedQuaternion(1);
            }
        }
        /* case for Ped Quaternion */
        if ((batchMode == 1) && (featureMask & INV_DMP_PED_QUATERNION) &&
                (mEnabled & (1 << GameRotationVector)) &&
                (mFeatureActiveMask & INV_DMP_PEDOMETER)) {
            ALOGI("ID_SD and GRV or ALL = 0x%x", mBatchEnabled);
            ALOGI("ID_SD is enabled for batching, PED quat will be automatically enabled");
            enableLPQuaternion(0);
            enablePedQuaternion(1);
            wanted = mBatchDelays[GameRotationVector];
            /* set pedq rate */
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     int(1000000000.f / wanted), mpu.ped_q_rate,
                     getTimestamp());
            write_sysfs_int(mpu.ped_q_rate, 1000000000.f / wanted);
            ALOGV_IF(PROCESS_VERBOSE,
                     "HAL:DMP ped quaternion rate %.2f Hz", 1000000000.f / wanted);
        } else if (!(featureMask & INV_DMP_PED_STANDALONE)){
            ALOGV_IF(ENG_VERBOSE, "batch - PedQ Toggle back to normal 6 axis");
            if (mEnabled & (1 << GameRotationVector)) {
                enableLPQuaternion(checkLPQRateSupported());
            }
            enablePedQuaternion(0);
        } else {
            enablePedQuaternion(0);
        }
         /* case for Ped indicator */
        if ((batchMode == 1) && ((featureMask & INV_DMP_PED_INDICATOR))) {
            enablePedIndicator(1);
        } else {
            enablePedIndicator(0);
        }
        /* case for Six Axis Quaternion */
        if ((batchMode == 1) && (featureMask & INV_DMP_6AXIS_QUATERNION) &&
                (mEnabled & (1 << GameRotationVector))) {
            ALOGI("GRV = 0x%x", mBatchEnabled);
            enableLPQuaternion(0);
            enable6AxisQuaternion(1);
            if (what == GameRotationVector) {
                setInitial6QuatValue();
            }
            wanted = mBatchDelays[GameRotationVector];
            /* set sixaxis rate */
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     int(1000000000.f / wanted), mpu.six_axis_q_rate,
                     getTimestamp());
            write_sysfs_int(mpu.six_axis_q_rate, 1000000000.f / wanted);
            ALOGV_IF(PROCESS_VERBOSE,
                     "HAL:DMP six axis rate %.2f Hz", 1000000000.f / wanted);
        } else if (!(featureMask & INV_DMP_PED_QUATERNION)){
            ALOGV_IF(ENG_VERBOSE, "batch - 6Axis Toggle back to normal 6 axis");
            if (mEnabled & (1 << GameRotationVector)) {
                enableLPQuaternion(checkLPQRateSupported());
            }
            enable6AxisQuaternion(0);
        } else {
            enable6AxisQuaternion(0);
        }
        /* TODO: This may make a come back some day */
        /* Not to overflow hardware FIFO if flag is set */
        /*if (flags & (1 << SENSORS_BATCH_WAKE_UPON_FIFO_FULL)) {
            ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                     0, mpu.batchmode_wake_fifo_full_on, getTimestamp());
            if (write_sysfs_int(mpu.batchmode_wake_fifo_full_on, 0) < 0) {
                ALOGE("HAL:ERR can't write batchmode_wake_fifo_full_on");
            }
        }*/
        /* write required timeout to sysfs */
        ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %lld > %s (%lld)",
                 timeoutInMs, mpu.batchmode_timeout, getTimestamp());
        if (write_sysfs_int(mpu.batchmode_timeout, timeoutInMs) < 0) {
            ALOGE("HAL:ERR can't write batchmode_timeout");
        }
        if (computeAndSetDmpState() < 0) {
            ALOGE("HAL:ERR can't compute dmp state");
        }
    }//end of batch mode modify

    if (batchMode == 1) {
        /* set batch rates */
        if (setBatchDataRates() < 0) {
            ALOGE("HAL:ERR can't set batch data rates");
        }
    } else {
        /* reset sensor rate */
        if (resetDataRates() < 0) {
            ALOGE("HAL:ERR can't reset output rate back to original setting");
        }
    }

    // set sensor data interrupt
    uint32_t dataInterrupt = (mEnabled || (mFeatureActiveMask & INV_DMP_BATCH_MODE));
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
             !dataInterrupt, mpu.dmp_event_int_on, getTimestamp());
    if (write_sysfs_int(mpu.dmp_event_int_on, !dataInterrupt) < 0) {
        res = -1;
        ALOGE("HAL:ERR can't enable DMP event interrupt");
    }

    return res;
}

int MPLSensor::flush(int handle)
{
    /* There are 4 possible ways to handle the flush:
     * 1. The device is disabled: return -EINVAL
     * 2. The device doesn't support batch: send a meta event at the next
     *    opportunity.
     * 3. The device is a virtual sensor:
     *    A. Send a flush to the physical sensor.
     *    B. When the physical sensor returns the meta event, intercept and
     *       send the flush corresponding to the virtual sensor.
     * 4. The device is a physical sensor:
     *    A. Send a flush to the physical sensor.
     *    B. Pass the physical sensor meta event upward.
     * Because of these possibilities there are 3 flag variables:
     * 1. mFlushSensor[MpuNumSensors]: physical sensors needed for each sensor
     * 2. mFlushPSensor: physical Sensors when flushed
     * 3. mFlush: pending sensor meta events
     */
    VFUNC_LOG;
    android::String8 sname;
    int what = -1;
    Nvs *sensor = NULL;
    unsigned int flags = 0;
    int ret;

    getHandle(handle, what, sname);
    if (uint32_t(what) >= MpuNumSensors) {
        ALOGE("HAL:flush - what=%d is invalid", what);
        return -EINVAL;
    }

    if (!(mEnabled & (1 << what)))
        /* return -EINVAL if not enabled */
        return -EINVAL;

    if (!(mSensorList[handle - 1].fifoMaxEventCount)) {
        /* if batch mode isn't supported then we'll be sending a
         * meta event without a physical flush.
         */
        mFlush |= (1 << what);
        return 0;
    }

    /* get the physical sensor that needs the flush */
    switch (what) {
    case Gyro:
    case RawGyro:
        flags = (1 << Gyro);
        break;

    case Accelerometer:
        flags = (1 << Accelerometer);
        break;

    case MagneticField:
    case RawMagneticField:
        flags = (1 << MagneticField);
        break;

    case Pressure:
        flags = (1 << Pressure);
        break;

    case Orientation:
    case RotationVector:
    case LinearAccel:
    case Gravity:
        flags = ((1 << Accelerometer) | (1 << Gyro) | (1 << MagneticField));
        break;

    case GameRotationVector:
        flags = ((1 << Accelerometer) | (1 << Gyro));
        break;

    case GeomagneticRotationVector:
        flags = ((1 << Accelerometer) | (1 << MagneticField));
        break;

    default:
        break;
    }

    mFlushSensor[what] = flags;
    if (mAccelSensor && (flags & (1 << Accelerometer))) {
        ret = mAccelSensor->flush(handle);
        if (ret)
            mFlushSensor[what] &= ~(1 << Accelerometer);
    }
    if (mGyroSensor && (flags & (1 << Gyro))) {
        ret = mGyroSensor->flush(handle);
        if (ret)
            mFlushSensor[what] &= ~(1 << Gyro);
    }
    if (mCompassSensor && (flags & (1 << MagneticField))) {
        ret = mCompassSensor->flush(handle);
        if (ret)
            mFlushSensor[what] &= ~(1 << MagneticField);
    }
    if (mPressureSensor && (flags & (1 << Pressure))) {
        ret = mPressureSensor->flush(handle);
        if (ret)
            mFlushSensor[what] &= ~(1 << Pressure);
    }
    if (!mFlushSensor[what])
        /* if physical sensor(s) flush failed then just send meta data */
        mFlush |= (1 << what);
//FIXME: consider returning -EINVAL instead

    ALOGI("%s handle=%d what=%d mFlushSensor[what]=%u mFlush=%u mFlushPSensor=%u\n",
          __func__, handle, what, mFlushSensor[what], mFlush, mFlushPSensor);

    return 0;
}

int MPLSensor::selectAndSetQuaternion(int batchMode, int mEnabled, long long featureMask)
{
    VFUNC_LOG;
    int res = 0;

    int64_t wanted;

    /* case for Ped Quaternion */
    if (batchMode == 1) {
        if ((featureMask & INV_DMP_PED_QUATERNION) &&
            (mEnabled & (1 << GameRotationVector)) &&
            (mFeatureActiveMask & INV_DMP_PEDOMETER)) {
                enableLPQuaternion(0);
                enable6AxisQuaternion(0);
                setInitial6QuatValue();
                enablePedQuaternion(1);

                wanted = mBatchDelays[GameRotationVector];
                /* set pedq rate */
                ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                         int(1000000000.f / wanted), mpu.ped_q_rate,
                         getTimestamp());
                write_sysfs_int(mpu.ped_q_rate, 1000000000.f / wanted);
                ALOGV_IF(PROCESS_VERBOSE,
                         "HAL:DMP ped quaternion rate %.2f Hz", 1000000000.f / wanted);
        } else if ((featureMask & INV_DMP_6AXIS_QUATERNION) &&
                   (mEnabled & (1 << GameRotationVector))) {
                enableLPQuaternion(0);
                enablePedQuaternion(0);
                setInitial6QuatValue();
                enable6AxisQuaternion(1);

                wanted = mBatchDelays[GameRotationVector];
                /* set sixaxis rate */
                ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                         int(1000000000.f / wanted), mpu.six_axis_q_rate,
                         getTimestamp());
                write_sysfs_int(mpu.six_axis_q_rate, 1000000000.f / wanted);
                ALOGV_IF(PROCESS_VERBOSE,
                         "HAL:DMP six axis rate %.2f Hz", 1000000000.f / wanted);
        } else {
            enablePedQuaternion(0);
            enable6AxisQuaternion(0);
        }
    } else {
        if(mEnabled & (1 << GameRotationVector)) {
            enablePedQuaternion(0);
            enable6AxisQuaternion(0);
            enableLPQuaternion(checkLPQRateSupported());
        }
        else {
            enablePedQuaternion(0);
            enable6AxisQuaternion(0);
        }
    }

    return res;
}

/*
Select Quaternion and Options for Batching

    ID_SD    ID_GRV     HW Batch     Type
a   1       1          1            PedQ, Ped Indicator, HW
b   1       1          0            PedQ
c   1       0          1            Ped Indicator, HW
d   1       0          0            Ped Standalone, Ped Indicator
e   0       1          1            6Q, HW
f   0       1          0            6Q
g   0       0          1            HW
h   0       0          0            LPQ <defualt>
*/
int MPLSensor::computeBatchDataOutput()
{
    VFUNC_LOG;

    int featureMask = 0;
    if (mBatchEnabled == 0)
        return 0;//h

    uint32_t hardwareSensorMask = (1 << Gyro)
                                | (1 << RawGyro)
                                | (1 << Accelerometer)
                                | (1 << MagneticField)
                                | (1 << RawMagneticField)
                                | (1 << Pressure);
    ALOGV_IF(ENG_VERBOSE, "hardwareSensorMask = 0x%0x, mBatchEnabled = 0x%0x",
             hardwareSensorMask, mBatchEnabled);

    if (mBatchEnabled & (1 << StepDetector)) {
        if (mBatchEnabled & (1 << GameRotationVector)) {
            if ((mBatchEnabled & hardwareSensorMask)) {
                featureMask |= INV_DMP_6AXIS_QUATERNION;//a
                featureMask |= INV_DMP_PED_INDICATOR;
//LOGE("batch output: a");
            } else {
                featureMask |= INV_DMP_PED_QUATERNION;  //b
                featureMask |= INV_DMP_PED_INDICATOR;   //always piggy back a bit
//LOGE("batch output: b");
            }
        } else {
            if (mBatchEnabled & hardwareSensorMask) {
                featureMask |= INV_DMP_PED_INDICATOR;   //c
//LOGE("batch output: c");
            } else {
                featureMask |= INV_DMP_PED_STANDALONE;  //d
                featureMask |= INV_DMP_PED_INDICATOR;   //required for standalone
//LOGE("batch output: d");
            }
        }
    } else if (mBatchEnabled & (1 << GameRotationVector)) {
        featureMask |= INV_DMP_6AXIS_QUATERNION;        //e,f
//LOGE("batch output: e,f");
    } else {
        ALOGV_IF(ENG_VERBOSE,
                 "HAL:computeBatchDataOutput: featuerMask=0x%x", featureMask);
//LOGE("batch output: g");
        return 0; //g
    }

    ALOGV_IF(ENG_VERBOSE,
             "HAL:computeBatchDataOutput: featuerMask=0x%x", featureMask);
    return featureMask;
}

int MPLSensor::getDmpPedometerFd()
{
    VFUNC_LOG;
    ALOGV_IF(EXTRA_VERBOSE, "getDmpPedometerFd returning %d", dmp_pedometer_fd);
    return dmp_pedometer_fd;
}

/* @param [in] : outputType = 1 --event is from PED_Q        */
/*               outputType = 0 --event is from ID_SC, ID_SD  */
int MPLSensor::readDmpPedometerEvents(sensors_event_t* data, int count,
                                      int32_t id, int outputType)
{
    VFUNC_LOG;

    int res = 0;
    char dummy[4];

    int numEventReceived = 0;
    int update = 0;

    ALOGI_IF(0, "HAL: Read Pedometer Event ID=%d", id);
    switch (id) {
    case StepDetector:
        if (mDmpPedometerEnabled && count > 0) {
            /* Handles return event */
            ALOGI("HAL: Step detected");
            update = sdHandler(&mPendingEvents[StepDetector]);
        }

        if (update && count > 0) {
            *data++ = mPendingEvents[StepDetector];
            count--;
            numEventReceived++;
        }
        break;
    case StepCounter:
        FILE *fp;
        uint64_t stepCount;
        uint64_t stepCountTs;

        if (mDmpStepCountEnabled && count > 0) {
            fp = fopen(mpu.pedometer_steps, "r");
            if (fp == NULL) {
                ALOGE("HAL:cannot open pedometer_steps");
            } else {
                if (fscanf(fp, "%lld\n", &stepCount) < 0 || fclose(fp) < 0) {
                    ALOGE("HAL:cannot read pedometer_steps");
                    return 0;
                }
            }

            /* return event onChange only */
            if (stepCount == mLastStepCount) {
                return 0;
            }

            mLastStepCount = stepCount;

            /* Read step count timestamp */
            fp = fopen(mpu.pedometer_counter, "r");
            if (fp == NULL) {
                ALOGE("HAL:cannot open pedometer_counter");
            } else{
                if (fscanf(fp, "%lld\n", &stepCountTs) < 0 || fclose(fp) < 0) {
                    ALOGE("HAL:cannot read pedometer_counter");
                    return 0;
                }
            }
            mPendingEvents[StepCounter].timestamp = stepCountTs;

            /* Handles return event */
            update = scHandler(&mPendingEvents[StepCounter]);
        }

        if (update && count > 0) {
            *data++ = mPendingEvents[StepCounter];
            count--;
            numEventReceived++;
        }
        break;
    }

    if (!outputType) {
        // read dummy data per driver's request
        // only required if actual irq is issued
        read(dmp_pedometer_fd, dummy, 4);
    } else {
        return 1;
    }

    return numEventReceived;
}

int MPLSensor::readDmpSignificantMotionEvents(sensors_event_t* data, int count)
{
    VFUNC_LOG;

    int res = 0;
    char dummy[4];
    int significantMotion;
    FILE *fp;
    int sensors = mEnabled;
    int numEventReceived = 0;
    int update = 0;

    /* Technically this step is not necessary for now  */
    /* In the future, we may have meaningful values */
    fp = fopen(mpu.event_smd, "r");
    if (fp == NULL) {
        ALOGE("HAL:cannot open event_smd");
        return 0;
    } else {
        if (fscanf(fp, "%d\n", &significantMotion) < 0 || fclose(fp) < 0) {
            ALOGE("HAL:cannot read event_smd");
        }
    }

    if(mDmpSignificantMotionEnabled && count > 0) {
       /* By implementation, smd is disabled once an event is triggered */
        sensors_event_t temp;

        /* Handles return event */
        ALOGI("HAL: SMD detected");
        int update = smHandler(&mPendingEvents[SignificantMotion]);
        if (update && count > 0) {
            *data++ = mPendingEvents[SignificantMotion];
            count--;
            numEventReceived++;

            /* reset smd state */
            mDmpSignificantMotionEnabled = 0;
            mFeatureActiveMask &= ~INV_DMP_SIGNIFICANT_MOTION;
        }
    }

    // read dummy data per driver's request
    read(dmp_sign_motion_fd, dummy, 4);

    return numEventReceived;
}

int MPLSensor::enableDmpSignificantMotion(int en)
{
    VFUNC_LOG;

    int res = 0;
    int enabled_sensors = mEnabled;

    //Toggle significant montion detection
    if(en) {
        ALOGV_IF(ENG_VERBOSE, "HAL:Enabling Significant Motion");
        ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                 1, mpu.smd_enable, getTimestamp());
        if (write_sysfs_int(mpu.smd_enable, 1) < 0) {
            ALOGE("HAL:ERR can't write DMP smd_enable");
            res = -1;   //Indicate an err
        }
        mFeatureActiveMask |= INV_DMP_SIGNIFICANT_MOTION;
    }
    else {
        ALOGV_IF(ENG_VERBOSE, "HAL:Disabling Significant Motion");
        ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                 0, mpu.smd_enable, getTimestamp());
        if (write_sysfs_int(mpu.smd_enable, 0) < 0) {
            ALOGE("HAL:ERR write DMP smd_enable");
        }
        mFeatureActiveMask &= ~INV_DMP_SIGNIFICANT_MOTION;
    }

    if ((res = setDmpFeature(en)) < 0)
        return res;

    if ((res = computeAndSetDmpState()) < 0)
        return res;

    return res;
}

void MPLSensor::setInitial6QuatValue()
{
    VFUNC_LOG;

    if (!mInitial6QuatValueAvailable)
        return;

    /* convert to unsigned char array */
    size_t length = 16;
    unsigned char quat[16];
    convert_long_to_hex_char(mInitial6QuatValue, quat, 4);

    /* write to sysfs */
    ALOGV_IF(EXTRA_VERBOSE, "HAL:six axis q value: %s", mpu.six_axis_q_value);
    FILE* fptr = fopen(mpu.six_axis_q_value, "w");
    if(fptr == NULL) {
        ALOGE("HAL:could not open six_axis_q_value");
    } else {
        if (fwrite(quat, 1, length, fptr) != length || fclose(fptr) < 0) {
           ALOGE("HAL:write six axis q value failed");
        } else {
            mInitial6QuatValueAvailable = 0;
        }
    }

    return;
}
int MPLSensor::writeSignificantMotionParams(bool toggleEnable,
                                            uint32_t delayThreshold1,
                                            uint32_t delayThreshold2,
                                            uint32_t motionThreshold)
{
    VFUNC_LOG;
    int res = 0;

    // Write supplied values
    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
             delayThreshold1, mpu.smd_delay_threshold, getTimestamp());
    res = write_sysfs_int(mpu.smd_delay_threshold, delayThreshold1);
    if (res == 0) {
        ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                 delayThreshold2, mpu.smd_delay_threshold2, getTimestamp());
        res = write_sysfs_int(mpu.smd_delay_threshold2, delayThreshold2);
    }
    if (res == 0) {
        ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %d > %s (%lld)",
                 motionThreshold, mpu.smd_threshold, getTimestamp());
        res = write_sysfs_int(mpu.smd_threshold, motionThreshold);
    }
    return res;
}

/* set batch data rate */
/* this function should be optimized */
int MPLSensor::setBatchDataRates()
{
    VFUNC_LOG;

    int res = 0;
    int tempFd = -1;

    int64_t gyroRate;
    int64_t accelRate;
    int64_t compassRate;
    int64_t pressureRate;
    int64_t quatRate;

    int mplGyroRate;
    int mplAccelRate;
    int mplCompassRate;
    int mplQuatRate;

#ifdef ENABLE_MULTI_RATE
    gyroRate = mBatchDelays[Gyro];
    /* take care of case where only one type of gyro sensors or
       compass sensors is turned on */
    if (mBatchEnabled & (1 << Gyro) || mBatchEnabled & (1 << RawGyro)) {
        gyroRate = (mBatchDelays[Gyro] <= mBatchDelays[RawGyro]) ?
            (mBatchEnabled & (1 << Gyro) ? mBatchDelays[Gyro] : mBatchDelays[RawGyro]):
            (mBatchEnabled & (1 << RawGyro) ? mBatchDelays[RawGyro] : mBatchDelays[Gyro]);
    }
    compassRate = mBatchDelays[MagneticField];
    if (mBatchEnabled & (1 << MagneticField) || mBatchEnabled & (1 << RawMagneticField)) {
        compassRate = (mBatchDelays[MagneticField] <= mBatchDelays[RawMagneticField]) ?
                (mBatchEnabled & (1 << MagneticField) ? mBatchDelays[MagneticField] :
                    mBatchDelays[RawMagneticField]) :
                (mBatchEnabled & (1 << RawMagneticField) ? mBatchDelays[RawMagneticField] :
                    mBatchDelays[MagneticField]);
    }
    accelRate = mBatchDelays[Accelerometer];
    pressureRate = mBatchDelays[Pressure];

    if ((mFeatureActiveMask & INV_DMP_PED_QUATERNION) ||
            (mFeatureActiveMask & INV_DMP_6AXIS_QUATERNION)) {
        quatRate = mBatchDelays[GameRotationVector];
        mplQuatRate = (int) quatRate / 1000LL;
        inv_set_quat_sample_rate(mplQuatRate);
        inv_set_rotation_vector_6_axis_sample_rate(mplQuatRate);
        ALOGV_IF(PROCESS_VERBOSE,
                 "HAL:MPL rv 6 axis sample rate: (mpl)=%d us (mpu)=%.2f Hz",
                 mplQuatRate, 1000000000.f / quatRate );
        ALOGV_IF(PROCESS_VERBOSE,
                 "HAL:MPL quat sample rate: (mpl)=%d us (mpu)=%.2f Hz",
                 mplQuatRate, 1000000000.f / quatRate );
        getDmpRate(&quatRate);
    }

    mplGyroRate = (int) gyroRate / 1000LL;
    mplAccelRate = (int) accelRate / 1000LL;
    mplCompassRate = (int) compassRate / 1000LL;

     /* set rate in MPL */
     /* compass can only do 100Hz max */
    inv_set_gyro_sample_rate(mplGyroRate);
    inv_set_accel_sample_rate(mplAccelRate);
    inv_set_compass_sample_rate(mplCompassRate);

    ALOGV_IF(PROCESS_VERBOSE,
             "HAL:MPL gyro sample rate: (mpl)=%d us (mpu)=%.2f Hz",
             mplGyroRate, 1000000000.f / gyroRate);
    ALOGV_IF(PROCESS_VERBOSE,
             "HAL:MPL accel sample rate: (mpl)=%d us (mpu)=%.2f Hz",
             mplAccelRate, 1000000000.f / accelRate);
    ALOGV_IF(PROCESS_VERBOSE,
             "HAL:MPL compass sample rate: (mpl)=%d us (mpu)=%.2f Hz",
             mplCompassRate, 1000000000.f / compassRate);

#else
    /* search the minimum delay requested across all enabled sensors */
    int64_t wanted = 1000000000LL;
    for (int i = 0; i < MpuNumSensors; i++) {
        if (mBatchEnabled & (1 << i)) {
            int64_t ns = mBatchDelays[i];
            wanted = wanted < ns ? wanted : ns;
        }
    }
    gyroRate = wanted;
    accelRate = wanted;
    compassRate = wanted;
    pressureRate = wanted;
#endif

    /* takes care of gyro rate */
    if (mGyroSensor != NULL) {
        ALOGV_IF(SYSFS_VERBOSE, "%s gyro rate = %.0f (%lld)",
                 __func__, 1000000000.f / gyroRate, getTimestamp());
        res = mGyroSensor->setDelay(0, gyroRate);
        if(res < 0)
            ALOGE("HAL:GYRO update delay error");
    }

    /* takes care of accel rate */
    if (mAccelSensor != NULL) {
        ALOGV_IF(SYSFS_VERBOSE, "%s accel rate = %.0f (%lld)",
                 __func__, 1000000000.f / accelRate, getTimestamp());
        res = mAccelSensor->setDelay(0, accelRate);
        if(res < 0)
            ALOGE("HAL:ACCEL update delay error");
    }

    /* takes care of compass rate */
    if (mCompassSensor != NULL) {
        if (compassRate < mCompassSensorList->minDelay * 1000LL)
            compassRate = mCompassSensorList->minDelay * 1000LL;
        mCompassSensor->setDelay(mCompassSensorList->handle, compassRate);
    }

    /* takes care of pressure rate */
    if (mPressureSensor != NULL)
        mPressureSensor->setDelay(-1, pressureRate);

    return res;
}

/* Set sensor rate */
/* this function should be optimized */
int MPLSensor::resetDataRates()
{
    VFUNC_LOG;

    int res = 0;
    int tempFd = -1;
    int64_t wanted = 1000000000LL;

    int64_t resetRate;
    int64_t gyroRate;
    int64_t accelRate;
    int64_t compassRate;
    int64_t pressureRate;

    if (!mEnabled) {
        ALOGV_IF(ENG_VERBOSE, "skip resetDataRates");
        return 0;
    }
    ALOGI("HAL:resetDataRates mEnabled=%d", mEnabled);
    /* search the minimum delay requested across all enabled sensors */
    /* skip setting rates if it is not changed */
    for (int i = 0; i < MpuNumSensors; i++) {
        if (mEnabled & (1 << i)) {
            int64_t ns = mDelays[i];
            if ((wanted == ns) && (i != Pressure)) {
                ALOGV_IF(ENG_VERBOSE, "skip resetDataRates : same delay mDelays[%d]=%lld", i,mDelays[i]);
                //return 0;
            }
            ALOGV_IF(ENG_VERBOSE, "resetDataRates - mDelays[%d]=%lld", i, mDelays[i]);
            wanted = wanted < ns ? wanted : ns;
        }
    }

    resetRate = wanted;
    gyroRate = wanted;
    accelRate = wanted;
    compassRate = wanted;
    pressureRate = wanted;

    /* set mpl data rate */
   inv_set_gyro_sample_rate((int)gyroRate/1000LL);
   inv_set_accel_sample_rate((int)accelRate/1000LL);
   inv_set_compass_sample_rate((int)compassRate/1000LL);
   inv_set_linear_acceleration_sample_rate((int)resetRate/1000LL);
   inv_set_orientation_sample_rate((int)resetRate/1000LL);
   inv_set_rotation_vector_sample_rate((int)resetRate/1000LL);
   inv_set_gravity_sample_rate((int)resetRate/1000LL);
   inv_set_orientation_geomagnetic_sample_rate((int)resetRate/1000LL);
   inv_set_rotation_vector_6_axis_sample_rate((int)resetRate/1000LL);
   inv_set_geomagnetic_rotation_vector_sample_rate((int)resetRate/1000LL);

    ALOGV_IF(PROCESS_VERBOSE,
             "HAL:MPL gyro sample rate: (mpl)=%lld us (mpu)=%.2f Hz",
             gyroRate/1000LL, 1000000000.f / gyroRate);
    ALOGV_IF(PROCESS_VERBOSE,
             "HAL:MPL accel sample rate: (mpl)=%lld us (mpu)=%.2f Hz",
             accelRate/1000LL, 1000000000.f / accelRate);
    ALOGV_IF(PROCESS_VERBOSE,
             "HAL:MPL compass sample rate: (mpl)=%lld us (mpu)=%.2f Hz",
             compassRate/1000LL, 1000000000.f / compassRate);

    /* reset dmp rate */
    getDmpRate (&wanted);

    ALOGV_IF(SYSFS_VERBOSE, "HAL:sysfs:echo %.0f > %s (%lld)",
             1000000000.f / wanted, mpu.gyro_fifo_rate,
             getTimestamp());
    tempFd = open(mpu.gyro_fifo_rate, O_RDWR);
    res = write_attribute_sensor(tempFd, 1000000000.f / wanted);
    ALOGE_IF(res < 0, "HAL:sampling frequency update delay error");

    /* takes care of gyro rate */
    if (mGyroSensor != NULL) {
        ALOGV_IF(SYSFS_VERBOSE, "%s gyro rate = %.0f (%lld)",
                 __func__, 1000000000.f / gyroRate, getTimestamp());
        res = mGyroSensor->setDelay(mGyroSensorList->handle, gyroRate);
        if(res < 0)
            ALOGE("HAL:GYRO update delay error");
    }

    /* takes care of accel rate */
    if (mAccelSensor != NULL) {
        ALOGV_IF(SYSFS_VERBOSE, "%s accel rate = %.0f (%lld)",
                 __func__, 1000000000.f / accelRate, getTimestamp());
        res = mAccelSensor->setDelay(mAccelSensorList->handle, accelRate);
        if(res < 0)
            ALOGE("HAL:ACCEL update delay error");
    }

    /* takes care of compass rate */
    if (mCompassSensor != NULL) {
        if (compassRate < mCompassSensorList->minDelay * 1000LL)
            compassRate = mCompassSensorList->minDelay * 1000LL;
        mCompassSensor->setDelay(mCompassSensorList->handle, compassRate);
    }

    /* takes care of pressure rate */
    if (mPressureSensor != NULL)
        mPressureSensor->setDelay(mPressureSensorList->handle, pressureRate);

    /* takes care of lpq case for data rate at 200Hz */
    if (checkLPQuaternion()) {
        if (resetRate <= RATE_200HZ) {
#ifndef USE_LPQ_AT_FASTEST
            enableLPQuaternion(0);
#endif
        }
    }

    return res;
}

void MPLSensor::resetMplStates()
{
    VFUNC_LOG;
    ALOGV_IF(ENG_VERBOSE, "HAL:resetMplStates()");

    inv_gyro_was_turned_off();
    inv_accel_was_turned_off();
    inv_compass_was_turned_off();
    inv_quaternion_sensor_was_turned_off();

    return;
}

void MPLSensor::initBias()
{
    VFUNC_LOG;
    int i;

    ALOGV_IF(ENG_VERBOSE, "HAL:inititalize dmp and device offsets to 0");
    if(write_attribute_sensor_continuous(accel_x_dmp_bias_fd, 0) < 0) {
        ALOGE("HAL:Error writing to accel_x_dmp_bias");
    }
    if(write_attribute_sensor_continuous(accel_y_dmp_bias_fd, 0) < 0) {
        ALOGE("HAL:Error writing to accel_y_dmp_bias");
    }
    if(write_attribute_sensor_continuous(accel_z_dmp_bias_fd, 0) < 0) {
        ALOGE("HAL:Error writing to accel_z_dmp_bias");
    }

    if (mAccelSensor != NULL) {
        for (i = 0; i < 3; i++)
            mAccelSensor->setOffset(-1, 0, i);
    }
    setGyroZeroBias();
    if (mGyroSensor != NULL) {
        for (i = 0; i < 3; i++)
            mGyroSensor->setOffset(-1, 0, i);
    }
    return;
}

/*TODO: reg_dump in a separate file*/
void MPLSensor::sys_dump(bool fileMode)
{
    VFUNC_LOG;

    char sysfs_path[MAX_SYSFS_NAME_LEN];
    char scan_element_path[MAX_SYSFS_NAME_LEN];

    memset(sysfs_path, 0, sizeof(sysfs_path));
    memset(scan_element_path, 0, sizeof(scan_element_path));
    inv_get_sysfs_path(sysfs_path);
    sprintf(scan_element_path, "%s%s", sysfs_path, "/scan_elements");

    read_sysfs_dir(fileMode, sysfs_path);
    read_sysfs_dir(fileMode, scan_element_path);

    dump_dmp_img("/data/local/read_img.h");
    return;
}
