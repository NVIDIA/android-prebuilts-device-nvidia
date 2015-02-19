/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Copyright (c) 2014, NVIDIA CORPORATION. All Rights Reserved.
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
#include <stdlib.h>
#include <linux/input.h>
#include <hardware/sensors.h>

#include "bmpx80.h"
#include "SensorUtil.h"
#include "nvs_input.h"

/*****************************************************************************/

void Bmpx80Pressure::fillSensorDef(sensor_t *ssensor_list, int &curIndex)
{
    if (inputDevPathNum(BMP180_DEV_NAME) < 0)
        return;

    sensor_t *sensor_def = &ssensor_list[curIndex];
    sensor_def->name = "MPL Pressure   ";
    sensor_def->vendor = "Invensense";
    sensor_def->version = 1;
    sensor_def->maxRange = 110000.0f;
    sensor_def->handle = ID_AP;
    sensor_def->type = SENSOR_TYPE_PRESSURE;
    sensor_def->resolution = 1.0f;
    sensor_def->power = 0.032f;
    sensor_def->minDelay = 25500;
    curIndex++;
}

int Bmpx80Pressure::inputDevPathNum(const char *dev_name)
{
    char name[32] = {0};
    char path[80];
    int i;
    int fd;
    int err;

    i = 0;
    while(1)
    {
        sprintf(path, "/sys/class/input/input%d/name", i);
        err = access(path, F_OK);
        if (err < 0) {
            break;
        } else {
            fd = open(path, O_RDONLY);
            if (fd >= 0) {
                memset(name, 0, sizeof(name));
                read(fd, name, sizeof(name));
                close(fd);
                if (!strncmp(name, dev_name, strlen(dev_name))) {
                    ALOGI("%s %s %s found", __func__, path, dev_name);
                    err = i;
                    break;
                }
            }
        }
        i++;
    }

    return err;
}

/*****************************************************************************/
