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

LOG_MODULE_DECLARE(coap_client_utils);

#define OT_CONNECTION_LED DK_LED1
#define COUNTER_LED DK_LED2
#define MTD_SED_LED DK_LED3

#define ALARM_DELAY 1000000 /*1 second, units for function is usec*/
#define ALARM_CHANNEL 0

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

static counter_top_callback_t submit_new_temperature_work (const struct device *dev, void *user_data){
	ARG_UNUSED(user_data);
	//printk("countering every second!\n");
	dk_set_led(COUNTER_LED, !isLedOn); 
	isLedOn = !isLedOn;

	if (isProvisioned()){
		coap_client_get_new_target_temp();
	}
}

float retrieve_stored_target_temp(void){
	return coap_utils_retrieve_stored_target_temp();
}

void coap_client_init(void)
{
	int ret;

	LOG_INF("Start CoAP-client sample");
	printk("Start CoAP-client sample");

	if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY)) {
		power_down_unused_ram();
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
	counter_cfg.callback = submit_new_temperature_work;
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
