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

/* NVSBuffer is the interface between NVS and the IIO kernel. */
/* The NVS (NVidia Sensor) implementation of scan_elements enable/disable works
 * using the IIO mechanisms with one additional NVS enhancement for disable:
 * To enable, the NVS HAL will:
 * 1. Disable buffer
 * 2. Enable channels
 * 3. Calculate buffer alignments based on enabled channels
 * 4. Enable buffer
 * It is expected that the NVS kernel driver will detect the channels enabled
 * and enable the device using the IIO iio_buffer_setup_ops.
 * To disable, the NVS HAL will:
 * 1. Disable buffer
 * 2. Disable channels
 * 3. Calculate buffer alignments based on remaining enabled channels, if any
 * 4. If (one or more channels are enabled)
 *        4a. Enable buffer
 *    else
 *        4b. Disable master enable
 * It is expected that the master enable will be enabled as part of the
 * iio_buffer_setup_ops.
 * The NVS sysfs attribute for the master enable is "enable".
 * The master enable is an enhancement of the standard IIO enable/disable
 * procedure.  Consider a device that contains multiple sensors.  To enable all
 * the sensors with the standard IIO enable mechanism, the device would be
 * powered off and on for each sensor that was enabled.  By having a master
 * enable, the device does not have to be powerered down for each buffer
 * disable but only when the master enable is disabled, thereby improving
 * efficiency.
 */


#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include "Nvsb.h"


Nvsb::Nvsb(Nvs *link, int fd, char *sysfsPath)
    : mFd(fd),
      mSysfsPath(sysfsPath),
      mLinkRoot(link),
      mLink(NULL),
      mChannelInfo(NULL),
      mNumChannels(0),
      mBufSize(0),
      mBuf(NULL),
      mPathBufLength(NULL),
      mPathBufEnable(NULL),
      mPathEnable(NULL)
{
    DIR *dp;
    FILE *fp;
    const struct dirent *d;
    struct iioChannelInfo tmpChanInf;
    struct iioChannelInfo *chan;
    char pathScan[NVS_IIO_SYSFS_PATH_SIZE_MAX];
    char path[NVS_IIO_SYSFS_PATH_SIZE_MAX];
    int i;
    int j;
    int ret;

    /* open scan_elements */
    sprintf(pathScan, "%s/scan_elements", sysfsPath);
    dp = opendir(pathScan);
    if (dp == NULL)
        return;

    /* count number of channels */
    while (d = readdir(dp), d != NULL) {
        if (!(strcmp(d->d_name + strlen(d->d_name) - strlen("_en"), "_en")))
            mNumChannels++;
    }
    if (!mNumChannels)
        goto errExit;

    /* allocate channel info memory */
    mChannelInfo = new iioChannelInfo[mNumChannels];
    memset(mChannelInfo, 0, sizeof(struct iioChannelInfo) * mNumChannels);
    /* create master enable path */
    ret = asprintf(&mPathEnable, "%s/enable", sysfsPath);
    if (ret < 0)
        goto errExit;

    /* create buffer enable path */
    ret = asprintf(&mPathBufEnable, "%s/buffer/enable", sysfsPath);
    if (ret < 0)
        goto errExit;

    /* create buffer length path */
    ret = asprintf(&mPathBufLength, "%s/buffer/length", sysfsPath);
    if (ret < 0)
        goto errExit;

    /* build channel info */
    rewinddir(dp);
    i = 0;
    while (d = readdir(dp), d != NULL) {
        if (!(strcmp(d->d_name + strlen(d->d_name) - strlen("_en"), "_en"))) {
            chan = &mChannelInfo[i++];
            /* channel sysfs enable path */
            ret = asprintf(&chan->pathEn,
                           "%s/%s", pathScan, d->d_name);
            if (ret < 0)
                goto errExit;

            /* init channel enable status */
            fp = fopen(chan->pathEn, "r");
            if (fp == NULL)
                goto errExit;

            fscanf(fp, "%u", &chan->enabled);
            fclose(fp);
            /* channel full name */
            chan->fullName = strndup(d->d_name,
                                     strlen(d->d_name) - strlen("_en"));
            if (chan->fullName != NULL) {
                char *str;
                char *dir;
                char *name;
                char *mod;

                /* channel dir name */
                str = strdup(chan->fullName);
                if (str == NULL)
                    goto errExit;

                dir = strtok(str, "_\0");
                /* channel name */
                name = strtok(NULL, "_\0");
                if (name == NULL) {
                    free(str);
                    goto errExit;
                }

                ret = asprintf(&chan->dirName, "%s_%s", dir, name);
                if (ret < 0) {
                    free(str);
                    goto errExit;
                }

                mod = strtok(NULL, "\0");
                if (mod == NULL)
                    chan->name = strdup(name);
                else
                    ret = asprintf(&chan->name, "%s_%s", name, mod);
                free(str);
                if (ret < 0)
                    goto errExit;

            } else {
                goto errExit;
            }

            /* channel index */
            sprintf(path, "%s/%s_index", pathScan, chan->fullName);
            fp = fopen(path, "r");
            if (fp == NULL)
                goto errExit;

            fscanf(fp, "%u", &chan->index);
            fclose(fp);
            /* channel type info */
            sprintf(path, "%s/%s_type", pathScan, chan->fullName);
            fp = fopen(path, "r");
            if (fp != NULL) {
                char endian;
                char sign;

                fscanf(fp, "%ce:%c%u/%u>>%u",
                       &endian,
                       &sign,
                       &chan->realbits,
                       &chan->bytes,
                       &chan->shift);
                fclose(fp);
                chan->bytes /= 8;
                if (endian == 'b')
                    chan->bigendian = 1;
                else
                    chan->bigendian = 0;
                if (sign == 's')
                    chan->sign = 1;
                else
                    chan->sign = 0;
                if (chan->realbits == 64)
                    chan->mask = ~0;
                else
                    chan->mask = (1LL << chan->realbits) - 1;
            } else {
                goto errExit;
            }

            if (strcmp(chan->name, strTimestamp)) {
                /* IIO sysfs attribute paths */
                for (j = 0; j < ATTR_N; j++) {
                    chan->attrFVal[j] = 0;
                    sprintf(path, "%s/%s_%s", mSysfsPath,
                            chan->dirName, attrPathTbl[j].attrName);
                    ret = access(path, F_OK);
                    if (!ret) {
                        chan->attrShared[j] = true;
                        chan->attrPath[j] = strdup(path);
                        continue;
                    }

                    chan->attrShared[j] = false;
                    sprintf(path, "%s/%s_%s", mSysfsPath,
                            chan->fullName, attrPathTbl[j].attrName);
                    ret = access(path, F_OK);
                    if (!ret) {
                        chan->attrPath[j] = strdup(path);
                        continue;
                    }

                    chan->attrPath[j] = NULL;
                }

                /* read offset value */
                ret = sysfsFloatRead(chan->attrPath[ATTR_OFFSET],
                                     &chan->attrFVal[ATTR_OFFSET]);
                if (ret)
                    ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                             "%s: %s=read ERR: %d\n",
                             __func__, chan->attrPath[ATTR_OFFSET], ret);
                else
                    chan->attrCached[ATTR_OFFSET] = true;
                /* read scale value */
                ret = sysfsFloatRead(chan->attrPath[ATTR_SCALE],
                                     &chan->attrFVal[ATTR_SCALE]);
                if (ret)
                    ALOGI_IF(SensorBase::EXTRA_VERBOSE,
                             "%s: %s=read ERR: %d\n",
                             __func__, chan->attrPath[ATTR_SCALE], ret);
                else
                    chan->attrCached[ATTR_SCALE] = true;
            } else {
                /* make sure timestamp is enabled */
                sysfsIntWrite(chan->pathEn, 1);
            }
        }
    }

    closedir(dp);
    /* reorder so that the array is in index order */
    for (i = 0; i < mNumChannels; i++) {
        for (j = 0; j < (mNumChannels - 1); j++) {
            if (mChannelInfo[j].index > mChannelInfo[j + 1].index) {
                tmpChanInf = mChannelInfo[j + 1];
                mChannelInfo[j + 1] = mChannelInfo[j];
                mChannelInfo[j] = tmpChanInf;
            }
        }
    }
    /* scan alignments and buffer size */
    j = 0;
    for (i = 0; i < mNumChannels; i++) {
        if (!(j % mChannelInfo[i].bytes))
            mChannelInfo[i].location = j;
        else
            mChannelInfo[i].location = j - j % mChannelInfo[i].bytes +
                                          mChannelInfo[i].bytes;
        j = mChannelInfo[i].location + mChannelInfo[i].bytes;
    }
    mBufSize = j;
    mBuf = new __u8[mBufSize];
    if (SensorBase::EXTRA_VERBOSE)
        bufDbgChan();
    return;

errExit:
    NvsbRemove();
    closedir(dp);
}

Nvsb::~Nvsb()
{
    NvsbRemove();
}

void Nvsb::NvsbRemove()
{
    int i;
    int j;

    if (mPathEnable)
        free(mPathEnable);
    if (mPathBufEnable)
        free(mPathBufEnable);
    if (mPathBufLength)
        free(mPathBufLength);
    if (mChannelInfo) {
        for (i = 0; i < mNumChannels; i++) {
            if (mChannelInfo[i].name != NULL)
                free(mChannelInfo[i].name);
            if (mChannelInfo[i].dirName != NULL)
                free(mChannelInfo[i].dirName);
            if (mChannelInfo[i].fullName != NULL)
                free(mChannelInfo[i].fullName);
            if (mChannelInfo[i].pathEn != NULL)
                free(mChannelInfo[i].pathEn);
            for (j = 0; j < ATTR_N; j++) {
                if (mChannelInfo[i].attrPath[j] != NULL)
                    free(mChannelInfo[i].attrPath[j]);
            }
        }
        delete mChannelInfo;
    }
    delete mBuf;
}

void Nvsb::bufSetLinkRoot(Nvs* link)
{
    mLinkRoot = link;
}

int Nvsb::bufDisable()
{
    int ret;

    ret = sysfsIntWrite(mPathBufEnable, 0);
    if (ret)
        ALOGE("%s 0 -> %s ERR: %d\n", __func__, mPathBufEnable, ret);
    return ret;
}

int Nvsb::bufEnable(bool force)
{
    bool bufEn = false;
    int i;
    int bytes = 0;
    int ret = 0;

    /* calculate alignments and buffer size */
    if (mNumChannels) {
        for (i = 0; i < mNumChannels; i++) {
            /* don't enable buffer if just timestamp (handle = 0) */
            if (mChannelInfo[i].enabled && mChannelInfo[i].handle)
                bufEn = true;
            if (mChannelInfo[i].enabled) {
                if (!(bytes % mChannelInfo[i].bytes))
                    mChannelInfo[i].location = bytes;
                else
                    mChannelInfo[i].location = bytes - bytes %
                                                  mChannelInfo[i].bytes +
                                                  mChannelInfo[i].bytes;
                bytes = mChannelInfo[i].location + mChannelInfo[i].bytes;
            }
        }
    }
    mBufSize = bytes;
    /* if something is enabled then enable buffer or just force on anyway */
    if (bufEn || force) {
        mLink = NULL; /* reset buffer fill flag */
        ret = sysfsIntWrite(mPathBufEnable, 1);
    } else {
        /* mPathEnable is for NVS and may not be supported hence no ret err */
        sysfsIntWrite(mPathEnable, 0); /* turn device power off */
    }
    if (ret)
        ALOGE("%s 1 -> %s ERR: %d\n", __func__, mPathBufEnable, ret);
    return ret;
}

int Nvsb::bufSetLength(int length)
{
    int ret;
    int ret_t;

    ret_t = bufDisable();
    ret = sysfsIntWrite(mPathBufLength, length);
    if (ret) {
        ALOGE("%s %d -> %s ERR: %d\n", __func__, length, mPathBufLength, ret);
        ret_t |= ret;
    }
    ret_t |= bufEnable(false);
    return ret_t;
}

int Nvsb::bufChanAble(int32_t handle, int channel, int enable)
{
    bool err = true;
    int i;
    int ret;
    int ret_t;

    if (!mNumChannels) {
        ALOGE("%s ERR: -ENODEV\n", __func__);
        return -ENODEV;
    }

    /* disable buffer for any change */
    ret_t = bufDisable();
    if (ret_t)
        return ret_t;

    if ((handle > 0) && (channel < 0)) {
        /* enable/disable matching handle(s) */
        for (i = 0; i < mNumChannels; i++) {
            if (mChannelInfo[i].handle == handle) {
                err = false;
                ret = sysfsIntWrite(mChannelInfo[i].pathEn, enable);
                if (ret) {
                    ALOGE("%s %d -> %s ERR: %d\n",
                          __func__, enable, mChannelInfo[i].pathEn, ret);
                    ret_t |= ret;
                } else {
                    mChannelInfo[i].enabled = enable;
                }
            }
        }
        if (err) {
            ALOGE("%s ERR: handle not found\n", __func__);
            ret_t = -EINVAL;
        }
    } else if ((channel >= 0) && (channel < mNumChannels)) {
        /* enable/disable single channel */
        ret_t = sysfsIntWrite(mChannelInfo[channel].pathEn, enable);
        if (ret_t)
            ALOGE("%s %d -> %s ERR: %d\n",
                  __func__, enable, mChannelInfo[channel].pathEn, ret_t);
        else
            mChannelInfo[channel].enabled = enable;
    } else {
        ALOGE("%s ERR: -EINVAL handle=%d  channel=%d  enable=%d\n",
              __func__, handle, channel, enable);
        ret_t = -EINVAL;
    }
    ret_t |= bufEnable(false);
    return ret_t;
}

int Nvsb::bufFill(sensors_event_t* data, int count, int32_t handle)
{
    int n;
    int nEvents = 0;

    while (count) {
        if (mLink == NULL) {
            n = read(mFd, mBuf, mBufSize);
            if (SensorBase::ENG_VERBOSE &&
                ((mDbgFdN != n) || (mDbgBufSize != mBufSize))) {
                ALOGI("%s read %d bytes from fd %d (buffer size=%d)\n",
                      __func__, n, mFd, mBufSize);
                mDbgFdN = n;
                mDbgBufSize = mBufSize;
            }
            if (n < mBufSize)
                break;

            if (SensorBase::ENG_VERBOSE) {
                for (n = 0; n < mBufSize; n++)
                    ALOGI("%s buf byte %d=%x\n", __func__, n, mBuf[n]);
                if (mBufSize > (int)sizeof(__s64)) {
                    __s64 *ts = (__s64 *)mBuf;
                    ALOGI("%s timestamp=%lld (ts buffer index=%d)\n",
                          __func__, ts[(mBufSize / 8) - 1], mBufSize - 8);
                }
            }
            mLink = mLinkRoot;
        }
        n = mLink->processEvent(data, mBuf, handle);
        if (n > 0) {
            data++;
            nEvents++;
            count--;
        }
        mLink = mLink->getLink();
    }

    return nEvents;
}

int Nvsb::bufData(struct iioChannelInfo *chanInf, __u8 *buf, void *data)
{
    if (chanInf->sign) {
        switch (chanInf->bytes) {
        case 1:
            __s8 s8Data;

            s8Data = *((__s8 *)(buf + chanInf->location));
            s8Data >>= chanInf->shift;
            s8Data &= chanInf->mask;
            s8Data = (__s8)(s8Data << (8 - chanInf->realbits)) >>
                           (8 - chanInf->realbits);
            *(float *)data = (float)s8Data;
            return sizeof(float);

        case 2:
            __s16 s16Data;

            s16Data = *((__s16 *)(buf + chanInf->location));
            s16Data >>= chanInf->shift;
            s16Data &= chanInf->mask;
            s16Data = (__s16)(s16Data << (16 - chanInf->realbits)) >>
                             (16 - chanInf->realbits);
            *(float *)data = (float)s16Data;
            return sizeof(float);

        case 4:
            __s32 s32Data;

            s32Data = *((__s32 *)(buf + chanInf->location));
            s32Data >>= chanInf->shift;
            s32Data &= chanInf->mask;
            s32Data = (__s32)(s32Data << (32 - chanInf->realbits)) >>
                             (32 - chanInf->realbits);
            *(float *)data = (float)s32Data;
            return sizeof(float);

        case 8:
            __s64 s64Data;

            s64Data = *((__s64 *)(buf + chanInf->location));
            s64Data >>= chanInf->shift;
            s64Data &= chanInf->mask;
            s64Data = (__s64)(s64Data << (64 - chanInf->realbits)) >>
                             (64 - chanInf->realbits);
            *(uint64_t *)data = (uint64_t)s64Data;
            return sizeof(uint64_t);

        default:
            ALOGE("%s ERR: channel bytes=%u not supported\n",
                  __func__, chanInf->bytes);
            break;
        }
    } else {
        switch (chanInf->bytes) {
        case 1:
            __u8 u8Data;

            u8Data = *((__u8 *)(buf + chanInf->location));
            u8Data >>= chanInf->shift;
            u8Data &= chanInf->mask;
            *(float *)data = (float)u8Data;
            return sizeof(float);

        case 2:
            __u16 u16Data;

            u16Data = *((__u16 *)(buf + chanInf->location));
            u16Data >>= chanInf->shift;
            u16Data &= chanInf->mask;
            *(float *)data = (float)u16Data;
            return sizeof(float);

        case 4:
            __u32 u32Data;

            u32Data = *((__u32 *)(buf + chanInf->location));
            u32Data >>= chanInf->shift;
            u32Data &= chanInf->mask;
            *(float *)data = (float)u32Data;
            return sizeof(float);

        case 8:
            __u64 u64Data;

            u64Data = *((__u64 *)(buf + chanInf->location));
            u64Data >>= chanInf->shift;
            u64Data &= chanInf->mask;
            *(uint64_t *)data = (uint64_t)u64Data;
            return sizeof(uint64_t);

        default:
            ALOGE("%s ERR: channel bytes=%u not supported\n",
                  __func__, chanInf->bytes);
            break;
        }
    }
    return -EINVAL;
}

struct iioChannelInfo *Nvsb::bufGetChanInfo(char *name)
{
    int i;

    if (!mNumChannels) {
        ALOGE("%s ERR: no device channels\n", __func__);
        return NULL;
    }

    for (i = 0; i < mNumChannels; i++) {
        if (!(strcmp(name, mChannelInfo[i].name)))
            return &mChannelInfo[i];
    }
    ALOGE("%s ERR: channel %s not found\n", __func__, name);
    return NULL;
}

void Nvsb::bufDbgChan()
{
    int i;
    int j;

    ALOGI("%s %s Number of channels=%d  Buffer size=%d bytes\n",
          __func__, mSysfsPath, mNumChannels, mBufSize);
    for (i = 0; i < mNumChannels; i++) {
        ALOGI("------------------------------------\n");
        ALOGI("channel[%d].handle=%d\n",
              i, mChannelInfo[i].handle);
        ALOGI("channel[%d].name=%s\n",
              i, mChannelInfo[i].name);
        ALOGI("channel[%d].dirName=%s\n",
              i, mChannelInfo[i].dirName);
        ALOGI("channel[%d].fullName=%s\n",
              i, mChannelInfo[i].fullName);
        ALOGI("channel[%d].pathEn=%s\n",
              i, mChannelInfo[i].pathEn);
        ALOGI("channel[%d].enabled=%u\n",
              i, mChannelInfo[i].enabled);
        ALOGI("channel[%d].index=%u\n",
              i, mChannelInfo[i].index);
        ALOGI("channel[%d].bigendian=%u\n",
              i, mChannelInfo[i].bigendian);
        ALOGI("channel[%d].sign=%u\n",
              i, mChannelInfo[i].sign);
        ALOGI("channel[%d].realbits=%u\n",
              i, mChannelInfo[i].realbits);
        ALOGI("channel[%d].shift=%u\n",
              i, mChannelInfo[i].shift);
        ALOGI("channel[%d].bytes=%u\n",
              i, mChannelInfo[i].bytes);
        ALOGI("channel[%d].mask=0x%llx\n",
              i, mChannelInfo[i].mask);
        ALOGI("channel[%d].location=%u\n",
              i, mChannelInfo[i].location);
        for (j = 0; j < ATTR_N; j++)
            ALOGI("channel[%d] %s path %s=%f (shared=%x)\n",
                  i, attrPathTbl[j].attrName, mChannelInfo[i].attrPath[j],
                  mChannelInfo[i].attrFVal[j], mChannelInfo[i].attrShared[j]);
    }
}

int sysfsStrRead(char *path, char *str)
{
    char buf[64] = {0};
    int fd;
    int ret;

    if (path == NULL)
        return -EINVAL;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -ENOENT;

    ret = read(fd, buf, sizeof(buf));
    close(fd);
    if (ret > 0) {
        strcpy(str, buf);
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %s=%s  ret=%d\n",
                 __func__, path, str, ret);
    } else {
        ALOGE_IF(SensorBase::SYSFS_VERBOSE, "%s: %s=ERR: %d  ret=%d\n",
                 __func__, path, -errno, ret);
        ret = -EIO;
    }
    return ret;
}

int sysfsFloatRead(char *path, float *fVal)
{
    char buf[32] = {0};
    int fd;
    int ret;

    if (path == NULL)
        return -EINVAL;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -ENOENT;

    ret = read(fd, buf, sizeof(buf));
    close(fd);
    if (ret > 0) {
        *fVal = atof(buf);
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %s=%f  ret=%d\n",
                 __func__, path, *fVal, ret);
        ret = 0;
    } else {
        ALOGE_IF(SensorBase::SYSFS_VERBOSE, "%s: %s=ERR: %d  ret=%d\n",
                 __func__, path, -errno, ret);
        if (!ret)
            ret = -EIO;
    }
    return ret;
}

int sysfsFloatWrite(char *path, float fVal)
{
    char buf[32] = {0};
    int fd;
    int ret;

    if (path == NULL)
        return -EINVAL;

    fd = open(path, O_RDWR);
    if (fd < 0)
        return -ENOENT;

    sprintf(buf, "%f", fVal);
    ret = write(fd, buf, sizeof(buf));
    close(fd);
    if (ret > 0) {
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %f -> %s ret=%d\n",
                 __func__, fVal, path, ret);
        ret = 0;
    } else {
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %f -> %s ret=%d ERR: %d\n",
                 __func__, fVal, path, ret, -errno);
        if (!ret)
            ret = -EIO;
    }
    return ret;
}

int sysfsIntRead(char *path, int *iVal)
{
    char buf[16] = {0};
    int fd;
    int ret;

    if (path == NULL)
        return -EINVAL;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -ENOENT;

    ret = read(fd, buf, sizeof(buf));
    close(fd);
    if (ret > 0) {
        *iVal = atoi(buf);
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %s=%d  ret=%d\n",
                 __func__, path, *iVal, ret);
        ret = 0;
    } else {
        ALOGE_IF(SensorBase::SYSFS_VERBOSE, "%s: %s=ERR: %d  ret=%d\n",
                 __func__, path, -errno, ret);
        if (!ret)
            ret = -EIO;
    }
    return ret;
}

int sysfsIntWrite(char *path, int iVal)
{
    char buf[16] = {0};
    int fd;
    int ret;

    if (path == NULL)
        return -EINVAL;

    fd = open(path, O_RDWR);
    if (fd < 0)
        return -ENOENT;

    sprintf(buf, "%d", iVal);
    ret = write(fd, buf, sizeof(buf));
    close(fd);
    if (ret > 0) {
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %d -> %s ret=%d\n",
                 __func__, iVal, path, ret);
        ret = 0;
    } else {
        ALOGI_IF(SensorBase::SYSFS_VERBOSE, "%s: %d -> %s ret=%d ERR: %d\n",
                 __func__, iVal, path, ret, -errno);
        if (!ret)
            ret = -EIO;
    }
    return ret;
}

