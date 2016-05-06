/** @file
 *  @brief BAS Service sample
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <misc/printk.h>
#include <misc/byteorder.h>
#include <zephyr.h>
#include <device.h>
#include <pwm.h>
#include <adc.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#define BT_UUID_WEBBT BT_UUID_DECLARE_16(0xfc00)

#define BT_UUID_TEMP BT_UUID_DECLARE_16(0xfc0a)
#define BT_UUID_RGB BT_UUID_DECLARE_16(0xfc0b)

#define SENSOR_1_NAME "Temperature"
#define SENSOR_2_NAME "Led"

static struct bt_gatt_ccc_cfg  blvl_ccc_cfg[CONFIG_BLUETOOTH_MAX_PAIRED] = {};
static uint8_t simulate_blvl;
static uint8_t temperature = 20;
static uint8_t rgb[3] = {0xff, 0x00, 0x00}; // red

struct device *pwm, *tmp36;
#define IO3_RED   0
#define IO5_GREEN 1
#define IO6_BLUE  2

#define ADC_DEVICE_NAME "ADC_0"

#define SLEEPTICKS      SECONDS(1)
/* about 2 ms, thus 500Hz */
#define PERIOD 64000
#define MAX(a,b) (((a)>(b))?(a):(b))

void pwm_write(void)
{
    uint32_t r_on, g_on, b_on;
    uint32_t r_off, g_off, b_off;

    r_on = (rgb[0] / 255.0) * PERIOD;
    g_on = (rgb[1] / 255.0) * PERIOD;
    b_on = (rgb[2] / 255.0) * PERIOD;

    r_on = MAX(1, r_on);
    g_on = MAX(1, g_on);
    b_on = MAX(1, b_on);

    r_off = PERIOD - r_on;
    g_off = PERIOD - g_on;
    b_off = PERIOD - b_on;

    r_off = MAX(1, r_off);
    g_off = MAX(1, g_off);
    b_off = MAX(1, b_off);

    printk("rgb: %x %x %x\n", rgb[0], rgb[1], rgb[2]);
    printk("on : %x %x %x\n", r_on, g_on, b_on);
    printk("off: %x %x %x\n", r_off, g_off, b_off);

    pwm_pin_set_values(pwm, IO3_RED, r_on, r_off);
    pwm_pin_set_values(pwm, IO5_GREEN, g_on, g_off);
    pwm_pin_set_values(pwm, IO6_BLUE, b_on, b_off);
}

static void blvl_ccc_cfg_changed(uint16_t value)
{
    printk("blvl_ccc_cfg_changed: %d\n", value);
    if ((value == BT_GATT_CCC_NOTIFY) || (value == BT_GATT_CCC_INDICATE))
        simulate_blvl = 1;
    else
        simulate_blvl = 0;
}

static ssize_t read_temperature(struct bt_conn *conn, const struct bt_gatt_attr *attr,
    void *buf, uint16_t len, uint16_t offset)
{
    const char *value = attr->user_data;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
        sizeof(*value));
}

static ssize_t read_rgb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
    void *buf, uint16_t len, uint16_t offset)
{
    memcpy(rgb, attr->user_data, sizeof(rgb));

    printk("read_rgb: %x %x %x\n", rgb[0], rgb[1], rgb[2]);

    return bt_gatt_attr_read(conn, attr, buf, len, offset, rgb,
                 sizeof(rgb));
}

static ssize_t write_rgb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
    const void *buf, uint16_t len, uint16_t offset)
{
    memcpy(rgb, buf, sizeof(rgb));

    printk("write_rgb: %x %x %x\n", rgb[0], rgb[1], rgb[2]);

    pwm_write();

    return sizeof(rgb);
}

/* WebBT Service Declaration */
static struct bt_gatt_attr attrs[] = {
    BT_GATT_PRIMARY_SERVICE(BT_UUID_WEBBT),

    /* Temperature */
    BT_GATT_CHARACTERISTIC(BT_UUID_TEMP,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY),
    BT_GATT_DESCRIPTOR(BT_UUID_TEMP, BT_GATT_PERM_READ,
        read_temperature, NULL, &temperature),
    BT_GATT_CCC(blvl_ccc_cfg, blvl_ccc_cfg_changed),
    BT_GATT_CUD(SENSOR_1_NAME, BT_GATT_PERM_READ),

    /* RGB Led */
    BT_GATT_CHARACTERISTIC(BT_UUID_RGB,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE),
    BT_GATT_DESCRIPTOR(BT_UUID_RGB, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        read_rgb, write_rgb, rgb),
    BT_GATT_CUD(SENSOR_2_NAME, BT_GATT_PERM_READ),
};

void service_init(void)
{
    if ((pwm = device_get_binding("PWM")) == NULL)
        printk("device_get_binding: failed for PWM\n");
    else {
        pwm_write();
        printk("PWM init OK\n");
    }

    bt_gatt_register(attrs, ARRAY_SIZE(attrs));
}

void service_notify(struct bt_conn *conn)
{
    if (!simulate_blvl) {
        return;
    }

    temperature--;
    if (!temperature) {
        /* Software eco temperature charger */
        temperature = 20;
    }
    printk("service_notify: temperature is %d\n", temperature);

    bt_gatt_notify(conn, &attrs[2], &temperature, sizeof(temperature));
}
