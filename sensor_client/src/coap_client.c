/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/logging/log.h>
#include <ram_pwrdn.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>



#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/i2c.h>

#include <zephyr/usb/usb_device.h>

#include "coap_client_utils.h"
#include "am2320.h"

LOG_MODULE_DECLARE(coap_client_utils, CONFIG_COAP_CLIENT_UTILS_LOG_LEVEL);

#define OT_CONNECTION_LED DK_LED1
#define COUNTER_LED DK_LED2
#define MTD_SED_LED DK_LED3

#define ALARM_DELAY 1000000 /*1 second, units for function is usec*/
#define ALARM_CHANNEL 0

/*Variables for storing sensor data*/
static const struct device* i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
static uint16_t temperature = 100;
static uint16_t humidity = 100;

/*Variable for work struct*/
struct k_work fetch_sensor_work;

static bool isLedOn = 0;

static void on_ot_connect(struct k_work *item)
{
	ARG_UNUSED(item);
	coap_client_send_provisioning_request();
	dk_set_led_on(OT_CONNECTION_LED);
}

static void on_ot_disconnect(struct k_work *item)
{
	ARG_UNUSED(item);

	dk_set_led_off(OT_CONNECTION_LED);
}

static void on_mtd_mode_toggle(uint32_t med)
{
#if IS_ENABLED(CONFIG_PM_DEVICE)
	const struct device *cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

	if (!device_is_ready(cons)) {
		return;
	}

	if (med) {
		pm_device_action_run(cons, PM_DEVICE_ACTION_RESUME);
	} else {
		pm_device_action_run(cons, PM_DEVICE_ACTION_SUSPEND);
	}
#endif
	dk_set_led(MTD_SED_LED, med);
}

/*Function provided to k_work object, retrieves sensors value*/
static void fetch_sensor_data(struct k_work *item){
	ARG_UNUSED(item);
	getSensorValues(i2c_dev, &humidity, &temperature);
}


/*Callback provided to counter top config*/
static void submit_read_sensor_work (const struct device *dev, void *user_data){
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);
	LOG_INF("countering every second!\n");
	dk_set_led(COUNTER_LED, !isLedOn); 
	isLedOn = !isLedOn;

	k_work_submit(&fetch_sensor_work);

	if (isProvisioned()){
		coap_client_send_sensor_data(temperature, humidity);
	}
}

void main(void)
{
	int ret;
	// uint16_t humidity = 100;
	// uint16_t temperature = 100;

	LOG_INF("Start CoAP-client sample");
	printk("Start CoAP-client sample");

	if (usb_enable(NULL)) {
		return;
	}

	if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY)) {
		power_down_unused_ram();
	}

	ret = dk_leds_init();
	if (ret) {
		LOG_ERR("Cannot init leds, (error: %d)", ret);
		return;
	}

	/*Initialise k_work related stuff*/
	k_work_init(&fetch_sensor_work, fetch_sensor_data);

	/*Initialise I2C peripheral for communicating with the sensor*/
	ret = initI2C(i2c_dev);
	if (ret){
		LOG_ERR("I2C initialisation error code: %d", ret);
	}
	LOG_INF("dev %p name %s\n", i2c_dev, i2c_dev->name);
	/*Initialise RTC counter device, provide top value and callback function*/

	const struct device* rtc_dev = DEVICE_DT_GET(DT_NODELABEL(rtc2));
	counter_stop(rtc_dev);
	struct counter_top_cfg counter_cfg;
	counter_cfg.ticks = counter_us_to_ticks(rtc_dev, 5*ALARM_DELAY);
	counter_cfg.callback = submit_read_sensor_work;
	counter_cfg.user_data = &counter_cfg;
	ret = counter_set_top_value(rtc_dev, &counter_cfg);


	if (-EINVAL == ret) {
		printk("Alarm settings invalid\n");
	} else if (-ENOTSUP == ret) {
		printk("Alarm setting request not supported\n");
	} else if (ret != 0) {
		printk("Error\n");
	}


	coap_client_utils_init(on_ot_connect, on_ot_disconnect,
			       on_mtd_mode_toggle);

	while (!isProvisioned()){
		coap_client_send_provisioning_request();
		printk("waiting for provisioning");
		k_sleep(K_SECONDS(10));
	}
	counter_start(rtc_dev);


	// while (1){
	// 	LOG_INF("countering every second!\n");

	// 	dk_set_led(COUNTER_LED, !isLedOn); 
	// 	isLedOn = !isLedOn;

	// 	getSensorValues(i2c_dev, &humidity, &temperature);

	// 	if (isProvisioned()){
	// 		coap_client_send_sensor_data(temperature, humidity);
	// 	}

	// 	k_msleep(5000);
	// }
}
