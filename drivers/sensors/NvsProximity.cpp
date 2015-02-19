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

/* NVS light/proximity drivers have two calibration mechanisms:
 * Method 1 (preferred):
 * This method uses interpolation and requires a low and high uncalibrated
 * value along with the corresponding low and high calibrated values.  The
 * uncalibrated values are what is read from the sensor in the steps below.
 * The corresponding calibrated values are what the correct value should be.
 * All values are programmed into the device tree settings.
 * 1. Read scale sysfs attribute.  This value will need to be written back.
 * 2. Disable device.
 * 3. Write 1 to the scale sysfs attribute.
 * 4. Enable device.
 * 5. The NVS HAL will announce in the log that calibration mode is enabled and
 *    display the data along with the scale and offset parameters applied.
 * 6. Write the scale value read in step 1 back to the scale sysfs attribute.
 * 7. Put the device into a state where the data read is a low value.
 * 8. Note the values displayed in the log.  Separately measure the actual
 *    value.  The value from the sensor will be the uncalibrated value and the
 *    separately measured value will be the calibrated value for the current
 *    state (low or high values).
 * 9. Put the device into a state where the data read is a high value.
 * 10. Repeat step 8.
 * 11. Enter the values in the device tree settings for the device.  Both
 *     calibrated and uncalibrated values will be the values before scale and
 *     offset are applied.
 *     For example, a proximity sensor has the following device tree
 *     parameters:
 *     proximity_uncalibrated_lo
 *     proximity_calibrated_lo
 *     proximity_uncalibrated_hi
 *     proximity_calibrated_hi
 *
 * Method 2:
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
/* NVS proximity drivers can be configured for binary output.  If the
 * proximity_binary_threshold setting in the device tree is set, the driver
 * will configure the rest of the settings so that a 1 is reported for
 * "far away" and 0 for "near".  The threshold is typically set for maximum
 * range allowing the minimal LED drive power to determine the actual range.
 * To disable binary output, set the proximity_binary_threshold to 0.  The
 * driver will then require the interpolation calibration for reporting actual
 * distances.
 */
/* Calibration for proximity_binary_threshold is done with the following steps:
 * 1. Disable proximity.
 * 2. Write 1 to the scale sysfs attribute.
 * 3. Enable device.
 * 4. The NVS HAL will announce in the log that calibration mode is enabled and
 *    display the raw HW proximity values instead of the 0 or 1.
 * 5. Move a surface within the range of the proximity where the output
 *    state should change.  There should be two places: the transition from
 *    1 to 0 and vice versa.  The middle of these two points will be the
 *    proximity_binary_threshold and the difference between each point and
 *    the proximity_binary_threshold will be the proximity_threshold_lo and
 *    proximity_threshold_hi settings in the device tree which will allow
 *    hysteresis around the threshold and can be skewed as such.
 * 6. Write 0 to the scale sysfs attribute.
 * 7. Disabling the device will turn off this calibration mode.
 * Note that proximity_threshold_lo and proximity_threshold_hi do not have to
 * be identical and can in fact result in being outside the HW range:
 * Example: HW values range from 0 to 255.
 *          proximity_binary_threshold is set to 1.
 *          proximity_threshold_lo is set to 10.
 *          proximity_threshold_hi is set to 20.
 *          In this configuration, the low state changes at 0 and the
 *          high at 20.
 * The driver will automatically handle out of bounds values.
 * Also, the proximity_thresh_falling_value and proximity_thresh_rising_value
 * sysfs attributes can change the proximity_threshold_lo/hi values at runtime
 * for further calibration flexibility.
 */


#include "NvsProximity.h"


NvsProximity::NvsProximity(int devNum, Nvs *link)
    : Nvs(devNum,
          "proximity",
          NULL,
          SENSOR_TYPE_PROXIMITY,
          link)
{
}

NvsProximity::~NvsProximity()
{
}

