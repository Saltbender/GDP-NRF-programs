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

#include "coap_client_utils.h"

LOG_MODULE_DECLARE(coap_client_utils, CONFIG_COAP_CLIENT_UTILS_LOG_LEVEL);

#define OT_CONNECTION_LED DK_LED1
#define COUNTER_LED DK_LED2
#define MTD_SED_LED DK_LED3

#define ALARM_DELAY 1000000 /*1 second, units for function is usec*/
#define ALARM_CHANNEL 0

/*Variables for storing sensor data*/
static uint16_t temperature;
static uint16_t humidity;

static bool isLedOn = 0;

static void on_ot_connect(struct k_work *item)
{
	ARG_UNUSED(item);

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

static void on_button_changed(uint32_t button_state, uint32_t has_changed)
{
	uint32_t buttons = button_state & has_changed;

	if (buttons & DK_BTN1_MSK) {
		coap_client_send_sensor_data(temperature, humidity);
	}

	if (buttons & DK_BTN2_MSK) {
		printk("Nothing going on here");
	}

	if (buttons & DK_BTN3_MSK) {
		coap_client_toggle_minimal_sleepy_end_device();
	}

	if (buttons & DK_BTN4_MSK) {
		coap_client_send_provisioning_request();
	}
}

static counter_top_callback_t submit_read_sensor_work (const struct device *dev, void *user_data){
	ARG_UNUSED(user_data);
	//printk("countering every second!\n");
	dk_set_led(COUNTER_LED, !isLedOn); 
	isLedOn = !isLedOn;

	if (isProvisioned()){
		coap_client_send_sensor_data(temperature, humidity);
	}
}

void main(void)
{
	int ret;

	LOG_INF("Start CoAP-client sample");
	printk("Start CoAP-client sample");

	if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY)) {
		power_down_unused_ram();
	}

	ret = dk_buttons_init(on_button_changed);
	if (ret) {
		LOG_ERR("Cannot init buttons (error: %d)", ret);
		return;
	}

	ret = dk_leds_init();
	if (ret) {
		LOG_ERR("Cannot init leds, (error: %d)", ret);
		return;
	}

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
}
