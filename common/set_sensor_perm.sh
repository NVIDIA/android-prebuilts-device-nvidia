#!/system/bin/sh
#
# Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
#
chown system:system /dev/iio:device*
chown system:system /sys/bus/iio/devices/iio:device*/enable
chown system:system /sys/bus/iio/devices/iio:device*/buffer/enable
chown system:system /sys/bus/iio/devices/iio:device*/buffer/length
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_timestamp_en


chown system:system /sys/bus/iio/devices/iio:device*/dmp_firmware

chown system:system /sys/bus/iio/devices/iio:device*/self_test
chown system:system /sys/bus/iio/devices/iio:device*/accel_batch_flags
chown system:system /sys/bus/iio/devices/iio:device*/accel_batch_period
chown system:system /sys/bus/iio/devices/iio:device*/accel_batch_timeout
chown system:system /sys/bus/iio/devices/iio:device*/accel_flush
chown system:system /sys/bus/iio/devices/iio:device*/in_accel_y_offset
chown system:system /sys/bus/iio/devices/iio:device*/in_accel_z_offset
chown system:system /sys/bus/iio/devices/iio:device*/in_accel_sampling_frequency
chown system:system /sys/bus/iio/devices/iio:device*/in_accel_x_offset
chown system:system /sys/bus/iio/devices/iio:device*/in_accel_y_offset
chown system:system /sys/bus/iio/devices/iio:device*/in_accel_z_offset
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_accel_x_en
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_accel_y_en
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_accel_z_en

chown system:system /sys/bus/iio/devices/iio:device*/anglvel_batch_flags
chown system:system /sys/bus/iio/devices/iio:device*/anglvel_batch_period
chown system:system /sys/bus/iio/devices/iio:device*/anglvel_batch_timeout
chown system:system /sys/bus/iio/devices/iio:device*/anglvel_flush
chown system:system /sys/bus/iio/devices/iio:device*/in_anglvel_sampling_frequency
chown system:system /sys/bus/iio/devices/iio:device*/in_anglvel_x_offset
chown system:system /sys/bus/iio/devices/iio:device*/in_anglvel_y_offset
chown system:system /sys/bus/iio/devices/iio:device*/in_anglvel_z_offset
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_anglvel_x_en
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_anglvel_y_en
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_anglvel_z_en

chown system:system /sys/bus/iio/devices/iio:device*/magn_batch_flags
chown system:system /sys/bus/iio/devices/iio:device*/magn_batch_period
chown system:system /sys/bus/iio/devices/iio:device*/magn_batch_timeout
chown system:system /sys/bus/iio/devices/iio:device*/magn_flush
chown system:system /sys/bus/iio/devices/iio:device*/in_magn_sampling_frequency
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_magn_x_en
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_magn_y_en
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_magn_z_en

chown system:system /sys/bus/iio/devices/iio:device*/pressure_batch_flags
chown system:system /sys/bus/iio/devices/iio:device*/pressure_batch_period
chown system:system /sys/bus/iio/devices/iio:device*/pressure_batch_timeout
chown system:system /sys/bus/iio/devices/iio:device*/pressure_flush
chown system:system /sys/bus/iio/devices/iio:device*/in_pressure_sampling_frequency
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_pressure_en

chown system:system /sys/bus/iio/devices/iio:device*/temp_batch_flags
chown system:system /sys/bus/iio/devices/iio:device*/temp_batch_period
chown system:system /sys/bus/iio/devices/iio:device*/temp_batch_timeout
chown system:system /sys/bus/iio/devices/iio:device*/temp_flush
chown system:system /sys/bus/iio/devices/iio:device*/in_temp_sampling_frequency
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_temp_en

chown system:system /sys/bus/iio/devices/iio:device*/in_illuminance_sampling_frequency
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_illuminance_en

chown system:system /sys/bus/iio/devices/iio:device*/in_proximity_sampling_frequency
chown system:system /sys/bus/iio/devices/iio:device*/scan_elements/in_proximity_en

# For HAL MA514
# For LTR558
chown system:system /sys/bus/iio/devices/iio:device*/proximity_enable

# Sensor - For InvenSense Input driver
chown system:system /sys/class/invensense/mpu/accl_enable
chown system:system /sys/class/invensense/mpu/accl_fifo_enable
chown system:system /sys/class/invensense/mpu/accl_delay
chown system:system /sys/class/invensense/mpu/accl_max_range
chown system:system /sys/class/invensense/mpu/enable
chown system:system /sys/class/invensense/mpu/gyro_enable
chown system:system /sys/class/invensense/mpu/gyro_fifo_enable
chown system:system /sys/class/invensense/mpu/gyro_delay
chown system:system /sys/class/invensense/mpu/gyro_max_range
chown system:system /sys/class/invensense/mpu/lpa_delay
chown system:system /sys/class/invensense/mpu/motion_enable
chown system:system /sys/class/invensense/mpu/motion_threshold
chown system:system /sys/class/invensense/mpu/power_state
chown system:system /sys/class/invensense/mpu/key

chown system:system /sys/class/invensense/mpu/akm89xx/akm89xx/enable
chown system:system /sys/class/invensense/mpu/akm89xx/akm89xx/delay
chown system:system /sys/class/invensense/mpu/akm89xx/akm89xx/max_range
chown system:system /sys/class/invensense/mpu/akm89xx/akm89xx/resolution

chown system:system /sys/class/invensense/mpu/bmpX80/bmpX80/enable
chown system:system /sys/class/invensense/mpu/bmpX80/bmpX80/delay
chown system:system /sys/class/invensense/mpu/bmpX80/bmpX80/max_range
chown system:system /sys/class/invensense/mpu/bmpX80/bmpX80/resolution
