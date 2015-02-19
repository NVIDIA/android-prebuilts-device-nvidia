/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ANDROID_SENSOR_BASE_H
#define ANDROID_SENSOR_BASE_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <poll.h>
#include <math.h>
#include <time.h>


#define FUNC_LOG \
            ALOGV("%s", __PRETTY_FUNCTION__)
#define VFUNC_LOG \
            ALOGV_IF(SensorBase::FUNC_ENTRY, \
                     "Entering function '%s'", __PRETTY_FUNCTION__)
#define VHANDLER_LOG \
            ALOGV_IF(SensorBase::HANDLER_ENTRY, \
                     "Entering handler '%s'", __PRETTY_FUNCTION__)
#define CALL_MEMBER_FN(pobject, ptrToMember) ((pobject)->*(ptrToMember))

#define MAX_SYSFS_NAME_LEN  (100)
#define IIO_BUFFER_LENGTH   (480)


struct sensors_event_t;

class SensorBase {
public:
    /* Log enablers, each of these independent */
    static bool PROCESS_VERBOSE;   /* process log messages */
    static bool EXTRA_VERBOSE;     /* verbose log messages */
    static bool SYSFS_VERBOSE;     /* log sysfs interactions as cat/echo for
                                      repro purpose on a shell */
    /* Note that enabling this logs may affect performance */
    static bool FUNC_ENTRY;        /* log entry in all one-time functions */
    static bool HANDLER_ENTRY;     /* log entry in all handler functions */
    static bool ENG_VERBOSE;       /* log a lot more info about the internals */
    static bool INPUT_DATA;        /* log the data input from the events */
    static bool HANDLER_DATA;      /* log the data fetched from the handlers */
    static bool DEBUG_BATCHING;    /* log the data for debugging batching */

protected:
    const char *dev_name;
    const char *data_name;
    char input_name[PATH_MAX];
    int dev_fd;
    int data_fd;

    int openInput(const char* inputName);
    static int64_t getTimestamp();
    static int64_t timevalToNano(timeval const& t) {
        return t.tv_sec * 1000000000LL + t.tv_usec * 1000;
    }

    int open_device();
    int close_device();

public:
    SensorBase(const char *dev_name, const char *data_name);
    virtual ~SensorBase();

    virtual int enable(int32_t handle, int enabled, int channel = -1)
                { return 0; };
    virtual int setDelay(int32_t handle, int64_t ns, int channel = -1)
                { return 0; };
    virtual int batch(int handle, int flags,
                      int64_t period_ns, int64_t timeout);
    virtual int flush(int handle)
                { return -EINVAL; };
    virtual int getSensorList(struct sensor_t *list, int index, int limit)
                { return 0; };
    virtual int getFd(int32_t handle = -1);
    virtual int initFd(struct pollfd *pollFd, int32_t handle = -1)
                { return -1; };
    virtual int readEvents(sensors_event_t *data, int count,
                           int32_t handle = -1) = 0;
    virtual int readRaw(int32_t handle, int *data, int channel = -1)
                { return -1; };
    virtual bool hasPendingEvents()
                 { return false; };
    virtual int64_t now_ns()
                    { return 0;};
};

#endif  // ANDROID_SENSOR_BASE_H
