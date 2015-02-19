/* Copyright (C) 2012 The Android Open Source Project
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

/* NVSP NVidia Sensor Platform include file is the NVS interface between the
 * Android OS and the NVS HAL.  It's an include file that is to be included
 * at the end of the platform's sensors.cpp file.  It allows the sensors.cpp
 * file to contain just the nvspDriver structure and any supporting driver
 * loading code.  See the nvspDriver documentation in Nvs.h.
 * NVSP expects the following defines when included in the platform's
 * sensors.cpp file:
 * #define SENSOR_COUNT_MAX             Maximum number of possible sensors
 * #define SENSOR_PLATFORM_NAME         "<platform> sensors module"
 * #define SENSOR_PLATFORM_AUTHOR       ex: "NVIDIA CORP"
 * #define SENSORS_DEVICE_API_VERSION   ex: SENSORS_DEVICE_API_VERSION_1_1
 */
/* NVSP provides a mechanism for setting dev->device.common.version
 * dynamically:
 * If SENSORS_DEVICE_API_VERSION is defined then dev->device.common.version
 * is static and set to this define.
 * If SENSORS_DEVICE_API_VERSION is not defined, then the variable,
 * uint32_t devDeviceCommonVersion is expected to hold the version value
 * when the nvspDriverList is completed.
 * This is to allow platforms with legacy sensors that aren't compatible with
 * the latest sensor API.
 */


#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include "Nvs.h"

#define DRIVER_COUNT_MAX                (SENSOR_COUNT_MAX + 1)

struct NvSensor {
    SensorBase *driver;
    int fd;
    bool fusionClient;
    bool enabled;
    unsigned int pollTime;
};


static int numSensors = 0;

static int open_sensors(const struct hw_module_t *module, const char *id,
                        struct hw_device_t **device);

static int sensors__get_sensors_list(struct sensors_module_t *module,
                                     struct sensor_t const **list)
{
    *list = nvspSensorList;
    return numSensors;
}

static struct hw_module_methods_t sensors_module_methods = {
        open: open_sensors
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
        common: {
                tag: HARDWARE_MODULE_TAG,
                version_major: 1,
                version_minor: 0,
                id: SENSORS_HARDWARE_MODULE_ID,
                name: SENSOR_PLATFORM_NAME,
                author: SENSOR_PLATFORM_AUTHOR,
                methods: &sensors_module_methods,
                dso: NULL,
                reserved: {0}
        },
        get_sensors_list: sensors__get_sensors_list,
};

struct sensors_poll_context_t {
    sensors_poll_device_1_t device; /* must be first */

    sensors_poll_context_t();
    ~sensors_poll_context_t();
    int activate(int handle, int enabled);
    int setDelay(int handle, int64_t ns);
    int pollEvents(sensors_event_t *data, int count);
    int batch(int handle, int flags, int64_t period_ns, int64_t timeout);
    int flush(int handle);
    unsigned int polltime;

private:
    struct NvSensor mSensor[DRIVER_COUNT_MAX];
    struct pollfd mPollFds[DRIVER_COUNT_MAX];
    int mFdHandles[DRIVER_COUNT_MAX];
    int numFds;
    int mWritePipeFd;
    size_t wake;
    static const char WAKE_MESSAGE = 'W';
    int mDrvHandles[DRIVER_COUNT_MAX];
    int numDrivers;

    int getDevNum(const char *dev_name, bool iio) {
        const struct dirent *d;
        DIR *dp;
        FILE *fp;
        const char *dpath;
        const char *dname;
        char dname_iio[] = "iio:device";
        char dname_input[] = "input";
        char path[NVS_IIO_SYSFS_PATH_SIZE_MAX];
        char name[32] = {0};
        int devNum;
        int fd;
        int ret;
        size_t dsize;

        if (iio) {
            dpath = "/sys/bus/iio/devices";
            dname = dname_iio;
            dsize = sizeof(dname_iio);
        } else {
            dpath = "/sys/class/input";
            dname = dname_input;
            dsize = sizeof(dname_input);
        }
        dp = opendir(dpath);
        if (dp == NULL) {
            ALOGE("%s ERR: opening directory %s", __func__, dpath);
            return -ENODEV;
        }

        do {
            errno = 0;
            if ((d = readdir(dp)) != NULL) {
                ret = strncmp(d->d_name, dname, dsize - 1);
                if (!ret) {
                    sprintf(path, "%s%%d", dname);
                    sscanf(d->d_name, path, &devNum);
                    sprintf(path, "%s/%s%d/name", dpath, dname, devNum);
                    fd = open(path, O_RDONLY);
                    if (fd >= 0) {
                        memset(name, 0, sizeof(name));
                        read(fd, name, sizeof(name));
                        close(fd);
                        name[strlen(name) - 1] = 0;
                        ret = strcmp(name, dev_name);
                        if (!ret) {
                            ALOGI("%s %s/%s found\n", __func__, path, dev_name);
                            closedir(dp);
                            return devNum;
                        }
                    }
                }
            }
        } while (d != NULL);

        if (errno)
            ALOGE("%s ERR: reading %s/%s\n", __func__, dpath, d->d_name);
        else
            ALOGI_IF(SensorBase::EXTRA_VERBOSE, "%s %s NOT found\n",
                     __func__, dev_name);
        closedir(dp);
        return -ENODEV;
    }

    void setPollTime() {
        unsigned int poll_time = UINT_MAX;

        for (int handle = 1; handle <= numSensors; handle++) {
            unsigned int poll_time_tmp = mSensor[handle].pollTime;
            if (poll_time_tmp < poll_time)
                poll_time = poll_time_tmp;
        }
        polltime = poll_time;
        const char wakeMessage(WAKE_MESSAGE);
        int result = write(mWritePipeFd, &wakeMessage, 1);
        ALOGE_IF(result < 0, "error sending wake message (%s)", strerror(errno));
    }

    void dbgSensorList(unsigned int i) {
        ALOGI("%s ------------------ SENSOR %u ------------------\n",
              __func__, i);
        ALOGI("%s sensor_t[%u].name=%s",
              __func__, i, nvspSensorList[i].name);
        ALOGI("%s sensor_t[%u].vendor=%s",
              __func__, i, nvspSensorList[i].vendor);
        ALOGI("%s sensor_t[%u].version=%d\n",
              __func__, i, nvspSensorList[i].version);
        ALOGI("%s sensor_t[%u].handle=%d\n",
              __func__, i, nvspSensorList[i].handle);
        ALOGI("%s sensor_t[%u].type=%d\n",
              __func__, i, nvspSensorList[i].type);
        ALOGI("%s sensor_t[%u].maxRange=%f\n",
              __func__, i, nvspSensorList[i].maxRange);
        ALOGI("%s sensor_t[%u].resolution=%f\n",
              __func__, i, nvspSensorList[i].resolution);
        ALOGI("%s sensor_t[%u].power=%f\n",
              __func__, i, nvspSensorList[i].power);
        ALOGI("%s sensor_t[%u].minDelay=%d\n",
              __func__, i, nvspSensorList[i].minDelay);
        ALOGI("%s sensor_t[%u].fifoReservedEventCount=%d\n",
              __func__, i, nvspSensorList[i].fifoReservedEventCount);
        ALOGI("%s sensor_t[%u].fifoMaxEventCount=%d\n",
              __func__, i, nvspSensorList[i].fifoMaxEventCount);
        ALOGI("%s driver=%p\n", __func__, mSensor[i + 1].driver);
        if (mSensor[i + 1].fusionClient)
            ALOGI("%s fusion driver=%p\n", __func__, mSensor[0].driver);
        ALOGI("%s fd=%d\n", __func__, mSensor[i + 1].fd);
    }
};


sensors_poll_context_t::sensors_poll_context_t()
{
    VFUNC_LOG;
    int devNum;
    int handle;
    int handleEnd;
    int noListHandle;
    int i;
    int ret;

    polltime = UINT_MAX;
    if (SENSOR_COUNT_MAX < 1) {
        ALOGI("%s no SENSOR_COUNT_MAX. Exiting\n", __func__);
        return;
    }

    memset(&mSensor, 0, sizeof(mSensor));
    for (handle = 0; handle < DRIVER_COUNT_MAX; handle++) {
        mSensor[handle].driver = NULL;
        mSensor[handle].fd = -1;
        mSensor[handle].pollTime = UINT_MAX;
    }
/* If the sensor list is empty, the handle starts at 1 due to the below google
 * comment.  Therefore we align our mSensor[] with the sensor handle.
 * sensor_t[0].handle = 1 => mSensor[1] = sensor_t[0] struct NvSensor details.
 * In other words, the sensors handle will always be +1 than the index in the
 * sensor list.  This is why mSensor[SENSOR_COUNT_MAX + 1].
 * Since mSensor[] is one based and sensor_t[] is zero based, we reserve
 * mSensor[0] for the fusion sensor.
 */
/** From .../hardware/libhardware/include/hardware/sensors.h:
 * Handles must be higher than SENSORS_HANDLE_BASE and must be unique.
 * A Handle identifies a given sensors. The handle is used to activate
 * and/or deactivate sensors.
 * In this version of the API there can only be 256 handles.
#define SENSORS_HANDLE_BASE             0
 */
    /* skip past sensors already in the sensor list (static entries) */
    for (i = 0; i < SENSOR_COUNT_MAX; i++) {
        if (!nvspSensorList[i].type)
            break;
    }
    /* The non-listed sensors will be added from the end of the list and not
     * added to numSensors.  noListHandle's value is the handle that will be
     * assigned to the next noList sensor
     */
    noListHandle = SENSOR_COUNT_MAX;
    numSensors = i; /* running total of sensors */
    for (unsigned nvsp_i = 0; nvsp_i < ARRAY_SIZE(nvspDriverList); nvsp_i++) {
        if (numSensors >= noListHandle) {
            ALOGE("%s ERR: exceeded SENSOR_COUNT_MAX\n", __func__);
            break;
        }

        handleEnd = 0; /* fix bogus compiler warning */
        /* sensor handle is mSensor[] index */
        if (nvspDriverList[nvsp_i].noList) {
            if (nvspDriverList[nvsp_i].staticHandle) {
                ALOGE("%s ERR: noList for a static sensor entry\n", __func__);
                /* skip this entry */
                continue;
            }
            handle = noListHandle;
        } else {
            if (nvspDriverList[nvsp_i].staticHandle) {
                /* if this is for static entry/entries */
                handle = nvspDriverList[nvsp_i].staticHandle;
                handleEnd = nvspDriverList[nvsp_i].numStaticHandle;
                if (handleEnd > 0) {
                    handleEnd += handle;
                } else {
                    handleEnd = handle + 1;
                    ALOGE("%s ERR: numStaticHandle modified\n", __func__);
                }
            } else {
                /* dynamic handle */
                handle = numSensors + 1;
            }
        }

        /* nvspDriverList.devName if HW device associated with driver */
        if (nvspDriverList[nvsp_i].devName != NULL) {
            devNum = getDevNum(nvspDriverList[nvsp_i].devName,
                               nvspDriverList[nvsp_i].iio);
            if (devNum < 0)
                /* device not found */
                continue;
        } else {
            devNum = -1;
        }

        /* load the driver */
        if (nvspDriverList[nvsp_i].newDriver != NULL)
            mSensor[handle].driver = nvspDriverList[nvsp_i].newDriver(devNum);
        if (mSensor[handle].driver == NULL) {
            /* if no driver then a fusion client static entry is allowed */
            if (nvspDriverList[nvsp_i].staticHandle &&
                                         nvspDriverList[nvsp_i].fusionClient) {
                for (i = handle; i < handleEnd; i++) {
                    mSensor[i].driver = NULL;
                    mSensor[i].fusionClient = true;
                }
            } /* else just skip entry */
            continue;
        }

        if (nvspDriverList[nvsp_i].fusionDriver)
            /* mSensor[0] reserved for fusion driver */
            mSensor[0].driver = mSensor[handle].driver;
        /* populate the sensor list */
        if (nvspDriverList[nvsp_i].staticHandle) {
            mSensor[handle].driver->getSensorList(nvspSensorList, handle - 1,
                                                  handleEnd - handle);
        } else {
            if (nvspDriverList[nvsp_i].noList) {
                int tmp = mSensor[handle].driver->getSensorList(nvspSensorList,
                                                          noListHandle - 1, 1);
                if (tmp) {
                    handleEnd = handle + 1;
                    noListHandle--;
                } else {
                    /* if no sensor was added to the list */
                    continue;
                }
            } else {
                numSensors += mSensor[handle].driver->
                                      getSensorList(nvspSensorList, numSensors,
                                                    SENSOR_COUNT_MAX - numSensors);
                if (handle > numSensors)
                    /* if no sensors were added to the list */
                    continue;

                handleEnd = numSensors + 1;
            }
        }

        /* fill in mSensor[i] */
        for (i = handle; i < handleEnd; i++) {
            mSensor[i].driver = mSensor[handle].driver;
            mSensor[i].fusionClient = nvspDriverList[nvsp_i].fusionClient;
            /* get the sensor file descriptors */
            mSensor[i].fd = mSensor[i].driver->getFd(i);
        }
    }

    if (!numSensors) {
        ALOGI("%s no sensors. Exiting\n", __func__);
        return;
    }

    /* make sure all sensors have a driver or using a valid fusion driver */
    for (i = 1; i <= numSensors; i++) {
        if (mSensor[i].driver)
            continue;

        if (mSensor[i].fusionClient && mSensor[0].driver)
            continue;

        numSensors = 0;
        ALOGE("%s ERR: no sensor or fusion driver! Aborting\n", __func__);
        return;
    }

    /* make sure all non-list sensors have a driver or using fusion */
    if (noListHandle < SENSOR_COUNT_MAX) {
        for (i = noListHandle + 1; i < DRIVER_COUNT_MAX; i++) {
            if (mSensor[i].driver)
                continue;

            if (mSensor[i].fusionClient && mSensor[0].driver)
                continue;

            numSensors = 0;
            ALOGE("%s ERR: no sensor or fusion driver! Aborting\n", __func__);
            return;
        }
    }

    if (SensorBase::EXTRA_VERBOSE) {
        for (i = 0; i < numSensors; i++)
            dbgSensorList(i);
        if (noListHandle < SENSOR_COUNT_MAX) {
            ALOGI("%s ---- active sensors not included in sensor list ----\n",
                  __func__);
            for (i = noListHandle; i < SENSOR_COUNT_MAX; i++)
                dbgSensorList(i);
        }
    }
    /* add unique fds to mPollFds and link fds to mSensor[] via mFdHandles[] */
    numFds = 0;
    memset(&mPollFds, 0, sizeof(mPollFds));
    for (i = 0; i < numSensors; i++)
        mPollFds->fd = -1;
    for (handle = numSensors; handle > 0; handle--) {
        if (mSensor[handle].fd < 0)
            continue;

        if (numFds) {
            /* find if fd already in mPollFds */
            for (i = 0; i < numFds; i++) {
                if (mPollFds[i].fd == mSensor[handle].fd)
                    break;
            }
        } else {
            i = 0;
        }
        if (i == numFds) {
            /* new fd */
            mPollFds[numFds].fd = mSensor[handle].fd;
            /* link to mSensor[] */
            mFdHandles[numFds] = handle;
            /* initialize pollfd */
            mSensor[handle].driver->initFd(&mPollFds[numFds], handle);
            numFds++;
        }
    }

    if (SensorBase::EXTRA_VERBOSE) {
        ALOGI("%s -------------------- FD's --------------------\n", __func__);
        for (i = 0; i < numFds; i++) {
            if (mSensor[mFdHandles[i]].fusionClient)
                ALOGI("%s mPollFds[%d]=%d  driver=%p  handler=%d\n",
                      __func__, i, mPollFds[i].fd,
                      mSensor[0].driver, mFdHandles[i]);
            else
                ALOGI("%s mPollFds[%d]=%d  driver=%p  handler=%d\n",
                      __func__, i, mPollFds[i].fd,
                      mSensor[mFdHandles[i]].driver, mFdHandles[i]);
        }
    }
    /* setup wake pipe */
    wake = numFds;
    int wakeFds[2];
    int result = pipe(wakeFds);
    ALOGE_IF(result < 0, "error creating wake pipe (%s)", strerror(errno));
    fcntl(wakeFds[0], F_SETFL, O_NONBLOCK);
    fcntl(wakeFds[1], F_SETFL, O_NONBLOCK);
    mWritePipeFd = wakeFds[1];
    mPollFds[wake].fd = wakeFds[0];
    mPollFds[wake].events = POLLIN;
    mPollFds[wake].revents = 0;
    if (SensorBase::EXTRA_VERBOSE)
        ALOGI("%s mPollFds[%d]=%d  driver=wake\n",
              __func__, wake, mPollFds[wake].fd);
    /* make a list of unique drivers for pending events */
    numDrivers = 0;
    for (handle = 1; handle <= numSensors; handle++) {
        if (mSensor[handle].driver == NULL)
            continue;

        if (numDrivers) {
            /* find drivers already in list */
            for (i = 0; i < numDrivers; i++) {
                if (mSensor[mDrvHandles[i]].driver == mSensor[handle].driver)
                    break;
            }
        } else {
            i = 0;
        }
        if (i == numDrivers) {
            mDrvHandles[numDrivers++] = handle;
        }
    }
    if (SensorBase::EXTRA_VERBOSE) {
        ALOGI("%s --------------- SENSOR DRIVERS ---------------\n", __func__);
        ALOGI("%s fusion driver=%p\n", __func__, mSensor[0].driver);
        for (i = 0; i < numDrivers; i++)
            ALOGI("%s mDrvHandles[%d]=%d  driver=%p  use fusion=%x\n",
                  __func__, i, mDrvHandles[i], mSensor[mDrvHandles[i]].driver,
                  mSensor[mDrvHandles[i]].fusionClient);
    }
}

sensors_poll_context_t::~sensors_poll_context_t()
{
    VFUNC_LOG;
    int i;
    int handle;

    for (handle = 1; handle <= numSensors; handle++) {
        if (mSensor[handle].driver != NULL) {
            delete mSensor[handle].driver;
            for (i = numSensors; i >= handle; i--) {
                if (mSensor[handle].driver == mSensor[i].driver)
                    mSensor[i].driver = NULL;
            }
        }
    }
    for (i = 0; i <= numFds; i++) {
        if (mPollFds[i].fd >= 0)
            close(mPollFds[i].fd);
    }
    close(mWritePipeFd);
}

int sensors_poll_context_t::activate(int handle, int enabled)
{
    VFUNC_LOG;
    int ret;

    if ((handle < 1) || (handle > numSensors))
        return -EINVAL;

    if (mSensor[handle].driver == NULL)
        return 0;

    if (mSensor[handle].fusionClient)
        ret = mSensor[0].driver->enable(handle, enabled);
    else
        ret = mSensor[handle].driver->enable(handle, enabled);
    if (!ret) {
        const char wakeMessage(WAKE_MESSAGE);
        if (write(mWritePipeFd, &wakeMessage, 1) < 0)
            ALOGE("error sending wake message (%s)", strerror(errno));
        /* poll should wait indefinitely for interrupt based sensors
         * and poll should not block others, hence timeout is required
         */
        if (mSensor[handle].fd != -1)
            return 0;

        mSensor[handle].enabled = enabled;
        if (!enabled)
            mSensor[handle].pollTime = UINT_MAX;
        setPollTime();
    } else {
        ALOGE("%s: ERR=%d handle=%d enabled=%d", __func__, ret, handle, enabled);
    }
    return ret;
}

int sensors_poll_context_t::setDelay(int handle, int64_t ns)
{
    VFUNC_LOG;
    int ret;

    if ((handle < 1) || (handle > numSensors))
        return -EINVAL;

    if (mSensor[handle].driver == NULL)
        return 0;

    if (mSensor[handle].fusionClient)
        ret = mSensor[0].driver->setDelay(handle, ns);
    else
        ret = mSensor[handle].driver->setDelay(handle, ns);
    if (!ret) {
        if (mSensor[handle].fd != -1)
            return 0;

        unsigned int ms = ns / 1000000;
        if (mSensor[handle].enabled && (mSensor[handle].pollTime > ms))
            mSensor[handle].pollTime = ms;
        setPollTime();
    }
    return ret;
}

int sensors_poll_context_t::batch(int handle, int flags, int64_t period_ns,
                                  int64_t timeout)
{
    VFUNC_LOG;
    int ret;

    if ((handle < 1) || (handle > numSensors))
        return -EINVAL;

    if (mSensor[handle].driver == NULL) {
        if (timeout || (flags & SENSORS_BATCH_WAKE_UPON_FIFO_FULL))
            return -EINVAL;

        return 0;
    }

    if (mSensor[handle].fusionClient)
        ret = mSensor[0].driver->batch(handle, flags, period_ns, timeout);
    else
        ret = mSensor[handle].driver->batch(handle, flags, period_ns, timeout);
    if (!ret) {
        if (mSensor[handle].fd != -1)
            return 0;

        unsigned int ms = timeout / 1000000;
        if (mSensor[handle].enabled && (mSensor[handle].pollTime > ms))
            mSensor[handle].pollTime = ms;
        setPollTime();
    }
    return ret;
}

int sensors_poll_context_t::flush(int handle)
{
    VFUNC_LOG;
    int ret;

    if ((handle < 1) || (handle > numSensors))
        return -EINVAL;

    if (mSensor[handle].driver == NULL)
        return -EINVAL;

    if (mSensor[handle].fusionClient)
        ret = mSensor[0].driver->flush(handle);
    else
        ret = mSensor[handle].driver->flush(handle);
    return ret;
}

int sensors_poll_context_t::pollEvents(sensors_event_t *data, int count)
{
    VHANDLER_LOG;
    int i;
    int handle;
    int n;
    int nEvents = 0;
    int nPoll = 0;

    do {
        for (i = 0; count && i < numFds; i++) {
            if (mPollFds[i].revents & (POLLIN | POLLPRI)) {
                handle = mFdHandles[i];
                if (mSensor[handle].fusionClient)
                    n = mSensor[0].driver->readEvents(data, count, handle);
                else
                    n = mSensor[handle].driver->readEvents(data, count, handle);
                if (n < count)
                    /* no more data for this sensor */
                    mPollFds[i].revents = 0;
                if (n > 0) {
                    count -= n;
                    nEvents += n;
                    data += n;
                }
            }
        }
        /* The hasPendingEvents is processed after the file descriptors for
         * the case where hasPendingEvents is dynamic and depends on file
         * desciptor data.  Fusion may use this to gather all the data for
         * processing before sending upstream.
         */
        for (i = 0; count && i < numDrivers; i++) {
            handle = mDrvHandles[i];
            if (mSensor[handle].driver->hasPendingEvents()) {
                if (mSensor[handle].fusionClient)
                    n = mSensor[0].driver->readEvents(data, count, handle);
                else
                    n = mSensor[handle].driver->readEvents(data, count);
                if (n > 0) {
                    count -= n;
                    nEvents += n;
                    data += n;
                }
            }
        }
        if (count) {
            /* we still have some room, so try to see if we can get
             * some events immediately or just wait for polltime
             * if we don't have anything to return
             */
            nPoll = poll(mPollFds, numFds, nEvents ? 0 : polltime);
            if (nPoll < 0) {
                if (errno == EINTR) {
                    return nEvents;
                } else {
                    ALOGE("poll() failed (%s)", strerror(errno));
                    return -errno;
                }
            }

            if (mPollFds[wake].revents & (POLLIN | POLLPRI)) {
                char msg;
                int result = read(mPollFds[wake].fd, &msg, 1);
                ALOGE_IF(result < 0, "error reading from wake pipe (%s)", strerror(errno));
                ALOGE_IF(msg != WAKE_MESSAGE, "unknown message on wake queue (0x%02x)", int(msg));
                mPollFds[wake].revents = 0;
            }
        }
    } while ((nPoll || !nEvents) && count);

    return nEvents;
}

static int poll__close(struct hw_device_t *dev)
{
    VFUNC_LOG;
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;

    if (ctx)
        delete ctx;
    return 0;
}

static int poll__activate(struct sensors_poll_device_t *dev,
                          int handle, int enabled)
{
    VFUNC_LOG;
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;

    return ctx->activate(handle, enabled);
}

static int poll__setDelay(struct sensors_poll_device_t *dev,
                          int handle, int64_t ns)
{
    VFUNC_LOG;
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;

    return ctx->setDelay(handle, ns);
}

static int poll__poll(struct sensors_poll_device_t *dev,
                      sensors_event_t *data, int count)
{
    VFUNC_LOG;
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;

    return ctx->pollEvents(data, count);
}

static int poll__batch(struct sensors_poll_device_1 *dev, int handle,
                       int flags, int64_t period_ns, int64_t timeout)
{
    VFUNC_LOG;
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;

    return ctx->batch(handle, flags, period_ns, timeout);
}

static int poll__flush(struct sensors_poll_device_1 *dev, int handle)
{
    VFUNC_LOG;
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;

    return ctx->flush(handle);
}


/** Open a new instance of a sensor device using name */
static int open_sensors(const struct hw_module_t *module, const char *id,
                        struct hw_device_t **device)
{
    VFUNC_LOG;
    sensors_poll_context_t *dev = new sensors_poll_context_t();

    memset(&dev->device, 0, sizeof(sensors_poll_device_1));
    dev->device.common.tag = HARDWARE_DEVICE_TAG;
#ifdef SENSORS_DEVICE_API_VERSION
    dev->device.common.version  = SENSORS_DEVICE_API_VERSION;
#else /* #ifdef SENSORS_DEVICE_API_VERSION */
    dev->device.common.version  = devDeviceCommonVersion;
#endif /* #ifdef SENSORS_DEVICE_API_VERSION */
    dev->device.common.module   = const_cast<hw_module_t*>(module);
    dev->device.common.close    = poll__close;
    dev->device.activate        = poll__activate;
    dev->device.setDelay        = poll__setDelay;
    dev->device.poll            = poll__poll;
    dev->device.batch           = poll__batch;
    dev->device.flush           = poll__flush;
    *device = &dev->device.common;
    return 0;
}

