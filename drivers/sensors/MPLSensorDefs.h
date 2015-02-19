/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2011 Invensense, Inc.
 * Copyright (c) 2013-2014 NVIDIA CORPORATION.  All rights reserved.
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

/**
 * Common definitions for MPL sensor devices.
 */

#ifndef ANDROID_MPL_SENSOR_DEFS_H
#define ANDROID_MPL_SENSOR_DEFS_H

enum {
    Gyro = 0,
    RawGyro,
    Accelerometer,
    MagneticField,
    RawMagneticField,
    Pressure,
    Orientation,
    RotationVector,
    GameRotationVector,
    LinearAccel,
    Gravity,
    SignificantMotion,
    StepDetector,
    StepCounter,
    GeomagneticRotationVector,
    MpuNumSensors
};

#define MPLGYRO_DEF {                                   \
    .name = "MPL Gyroscope",                            \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = Gyro,                                     \
    .type = SENSOR_TYPE_GYROSCOPE,                      \
    .maxRange = 2000.0f,                                \
    .resolution = 1.0f,                                 \
    .power = 0.5f,                                      \
    .minDelay = 10000,                                  \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

#define MPLRAWGYRO_DEF {                                \
    .name = "MPL Gyroscope",                            \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = RawGyro,                                  \
    .type = SENSOR_TYPE_GYROSCOPE_UNCALIBRATED,         \
    .maxRange = 2000.0f,                                \
    .resolution = 1.0f,                                 \
    .power = 0.5f,                                      \
    .minDelay = 10000,                                  \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

#define MPLACCELEROMETER_DEF {                          \
    .name = "MPL Accelerometer",                        \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = Accelerometer,                            \
    .type = SENSOR_TYPE_ACCELEROMETER,                  \
    .maxRange = 10240.0f,                               \
    .resolution = 1.0f,                                 \
    .power = 0.5f,                                      \
    .minDelay = 10000,                                  \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

#define MPLMAGNETICFIELD_DEF {                          \
    .name = "MPL Magnetic Field",                       \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = MagneticField,                            \
    .type = SENSOR_TYPE_MAGNETIC_FIELD,                 \
    .maxRange = 10240.0f,                               \
    .resolution = 1.0f,                                 \
    .power = 0.5f,                                      \
    .minDelay = 10000,                                  \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

#define MPLRAWMAGNETICFIELD_DEF {                       \
    .name = "MPL Raw Magnetic Field",                   \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = RawMagneticField,                         \
    .type = SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED,    \
    .maxRange = 10240.0f,                               \
    .resolution = 1.0f,                                 \
    .power = 0.5f,                                      \
    .minDelay = 10000,                                  \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

#define MPLPRESSURE_DEF {                               \
    .name = "MPL Pressure",                             \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = Pressure,                                 \
    .type = SENSOR_TYPE_PRESSURE,                       \
    .maxRange = 800.0f,                                 \
    .resolution = 0.01f,                                \
    .power = 0.5f,                                      \
    .minDelay = 10000,                                  \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

#define MPLORIENTATION_DEF {                            \
    .name = "MPL Orientation",                          \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = Orientation,                              \
    .type = SENSOR_TYPE_ORIENTATION,                    \
    .maxRange = 360.0f,                                 \
    .resolution = 0.00001f,                             \
    .power = 1.0f,                                      \
    .minDelay = 10000,                                  \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

#define MPLROTATIONVECTOR_DEF {                         \
    .name = "MPL Rotation Vector",                      \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = RotationVector,                           \
    .type = SENSOR_TYPE_ROTATION_VECTOR,                \
    .maxRange = 1.0f,                                   \
    .resolution = 0.00001f,                             \
    .power = 1.0f,                                      \
    .minDelay = 10000,                                  \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

#define MPLGAMEROTATIONVECTOR_DEF {                     \
    .name = "MPL Game Rotation Vector",                 \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = GameRotationVector,                       \
    .type = SENSOR_TYPE_GAME_ROTATION_VECTOR,           \
    .maxRange = 1.0f,                                   \
    .resolution = 0.00001f,                             \
    .power = 0.7f,                                      \
    .minDelay = 10000,                                  \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

#define MPLLINEARACCEL_DEF {                            \
    .name = "MPL Linear Acceleration",                  \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = LinearAccel,                              \
    .type = SENSOR_TYPE_LINEAR_ACCELERATION,            \
    .maxRange = 10240.0f,                               \
    .resolution = 1.0f,                                 \
    .power = 0.5f,                                      \
    .minDelay = 10000,                                  \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

#define MPLGRAVITY_DEF {                                \
    .name = "MPL Gravity",                              \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = Gravity,                                  \
    .type = SENSOR_TYPE_GRAVITY,                        \
    .maxRange = 9.81f,                                  \
    .resolution = 0.00001f,                             \
    .power = 0.5f,                                      \
    .minDelay = 10000,                                  \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

#define MPLSIGNIFICANTMOTION_DEF {                      \
    .name = "MPL Significant Motion",                   \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = SignificantMotion,                        \
    .type = SENSOR_TYPE_SIGNIFICANT_MOTION,             \
    .maxRange = 1.0f,                                   \
    .resolution = 1.0f,                                 \
    .power = 1.1f,                                      \
    .minDelay = -1,                                     \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

#define MPLSTEPDETECTOR_DEF {                           \
    .name = "MPL Step Detector",                        \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = StepDetector,                             \
    .type = SENSOR_TYPE_STEP_DETECTOR,                  \
    .maxRange = 100.0f,                                 \
    .resolution = 1.0f,                                 \
    .power = 1.1f,                                      \
    .minDelay = 0,                                      \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 64 }

#define MPLSTEPCOUNTER_DEF {                            \
    .name = "MPL Step Counter",                         \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = StepCounter,                              \
    .type = SENSOR_TYPE_STEP_COUNTER,                   \
    .maxRange = 100.0f,                                 \
    .resolution = 1.0f,                                 \
    .power = 1.1f,                                      \
    .minDelay = 0,                                      \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

#define MPLGEOMAGNETICROTATIONVECTOR_DEF {              \
    .name = "MPL Geomagnetic RV",                       \
    .vendor = "Invensense",                             \
    .version = 1,                                       \
    .handle = GeomagneticRotationVector,                \
    .type = SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR,    \
    .maxRange = 1.0f,                                   \
    .resolution = 0.00001f,                             \
    .power = 0.5f,                                      \
    .minDelay = 10000,                                  \
    .fifoReservedEventCount = 0,                        \
    .fifoMaxEventCount = 0 }

static struct sensor_t sMplSensorList[] =
{
    MPLGYRO_DEF,
    MPLRAWGYRO_DEF,
    MPLACCELEROMETER_DEF,
    MPLMAGNETICFIELD_DEF,
    MPLRAWMAGNETICFIELD_DEF,
    MPLPRESSURE_DEF,
    MPLORIENTATION_DEF,
    MPLROTATIONVECTOR_DEF,
    MPLGAMEROTATIONVECTOR_DEF,
    MPLLINEARACCEL_DEF,
    MPLGRAVITY_DEF,
    MPLSIGNIFICANTMOTION_DEF,
    MPLSTEPDETECTOR_DEF,
    MPLSTEPCOUNTER_DEF,
    MPLGEOMAGNETICROTATIONVECTOR_DEF,
};

#endif
