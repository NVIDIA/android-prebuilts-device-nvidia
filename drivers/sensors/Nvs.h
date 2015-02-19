/* Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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


#ifndef NVS_H
#define NVS_H

#include <errno.h>
#include <linux/input.h>
#include <hardware/sensors.h>
#include <utils/Vector.h>
#include "SensorBase.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif
#define NVS_IIO_SYSFS_PATH_SIZE_MAX     (128)
#define NVS_IIO_BUFFER_LENGTH           (32)
#define NVS_IIO_BUFFER_LEN_BATCH_EN     (256)

enum NVSB_ATTR {
    ATTR_OFFSET = 0,
    ATTR_PEAK_RAW,
    ATTR_PEAK_SCALE,
    ATTR_RAW,
    ATTR_SAMPLE_FREQ,
    ATTR_SCALE,
    ATTR_N
};

struct attrPath {
    enum NVSB_ATTR attr;
    const char *attrName;
};

static struct attrPath attrPathTbl[] = {
    { ATTR_OFFSET, "offset", },
    { ATTR_PEAK_RAW, "peak_raw", },
    { ATTR_PEAK_SCALE, "peak_scale", },
    { ATTR_RAW, "raw", },
    { ATTR_SAMPLE_FREQ, "sampling_frequency", },
    { ATTR_SCALE, "scale", }
};

struct NvspDriver {
    /* Setting the kernel device name enables the autodetection of a kernel
     * device and will provide the device number for the driver.
     * This can be set to NULL if the driver has its own device detection or
     * driver is not associated with a physical device.
     */
    const char *devName;                /* name of kernel device */
    bool iio;                           /* set if an IIO device */
    /* There may be a need for sensor data but do not want the sensor to be
     * populated in the struct sensor_t sensor list.  An example of this is
     * where the fusion driver needs the gyro device temperature for gyro
     * calculations.
     * Since the sensor will no longer be controlled by the OS, it's assumed
     * another entity will have control (i.g. fusion).  NVSP will still assign
     * it a dynamic handler and be aware of it on the low end of the SW stack
     * to pass its data to the driver.
     * Note: this feature is not compatible with static entries.
     */
    bool noList;                        /* set if to omit from sensor list */
    /* When fusionClient is set, all OS calls go through the fusion driver */
    bool fusionClient;                  /* set if driver uses fusion */
    bool fusionDriver;                  /* set if this is the fusion driver */
    SensorBase *(*newDriver)(int devNum); /* function to install driver */
    /* The purpose of the staticHandle and numStaticHandle is to load drivers
     * for static entries in the nvspSensorList.
     * staticHandle = the static entry of nvspSensorList[]->handle
     *                (nvspSensorList[index] => index + 1)
     * numStaticHandle = the number of sequential static entries to replace
     *                   (will be the limit variable in get_sensor_list)
     * Example:
     * struct NvspDriver nvspDriverList[] = {
     * {
     *  .devName                = NULL,
     *  .newDriver              = function to load driver without device,
     *  .staticHandle           = index to static entry in nvspSensorList + 1,
     *  .numStaticHandle        = number of static entries this driver is for,
     * },
     * Note that this mechanism also allows static entries in nvspSensorList to
     * be replaced by a device/driver using the device autodetect mechanism.
     * Example:
     * static struct sensor_t nvspSensorList[SENSOR_COUNT_MAX] = {
     *  {
     *   .name                   = "NAME",
     *   .vendor                 = "VENDOR",
     *   .version                = 1,
     *   .handle                 = 1,
     *   .type                   = SENSOR_TYPE_,
     *   .maxRange               = 1.0f,
     *   .resolution             = 1.0f,
     *   .power                  = 1.0f,
     *   .minDelay               = 10000,
     *  },
     * };
     * struct NvspDriver nvspDriverList[] = {
     * {
     *  .devName                = device name,
     *  .newDriver              = driver load function with associated device,
     *  .staticHandle           = 1,
     *  .numStaticHandle        = 1,
     * },
     * If devName is found, the driver will be loaded for the static entry
     * index 0 (handle 1).
     * In either case, the get_sensor_list will still be called where
     * index = staticHandle - 1
     * limit = numStaticHandle
     * allowing the driver to either replace the entry or pick up the handle.
     */
    int staticHandle;                   /* if dev found replace static list */
    int numStaticHandle;              /* number of static entries to replace */
};

static const char *strTimestamp = "timestamp";


class Nvsb;

class Nvs: public SensorBase {

    void NvsRemove();

protected:
    const char *mIioName;
    const char **mIioModNames;
    int mType;
    Nvs *mLink;
    Nvsb *mNvsb;
    struct iioChannelInfo **mChannelInfo;
    struct iioChannelInfo *mChannelInfoTs;
    struct sensor_t *mSensorList;
    int mFd;
    int mEnable;
    int mNumChannels;
    int mBatchFlags;
    bool mFlush;
    bool mHasPendingEvent;
    bool mFirstEvent;
    bool mCalibrationMode;
    bool mMatrixEn;
    signed char mMatrix[9];
    char *mSysfsPath;
    char *mSysfsBatchFlags;
    char *mSysfsBatchPeriod;
    char *mSysfsBatchTimeout;
    char *mSysfsFlush;
    char mName[64];
    char mVendor[64];
    float mEventData[16];
    uint64_t mEventData64[8];
    sensors_event_t mEvent;
    sensors_meta_data_event_t mMetaDataEvent;

    virtual int setAttr(int32_t handle, int channel,
                        enum NVSB_ATTR attr, float fVal);
    virtual float getAttr(int32_t handle, int channel, enum NVSB_ATTR attr);
    virtual void setSensorList();
    virtual int batchWrite(int flags, int64_t period_ns, int64_t timeout);
    virtual int metaFlush(sensors_event_t *data);
    virtual float matrix(float x, float y, float z, unsigned int axis);
    virtual void matrixValidate();
    virtual int readMatrix();
    virtual int processEventData(__u8 *buf, int32_t handle);

public:
    Nvs(int devNum,
        const char *iioName,
        const char **iioModNames,
        int type,
        Nvs *link = NULL);
    virtual ~Nvs();

    virtual int getFd(int32_t handle = -1);
    virtual int initFd(struct pollfd *pollFd, int32_t handle = -1);
    virtual int enable(int32_t handle, int enabled, int channel = -1);
    virtual int getEnable(int32_t handle, int channel = -1);
    virtual int setDelay(int32_t handle, int64_t ns, int channel = -1);
    virtual int64_t getDelay(int32_t handle, int channel = -1);
    virtual int setOffset(int32_t handle, float offset, int channel = -1);
    virtual float getOffset(int32_t handle, int channel = -1);
    virtual int setPeakRaw(int32_t handle, float offset, int channel = -1);
    virtual float getPeakRaw(int32_t handle, int channel = -1);
    virtual int setPeakScale(int32_t handle, float offset, int channel = -1);
    virtual float getPeakScale(int32_t handle, int channel = -1);
    virtual int setScale(int32_t handle, float scale, int channel = -1);
    virtual float getScale(int32_t handle, int channel = -1);
    virtual int getSensorList(struct sensor_t *list, int index, int limit);
    virtual struct sensor_t *getSensorListPtr();
    virtual char *getSysfsPath();
    virtual bool hasPendingEvents() { return false; };
    virtual int batch(int handle, int flags,
                      int64_t period_ns, int64_t timeout);
    virtual int flush(int32_t handle);
    virtual int getMatrix(int32_t handle, signed char *matrix);
    virtual int setMatrix(int32_t handle, signed char *matrix);
    virtual int readRaw(int32_t handle, int *data, int channel = -1);
    virtual int readEvents(sensors_event_t *data, int count,
                           int32_t handle = -1);
    virtual int processEvent(sensors_event_t *data, __u8 *buf, int32_t handle);
    Nvs *(getLink)(void);
};

#endif  // NVS_H

