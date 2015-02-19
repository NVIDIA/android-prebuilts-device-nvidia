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

/* The NVS = NVidia Sensor framework */
/* The NVS implementation of populating the sensor list is
 * provided by the kernel driver with the following sysfs attributes:
 * - <iio_device>_part = struct sensor_t.name
 * - <iio_device>_vendor = struct sensor_t.vendor
 * - <iio_device>_version = struct sensor_t.version
 * - in_<iio_device>_peak_raw = struct sensor_t.maxRange
 * - in_<iio_device>_peak_scale = struct sensor_t.resolution
 * - <iio_device>_milliamp = struct sensor_t.power
 * - in_<iio_device>_sampling_frequency = struct sensor_t.minDelay
 *                                        (read when device is off)
 * - <iio_device>_fifo_reserved_event_count =
 *                                       struct sensor_t.fifoReservedEventCount
 * - <iio_device>_fifo_max_event_count = struct sensor_t.fifoMaxEventCount
 * The struct sensor_t.minDelay is obtained by reading
 * in_<iio_device>_sampling_frequency when the device is off.
 * Attributes prefaced with in_ are actual IIO attributes that NVS uses.
 */
/* The NVS implementation of batch and flush are as follows:
 * If batch/flush is supported in a device, the kernel driver must have the
 * following sysfs attributes:
 * - <iio_device>_batch_flags
 *       read: returns the last _batch_flags written.
 *       write: sets the batch_flags parameter.
 * - <iio_device>_batch_period
 *       read: returns the last _batch_period written.
 *       write: sets the batch_period parameter.
 * - <iio_device>_batch_timeout
 *       read: returns the last _batch_timeout written.
 *       write: sets the batch_timeout and initiates the batch command using the
 *          current batch_flags and batch_period parameters.
 * - <iio_device>_flush
 *       read: returns the batch_flags that are supported.  If batch/flush is
 *             supported, then bit 0 must be 1.
 *       write: initiates a flush.
 * As part of the NVS initialization, NVS will attempt to read the _flush
 * attribute.  This will provide the supported flags.  Bit 0 of batch_flags
 * must be 1 if batch/flush is supported.  For efficiency, NVS will then use the
 * read result of this to determine if a batch command is supported before
 * attempting the batch command.
 * When the batch is done, the following sysfs attribute write order is required
 * by the NVS kernel driver:
 * 1. _batch_flags
 * 2. _batch_period
 * 3. _batch_timeout
 * The write to the _batch_timeout sysfs attribute is what initiates a batch
 * command and uses the last _batch_flags and _batch_period parameters written.
 * When a flush is initiated, NVS writes to the sysfs <iio_device>_flush
 * attribute and sets the mFlush flag.  When the flush is completed, the kernel
 * driver will send a timestamp = 0.  When NVS detects timestamp = 0 along with
 * the mFlush flag, the flag is cleared and the sensors_meta_data_event_t is
 * sent.
 */
/* NVS requires the buffer timestamp to be supported and enabled. */
/* The NVS HAL will use the IIO scale and offset sysfs attributes to modify the
 * data using the following formula: (data * scale) + offset
 * A scale value of 0 disables scale.
 * A scale value of 1 puts the NVS HAL into calibration mode where the scale
 * and offset are read everytime the data is read to allow realtime calibration
 * of the scale and offset values to be used in the device tree parameters.
 * Keep in mind the data is buffered but the NVS HAL will display the data and
 * scale/offset parameters in the log.  See calibration steps below.
 */
/* NVS has a calibration mechanism:
 * 1. Disable device.
 * 2. Write 1 to the scale sysfs attribute.
 * 3. Enable device.
 * 4. The NVS HAL will announce in the log that calibration mode is enabled and
 *    display the data along with the scale and offset parameters applied.
 * 5. Write to scale and offset sysfs attributes as needed to get the data
 *    modified as desired.
 * 6. Disabling the device disables calibration mode.
 * 7. Set the new scale and offset parameters in the device tree:
 *    <IIO channel>_scale_val
 *    <IIO channel>_scale_val2
 *    <IIO channel>_offset_val
 *    <IIO channel>_offset_val2
 *    The values are in IIO IIO_VAL_INT_PLUS_MICRO format.
 */
/* NVS kernel drivers have a test mechanism for sending specific data up the
 * SW stack by writing the requested data value to the IIO raw sysfs attribute.
 * That data will be sent anytime there would normally be new data from the
 * device.
 * The feature is disabled whenever the device is disabled.  It remains
 * disabled until the IIO raw sysfs attribute is written to again.
 */


#include <fcntl.h>
#include <cutils/log.h>
#include <stdlib.h>
#include "Nvsb.h"
#include "Nvs.h"


static const char *sysFsIio = "/sys/bus/iio/devices/iio:device";

Nvs::Nvs(int devNum,
         const char *iioName,
         const char **iioModNames,
         int type,
         Nvs *link)
    : SensorBase(NULL, NULL),
      mIioName(iioName),
      mIioModNames(iioModNames),
      mType(type),
      mLink(link),
      mNvsb(NULL),
      mChannelInfo(NULL),
      mChannelInfoTs(NULL),
      mSensorList(NULL),
      mFd(-1),
      mEnable(0),
      mNumChannels(0),
      mBatchFlags(0),
      mFlush(false),
      mHasPendingEvent(false),
      mFirstEvent(false),
      mCalibrationMode(false),
      mMatrixEn(false),
      mSysfsPath(NULL),
      mSysfsBatchFlags(NULL),
      mSysfsBatchPeriod(NULL),
      mSysfsBatchTimeout(NULL),
      mSysfsFlush(NULL)
{
    char str[NVS_IIO_SYSFS_PATH_SIZE_MAX];
    int i;
    int iVal;
    int ret;

    memset(&mMatrix, 0, sizeof(mMatrix));
    /* init event structures */
    memset(&mEvent, 0, sizeof(mEvent));
    mEvent.version = sizeof(sensors_event_t);
    mEvent.type = type;
    memset(&mMetaDataEvent, 0, sizeof(mMetaDataEvent));
    mMetaDataEvent.version = META_DATA_VERSION;
    mMetaDataEvent.type = SENSOR_TYPE_META_DATA;
    /* init sysfs root path */
    ret = asprintf(&mSysfsPath, "%s%d", sysFsIio, devNum);
    if (ret < 0) {
        ALOGE("%s ERR: asprintf\n", __func__);
        goto errExit;
    }

    ALOGI_IF(SensorBase::EXTRA_VERBOSE, "%s %s = %s\n",
             __func__, mIioName, mSysfsPath);
    /* get file descripter */
    if (mLink == NULL) {
        sprintf(str, "/dev/iio:device%d", devNum);
        mFd = open(str, O_RDONLY);
        if (mFd < 0)
            ALOGE("%s open %s ERR: %d\n", __func__, str, -errno);
        else
            /* we only need one instance of the buffer reader */
            mNvsb = new Nvsb(this, mFd, mSysfsPath);
    } else {
        mFd = mLink->getFd();
        /* the derived classes get directed to the one instance */
        mNvsb = mLink->mNvsb;
        if (mNvsb != NULL)
            mNvsb->bufSetLinkRoot(this);
    }
    if (mNvsb == NULL) {
        ALOGE("%s ERR: mNvsb == NULL\n", __func__);
        goto errExit;
    }

    /* get the number of MODS/channels for this device */
    if (mIioModNames != NULL) {
        while (mIioModNames[mNumChannels] != NULL)
            mNumChannels++;
    } else {
        mNumChannels = 1;
    }
    /* allocate channel info pointer(s) memory */
    mChannelInfo = new iioChannelInfo* [mNumChannels];
    /* get channel info */
    for (i = 0; i < mNumChannels; i++) {
        if (mNumChannels > 1) {
            sprintf(str, "%s_%s", mIioName, mIioModNames[i]);
            mChannelInfo[i] = mNvsb->bufGetChanInfo(str);
        } else {
            sprintf(str, "%s", mIioName);
            mChannelInfo[i] = mNvsb->bufGetChanInfo(str);
        }
        if (mChannelInfo[i] == NULL) {
            ALOGE("%s ERR: bufGetChanInfo(%s)\n", __func__, str);
            goto errExit;
        }
    }

    /* (optional) device orientation matrix on platform */
    readMatrix();
    matrixValidate();
    /* init sysfs paths for batch */
    ret = asprintf(&mSysfsBatchFlags,
                   "%s/%s_batch_flags", mSysfsPath, mIioName);
    if (ret < 0)
        goto errExit;

    ret = asprintf(&mSysfsBatchPeriod,
                   "%s/%s_batch_period", mSysfsPath, mIioName);
    if (ret < 0)
        goto errExit;

    ret = asprintf(&mSysfsBatchTimeout,
                   "%s/%s_batch_timeout", mSysfsPath, mIioName);
    if (ret < 0)
        goto errExit;

    ret = asprintf(&mSysfsFlush,
                   "%s/%s_flush", mSysfsPath, mIioName);
    if (ret < 0)
        goto errExit;

    /* get timestamp info */
    mChannelInfoTs = mNvsb->bufGetChanInfo((char *)strTimestamp);
    if (mChannelInfoTs == NULL)
        /* not NVS compliant */
        ALOGE("%s ERR: NO TIMESTAMP\n", __func__);
    else
        /* determine if batch/flush is supported.  See NVS description */
        sysfsIntRead(mSysfsFlush, &mBatchFlags);
    if (mBatchFlags) {
        ALOGI_IF(SensorBase::EXTRA_VERBOSE, "%s batch flags supported = %x\n",
                 __func__, mBatchFlags);
        mNvsb->bufSetLength(NVS_IIO_BUFFER_LEN_BATCH_EN);
    } else {
        ALOGI_IF(SensorBase::EXTRA_VERBOSE, "%s batch mode not supported\n",
                 __func__);
        mNvsb->bufSetLength(NVS_IIO_BUFFER_LENGTH);
    }

    return;

errExit:
    NvsRemove();
}

Nvs::~Nvs()
{
    NvsRemove();
}

void Nvs::NvsRemove()
{
    if (mSysfsPath != NULL)
        free(mSysfsPath);
    if (mSysfsBatchFlags != NULL)
        free(mSysfsBatchFlags);
    if (mSysfsBatchPeriod != NULL)
        free(mSysfsBatchPeriod);
    if (mSysfsBatchTimeout != NULL)
        free(mSysfsBatchTimeout);
    if (mSysfsFlush != NULL)
        free(mSysfsFlush);
    if (mLink == NULL)
        delete mNvsb;
    mNvsb = NULL;
    delete mChannelInfo;
    mNumChannels = 0;
    if (mFd >= 0)
        close(mFd);
}

Nvs *Nvs::getLink(void)
{
    VFUNC_LOG;

    return mLink;
}

int Nvs::getFd(int32_t handle)
{
    VFUNC_LOG;

    return mFd;
}

int Nvs::initFd(struct pollfd *pollFd, int32_t handle)
{
    VFUNC_LOG;
    int flags;

    if (mFd >= 0) {
        pollFd->fd = mFd;
        flags = fcntl(pollFd->fd, F_GETFL, 0);
        flags |= O_NONBLOCK;
        fcntl(pollFd->fd, F_SETFL, flags);
        pollFd->events = POLLIN;
        pollFd->revents = 0;
    }
    return mFd;
}

int Nvs::enable(int32_t handle, int enable, int channel)
{
    VFUNC_LOG;
    float scale;
    int i;
    int ret;

    if (mNvsb == NULL)
        return -EINVAL;

    if (handle < 1)
        handle = mEvent.sensor;

    ret = mNvsb->bufChanAble(handle, channel, enable);
    if (ret) {
        ALOGE("%s: %d -> handle %d ERR\n", __func__, enable, handle);
    } else {
        mEnable = enable;
        if (enable) {
            mFirstEvent = true;
            /* If scale == 1 then we're in calibration mode for the device.
             * This is where scale and offset are read on every sample and
             * applied to the data.  Calibration mode is disabled when the
             * device is disabled.
             */
            ret = sysfsFloatRead(mChannelInfo[0]->attrPath[ATTR_SCALE],
                                 &scale);
            if ((scale == 1.0f) && !ret) {
                mCalibrationMode = true;
                ALOGI("%s handle=%d channel=%d calibration mode ENABLED\n",
                      __func__, handle, channel);
            }
        } else {
            if (mCalibrationMode) {
                /* read new scale and offset after calibration mode */
                for (i = 0; i < mNumChannels; i++) {
                    sysfsFloatRead(mChannelInfo[i]->attrPath[ATTR_SCALE],
                                   &mChannelInfo[i]->attrFVal[ATTR_SCALE]);
                    sysfsFloatRead(mChannelInfo[i]->attrPath[ATTR_OFFSET],
                                   &mChannelInfo[i]->attrFVal[ATTR_OFFSET]);
                }
                mCalibrationMode = false;
                ALOGI("%s handle=%d channel=%d calibration mode DISABLED\n",
                      __func__, handle, channel);
            }
        }
    }
    ALOGI_IF(SensorBase::PROCESS_VERBOSE,
             "%s handle=%d channel=%d enable=%d return=%d\n",
             __func__, handle, channel, enable, ret);
    return ret;
}

int Nvs::getEnable(int32_t handle, int channel)
{
    VFUNC_LOG;

    if (channel >= mNumChannels)
        return -EINVAL;

    if (channel < 0)
        channel = 0;

    return mChannelInfo[channel]->enabled;
}

int Nvs::setAttr(int32_t handle, int channel, enum NVSB_ATTR attr, float fVal)
{
    VFUNC_LOG;
    int i;
    int limit;
    int ret;

    if ((mChannelInfo == NULL) || (channel >= mNumChannels))
        return -EINVAL;

    if ((channel < 0) && !mChannelInfo[0]->attrShared[attr]) {
        /* write all channels */
        i = 0;
        limit = mNumChannels;
    } else if ((channel < 0) || mChannelInfo[0]->attrShared[attr]) {
        /* write just first channel */
        i = 0;
        limit = 1;
    } else {
        /* write specific channel */
        i = channel;
        limit = channel + 1;
    }
    for (; i < limit; i++) {
        ret = sysfsFloatWrite(mChannelInfo[i]->attrPath[attr], fVal);
        if (!ret) {
            ret = sysfsFloatRead(mChannelInfo[i]->attrPath[attr],
                                 &mChannelInfo[i]->attrFVal[attr]);
            if (ret) {
                mChannelInfo[i]->attrCached[attr] = false;
                ret = 0; /* no error writing */
            } else {
                mChannelInfo[i]->attrCached[attr] = true;
            }
        } else if (ret == -EINVAL) {
            /* -EINVAL == no path so store request and cache */
            mChannelInfo[i]->attrFVal[attr] = fVal;
            mChannelInfo[i]->attrCached[attr] = true;
        } else {
            ALOGE("%s: %f -> %s ERR: %d\n",
                  __func__, fVal, mChannelInfo[i]->attrPath[attr], ret);
        }
    }
    return ret;
}

float Nvs::getAttr(int32_t handle, int channel, enum NVSB_ATTR attr)
{
    VFUNC_LOG;

    if ((mChannelInfo == NULL) || (channel >= mNumChannels))
        return 0;

    if (channel < 0)
        channel = 0;
    if (!mChannelInfo[channel]->attrCached[attr]) {
        int ret = sysfsFloatRead(mChannelInfo[channel]->attrPath[attr],
                                 &mChannelInfo[channel]->attrFVal[attr]);
        if (!ret)
            mChannelInfo[channel]->attrCached[attr] = true;
    }
    return mChannelInfo[channel]->attrFVal[attr];
}

int Nvs::setDelay(int32_t handle, int64_t ns, int channel)
{
    VFUNC_LOG;
    return setAttr(handle, channel, ATTR_SAMPLE_FREQ, (float)(ns / 1000LL));
}

int64_t Nvs::getDelay(int32_t handle, int channel)
{
    VFUNC_LOG;
    float fVal = getAttr(handle, channel, ATTR_SAMPLE_FREQ);
    return (int64_t)fVal * 1000LL;
}

int Nvs::setOffset(int32_t handle, float offset, int channel)
{
    VFUNC_LOG;

    int ret = setAttr(handle, channel, ATTR_OFFSET, offset);
    if (!ret)
        setSensorList();
    return ret;
}

float Nvs::getOffset(int32_t handle, int channel)
{
    VFUNC_LOG;
    return getAttr(handle, channel, ATTR_OFFSET);
}

int Nvs::setPeakRaw(int32_t handle, float peakRaw, int channel)
{
    VFUNC_LOG;

    int ret = setAttr(handle, channel, ATTR_PEAK_RAW, peakRaw);
    if (!ret)
        setSensorList();
    return ret;
}

float Nvs::getPeakRaw(int32_t handle, int channel)
{
    VFUNC_LOG;
    return getAttr(handle, channel, ATTR_PEAK_RAW);
}

int Nvs::setPeakScale(int32_t handle, float peakScale, int channel)
{
    VFUNC_LOG;

    int ret = setAttr(handle, channel, ATTR_PEAK_SCALE, peakScale);
    if (!ret)
        setSensorList();
    return ret;
}

float Nvs::getPeakScale(int32_t handle, int channel)
{
    VFUNC_LOG;
    return getAttr(handle, channel, ATTR_PEAK_SCALE);
}

int Nvs::setScale(int32_t handle, float scale, int channel)
{
    VFUNC_LOG;

    int ret = setAttr(handle, channel, ATTR_SCALE, scale);
    if (!ret)
        setSensorList();
    return ret;
}

float Nvs::getScale(int32_t handle, int channel)
{
    VFUNC_LOG;
    return getAttr(handle, channel, ATTR_SCALE);
}

int Nvs::batchWrite(int flags, int64_t period_ns, int64_t timeout)
{
    int ret;

    ret = sysfsIntWrite(mSysfsBatchFlags, flags);
    if (!ret) {
        ret = sysfsIntWrite(mSysfsBatchPeriod, period_ns / 1000LL); /* us */
        if (!ret)
            ret = sysfsIntWrite(mSysfsBatchTimeout,
                                timeout / 1000000LL); /* ms */
    }
    if (ret)
        ret = -EINVAL;
    return ret;
}

int Nvs::batch(int handle, int flags, int64_t period_ns, int64_t timeout)
{
    VFUNC_LOG;
    int flag;
    int ret = -EINVAL;

    flag = flags & SENSORS_BATCH_WAKE_UPON_FIFO_FULL;
    if ((flag & mBatchFlags) == flag) { /* if special flags supported */
        if (timeout) {
            if (mSensorList->fifoMaxEventCount && (flags ==
                                                   (flags & mBatchFlags)))
                /* if batch enabled && batch options supported */
                ret = batchWrite(flags, period_ns, timeout);
        } else {
            /* batch command must succeed */
            if (mSensorList->fifoMaxEventCount) {
                ret = batchWrite(flags, period_ns, timeout);
            } else {
                if (!(flags & SENSORS_BATCH_DRY_RUN))
                    /* if batch not supported send the period_ns to delay */
                    ret = setDelay(handle, period_ns);
                else
                    ret = 0;
            }
        }
    }
    ALOGI_IF(SensorBase::PROCESS_VERBOSE,
             "%s mBatchFlags=%d fifoMaxEventCount=%u "
             "handle=%d flags=%d period_ns=%lld timeout=%lld return=%d\n",
             __func__, mBatchFlags, mSensorList->fifoMaxEventCount,
             handle, flags, period_ns, timeout, ret);
    return ret;
}

int Nvs::flush(int32_t handle)
{
    VFUNC_LOG;
    int ret = -EINVAL;

    if (mEnable) {
        if (mSensorList->fifoMaxEventCount) {
            /* if batch is supported */
            ret = sysfsIntWrite(mSysfsFlush, 1);
            if (ret)
                ret = -EINVAL;
            else
                mFlush = true;
        } else {
            /* a meta packet will be sent without kernel driver interaction */
            ret = 0;
            mFlush = true;
        }
    }
    ALOGI_IF(SensorBase::PROCESS_VERBOSE,
             "%s handle=%d return=%d\n", __func__, handle, ret);
    return ret;
}

void Nvs::setSensorList()
{
    char path[NVS_IIO_SYSFS_PATH_SIZE_MAX];
    int en;

    if (mSensorList) {
        en = mEnable;
        if (mEnable)
            /* Need to turn off device to get some NVS attributes */
            enable(mEvent.sensor, 0);
        mSensorList->maxRange = getPeakRaw(mEvent.sensor);
        mSensorList->resolution = getPeakScale(mEvent.sensor);
        sprintf(path, "%s/%s_milliamp", mSysfsPath, mIioName);
        sysfsFloatRead(path, &mSensorList->power);
        sysfsIntRead(mChannelInfo[0]->attrPath[ATTR_SAMPLE_FREQ],
                     &mSensorList->minDelay);
        sprintf(path, "%s/%s_fifo_reserved_event_count", mSysfsPath, mIioName);
        sysfsIntRead(path, (int *)&mSensorList->fifoReservedEventCount);
        sprintf(path, "%s/%s_fifo_max_event_count", mSysfsPath, mIioName);
        sysfsIntRead(path, (int *)&mSensorList->fifoMaxEventCount);
        if (en)
            enable(mEvent.sensor, en);
        if (SensorBase::ENG_VERBOSE) {
            ALOGI("--- %s sensor_t ---\n", mIioName);
            ALOGI("sensor_t.name=%s", mSensorList->name);
            ALOGI("sensor_t.vendor=%s", mSensorList->vendor);
            ALOGI("sensor_t.version=%d\n", mSensorList->version);
            ALOGI("sensor_t.handle=%d\n", mSensorList->handle);
            ALOGI("sensor_t.type=%d\n", mSensorList->type);
            ALOGI("sensor_t.maxRange=%f\n", mSensorList->maxRange);
            ALOGI("sensor_t.resolution=%f\n", mSensorList->resolution);
            ALOGI("sensor_t.power=%f\n", mSensorList->power);
            ALOGI("sensor_t.minDelay=%d\n", mSensorList->minDelay);
            ALOGI("sensor_t.fifoReservedEventCount=%d\n",
                  mSensorList->fifoReservedEventCount);
            ALOGI("sensor_t.fifoMaxEventCount=%d\n",
                  mSensorList->fifoMaxEventCount);
        }
    }
}

int Nvs::getSensorList(struct sensor_t *list, int index, int limit)
{
    char path[NVS_IIO_SYSFS_PATH_SIZE_MAX];
    int i;
    int handle;
    int ret;

    if (list == NULL) {
        if (mNumChannels)
            return 1; /* just return count if NULL */

        return 0; /* flag no sensor due to error */
    }

    if (limit < 1) /* return 0 if no rooom in list */
        return 0;

    if (!mNumChannels)
        return 0; /* don't put sensor in list due to error */

    /* Remember our sensor in the global sensors list.
     * This is used to determine the sensor trigger mode as well as allowing us
     * to modify sensor list data at run-time, e.g. range/resolution changes.
     */
    mSensorList = &list[index];
    /* give handle to everyone that needs it */
    handle = index + 1;
    mEvent.sensor = handle;
    mMetaDataEvent.meta_data.sensor = handle;
    for (i = 0; i < mNumChannels; i++)
        mChannelInfo[i]->handle = handle;
    mSensorList->handle = handle;
    sprintf(path, "%s/%s_part", mSysfsPath, mIioName);
    ret = sysfsStrRead(path, mName);
    if (ret > 0) {
        mSensorList->name = mName;
    } else {
        /* if the NVS IIO extension for part is not supported
         * then use the driver name
         */
        sprintf(path, "%s/name", mSysfsPath);
        ret = sysfsStrRead(path, mName);
        if (ret > 0)
            mSensorList->name = mName;
    }
    sprintf(path, "%s/%s_vendor", mSysfsPath, mIioName);
    ret = sysfsStrRead(path, mVendor);
    if (ret > 0)
        mSensorList->vendor = mVendor;
    sprintf(path, "%s/%s_version", mSysfsPath, mIioName);
    sysfsIntRead(path, &mSensorList->version);
    mSensorList->type = mType;
    setSensorList();
    return 1; /* return count */
}

struct sensor_t *Nvs::getSensorListPtr()
{
    return mSensorList;
}

char *Nvs::getSysfsPath()
{
    return mSysfsPath;
}

int Nvs::metaFlush(sensors_event_t *data)
{
    mFlush = false;
    mMetaDataEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
    *data = mMetaDataEvent;
    ALOGI_IF(SensorBase::INPUT_DATA || SensorBase::EXTRA_VERBOSE,
             "%s %s sensors_meta_data_event_t version=%d sensor=%d type=%d\n"
             "timestamp=%lld meta_data.what=%d meta_data.sensor=%d\n",
             __func__, mIioName,
             data->version,
             data->sensor,
             data->type,
             data->timestamp,
             data->meta_data.what,
             data->meta_data.sensor);
    return 1;
}

int Nvs::readMatrix()
{
    VFUNC_LOG;
    FILE *fp;
    char str[NVS_IIO_SYSFS_PATH_SIZE_MAX];

    sprintf(str, "%s/%s_matrix", mSysfsPath, mIioName);
    fp = fopen(str, "r");
    if (fp != NULL) {
        fscanf(fp, "%hhd,%hhd,%hhd,%hhd,%hhd,%hhd,%hhd,%hhd,%hhd",
               &mMatrix[0], &mMatrix[1], &mMatrix[2], &mMatrix[3], &mMatrix[4],
               &mMatrix[5], &mMatrix[6], &mMatrix[7], &mMatrix[8]);
        fclose(fp);
        ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                 "%s %s = %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd\n",
                 __func__, str, mMatrix[0], mMatrix[1], mMatrix[2], mMatrix[3],
                  mMatrix[4], mMatrix[5], mMatrix[6], mMatrix[7], mMatrix[8]);
        return 0;
    }

    return -EINVAL;
}

int Nvs::getMatrix(int32_t handle, signed char *matrix)
{
    VFUNC_LOG;
    int ret = readMatrix();

    if (!ret)
        memcpy(matrix, &mMatrix, sizeof(mMatrix));
    return ret;
}

int Nvs::setMatrix(int32_t handle, signed char *matrix)
{
    VFUNC_LOG;
    memcpy(&mMatrix, matrix, sizeof(mMatrix));
    matrixValidate();
    return 0;
}

void Nvs::matrixValidate()
{
    VFUNC_LOG;
    unsigned int i;

    for (i = 0; i < sizeof(mMatrix); i++) {
        if (mMatrix[i]) {
            mMatrixEn = true;
            ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                     "%s %s mMatrixEn = true\n", __func__, mIioName);
            return;
        }
    }
    ALOGI_IF(SensorBase::EXTRA_VERBOSE,
             "%s %s mMatrixEn = false\n", __func__, mIioName);
    mMatrixEn = false;
}

float Nvs::matrix(float x, float y, float z, unsigned int axis)
{
    VFUNC_LOG;
    return ((mMatrix[0 + axis] == 1 ? x : (mMatrix[0 + axis] == -1 ? -x : 0)) +
            (mMatrix[3 + axis] == 1 ? y : (mMatrix[3 + axis] == -1 ? -y : 0)) +
            (mMatrix[6 + axis] == 1 ? z : (mMatrix[6 + axis] == -1 ? -z : 0)));
}

int Nvs::processEventData(__u8 *buf, int32_t handle)
{
    VFUNC_LOG;
    void *data;
    bool onChange = false;
    int n;
    int i;

    for (i = 0; i < mNumChannels; i++) {
        if (mChannelInfo[i]->bytes <= sizeof(float))
            data = &mEventData[i];
        else
            data = &mEventData64[i];
        n = mNvsb->bufData(mChannelInfo[i], buf, data);

        if (SensorBase::INPUT_DATA) {
            if (n <= (int)sizeof(float))
                ALOGI("%s size=%d mEventData[%d]=%f\n",
                      __func__, n, i, mEventData[i]);
            else
                ALOGI("%s size=%d mEventData64[%d]=%llu\n",
                      __func__, n, i, mEventData64[i]);
        }
        if (n < 0)
            return 0;

        if ((handle > 0) && (n <= (int)sizeof(float))) {
            /* apply offset and scale if not driven by fusion */
            if (mCalibrationMode) {
                float scale = 0;
                float offset = 0;

                /* read scale and offset each time in calibration mode */
                sysfsFloatRead(mChannelInfo[i]->attrPath[ATTR_SCALE], &scale);
                sysfsFloatRead(mChannelInfo[i]->attrPath[ATTR_OFFSET],
                               &offset);
                if (scale) {
                    ALOGI("calibration: data[%d]=%f * scale[%d]=%f + "
                          "offset[%d]=%f == %f\n", i, mEventData[i], i, scale,
                           i, offset, (mEventData[i] * scale) + offset);
                    mEventData[i] *= scale;
                    mEventData[i] += offset;
                } else {
                    ALOGI("calibration: data[%d]=%f * NO SCALE + "
                          "offset[%d]=%f == %f\n", i, mEventData[i],
                           i, offset, mEventData[i] + offset);
                    mEventData[i] += offset;
                }
            } else {
                if (mChannelInfo[i]->attrFVal[ATTR_SCALE])
                    mEventData[i] *= mChannelInfo[i]->attrFVal[ATTR_SCALE];
                if (mChannelInfo[i]->attrFVal[ATTR_OFFSET])
                    mEventData[i] += mChannelInfo[i]->attrFVal[ATTR_OFFSET];
            }
        }
    }

    if (mMatrixEn) {
        /* swap axis data based on matrix */
        float fVal;

        for (i = 0; i < 3; i++) {
            fVal = matrix(mEventData[0], mEventData[1], mEventData[2], i);
            if (mEvent.data[i] != fVal)
                onChange = true;
            mEvent.data[i] = fVal;
        }
    } else {
        /* fill event data and determine if changed for on-change sensors */
        for (i = 0; i < mNumChannels; i++) {
            if (mChannelInfo[i]->bytes <= sizeof(float)) {
                if (mEvent.data[i] != mEventData[i])
                    onChange = true;
                mEvent.data[i] = mEventData[i];
            } else {
                if ((uint64_t)mEvent.data[i] != mEventData64[i])
                    onChange = true;
                mEvent.data[i] = mEventData64[i];
            }
        }
    }
    /* if on-change sensor (minDelay == 0) drop data if same as before */
    if (mSensorList->minDelay)
        onChange = true;
    if (!onChange && !mFirstEvent && !mCalibrationMode)
        /* on-change sensor and data hasn't changed and first already sent */
        return 0;

    return 1;
}

int Nvs::processEvent(sensors_event_t *data, __u8 *buf, int32_t handle)
{
    VFUNC_LOG;
    int nEvents = 0;

    if (!mNumChannels)
        return 0;

    if (mFlush && !mSensorList->fifoMaxEventCount)
        /* if batch not supported but still need to send meta packet */
        return metaFlush(data);

    if (mChannelInfoTs)
        mEvent.timestamp = *((int64_t *)(buf + mChannelInfoTs->location));
    else
        /* NVS requires a timestamp, but if there isn't one, then batch/flush
         * and the ability to detect invalid data is disabled.
         */
        mEvent.timestamp = getTimestamp();
    if (!mEvent.timestamp) {
        /* The absence of a timestamp is because either a flush was done or the
         * sensor data is currently invalid.
         * If we've sent down a flush command (mFlush), then we'll send up a
         * meta data event.  Otherwise, the data stream stops here as data is
         * just dropped while there is no timestamp.
         */
        if (mFlush)
            return metaFlush(data);

        ALOGI_IF(SensorBase::INPUT_DATA || SensorBase::ENG_VERBOSE,
                 "%s %s data dropped - no timestamp\n",
                 __func__, mIioName);
        return 0;
    }

    if (mEnable) {
        nEvents = processEventData(buf, handle);
        if (nEvents > 0) {
            *data = mEvent;
            mFirstEvent = false;
            if (mSensorList->minDelay < 0)
                /* this is a one-shot and is disabled after one event */
                mEnable = 0;
            ALOGI_IF(SensorBase::INPUT_DATA,
                     "%s %s sensors_event_t version=%d sensor=%d type=%d\n"
                     "timestamp=%lld "
                     "float[0]=%f float[1]=%f float[2]=%f float[3]=%f\n",
                     __func__, mIioName,
                     data->version,
                     data->sensor,
                     data->type,
                     data->timestamp,
                     data->data[0],
                     data->data[1],
                     data->data[2],
                     data->data[3]);
        }
    }
    return nEvents;
}

int Nvs::readEvents(sensors_event_t *data, int count, int32_t handle)
{
    int nEvents;

    nEvents = mNvsb->bufFill(data, count, handle);
    return nEvents;
}

int Nvs::readRaw(int32_t handle, int *data, int channel)
{
    int ret;

    if (channel >= mNumChannels)
        return -EINVAL;

    if (channel < 0)
        channel = 0;

    ret = sysfsIntRead(mChannelInfo[channel]->attrPath[ATTR_RAW], data);
    return ret;
}

