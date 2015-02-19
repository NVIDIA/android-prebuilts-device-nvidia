/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Copyright (c) 2014, NVIDIA Corporation. All Rights Reserved.
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


#ifndef ANDROID_BMPX80_SENSOR_H
#define ANDROID_BMPX80_SENSOR_H

#include "sensors.h"
#include "SensorBase.h"

#define BMP180_DEV_NAME         "bmpX80"

class Bmpx80Pressure : public SensorBase {
private:
    static int inputDevPathNum(const char *dev_name);

public:
    static void fillSensorDef(sensor_t *ssensor_list, int &curIndex);
};

#endif //ANDROID_BMPX80_SENSOR_H

