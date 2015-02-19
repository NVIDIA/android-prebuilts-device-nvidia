/* Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (c) 2008 Jonathan Cameron
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


#ifndef NVSB_H
#define NVSB_H

#include "Nvs.h"

struct iioChannelInfo {
    int handle;                         /* sensor handle */
    char *name;                         /* iio_chan_type_name_spec( + MOD) */
    char *dirName;                      /* DIR_name */
    char *fullName;                     /* DIR_name(_MOD) */
    char *pathEn;                       /* channel enable sysfs path */
    unsigned int enabled;               /* channel enable status */
    unsigned int index;                 /* channel index */
    unsigned int bigendian;             /* scan_elements/<full_name>_type */
    unsigned int sign;                  /* scan_elements/<full_name>_type */
    unsigned int realbits;              /* scan_elements/<full_name>_type */
    unsigned int shift;                 /* scan_elements/<full_name>_type */
    unsigned int bytes;                 /* number of channel data bytes */
    uint64_t mask;                      /* mask for valid data */
    unsigned int location;              /* data alignment in buffer */
    char *attrPath[ATTR_N];             /* sysfs attribute paths */
    bool attrShared[ATTR_N];            /* if sysfs attribute is chan shared */
    bool attrCached[ATTR_N];            /* attribute is cached */
    float attrFVal[ATTR_N];             /* float read from sysfs attribute */
};

class Nvs;

class Nvsb
{
    int mFd;
    char *mSysfsPath;
    Nvs *mLinkRoot;
    Nvs *mLink;
    struct iioChannelInfo *mChannelInfo;
    int mNumChannels;
    int mBufSize;
    __u8 *mBuf;
    char *mPathBufLength;
    char *mPathBufEnable;
    char *mPathEnable;
    int mDbgFdN;
    int mDbgBufSize;

    void NvsbRemove();

public:
    Nvsb(Nvs *link, int fd, char *sysfsPath);
    ~Nvsb();

    void bufSetLinkRoot(Nvs *link);
    struct iioChannelInfo *bufGetChanInfo(char *name);
    int bufSetLength(int length);
    int bufDisable();
    int bufEnable(bool force);
    int bufChanAble(int32_t handle, int channel, int enable);
    int bufFill(sensors_event_t *data, int count, int32_t handle);
    int bufData(struct iioChannelInfo *chanInf, __u8 *buf, void *data);
    void bufDbgChan();
};

int sysfsStrRead(char *path, char *str);
int sysfsFloatRead(char *path, float *fVal);
int sysfsFloatWrite(char *path, float fVal);
int sysfsIntRead(char *path, int *iVal);
int sysfsIntWrite(char *path, int iVal);

#endif  // NVSB_H

