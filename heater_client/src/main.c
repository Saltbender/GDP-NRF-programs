/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>

/* Included these for USB bla bla*/

#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/printk.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(coap_client_utils);

/*Coap client stuff*/
#include "coap_client.h                                                        "

#include <stdlib.h>
#include <time.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS  1350

/* The devicetree node identifier for the "led0" alias. */
#define LED1_NODE DT_ALIAS(led1)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED1_NODE, gpios);


// PWM function
void heater_setting(const struct device *dev, float duty, uint8_t channel){
	int ret;

	// Set freq to 1khz, 1 million nanoseconds period
	uint32_t period = 1000000;

	// Duty is set in %, so 100 means pulse width is full
	float duty_pct = duty/100.0;
	uint32_t pulse = duty_pct*period;

	pwm_set(dev, channel, period, pulse, PWM_POLARITY_INVERTED);

	// Turn LED on if heating is on
	if (!device_is_ready(led.port)) {
		return;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}
	if (duty > 0){
		ret = gpio_pin_set_dt(&led, 1);
	}
	else{
		ret = gpio_pin_set_dt(&led, 0);
	}
}


void main(void)
{	
	/*Init coap fuckery*/
	coap_client_init();

	/* CONFIGURE THE GPIO PINS FOR HEATER OUTPUT*/

	/* PINOUTS
	Same PWM controller used to control all heaters
	HEATER 1 = D13, P1.09
	HEATER 2 = D12, P0.08
	HEATER 3 = D11, P0.06

	THERMOCOUPLE 1 = D10, P0.27
	THERMOCOUPLE 2 = D9, P0.26
	THERMOCOUPLE 3 = D6, P0.07
	*/

	const struct device *heater1 = DEVICE_DT_GET(DT_NODELABEL(pwm0));

	/* CONFIGURE SPI FOR THERMOCOUPLES */
	const struct device *thermocouple_1 = DEVICE_DT_GET(DT_NODELABEL(a));
	struct sensor_value val1;
	const struct device *dev2 = DEVICE_DT_GET(DT_NODELABEL(b));
	struct sensor_value val2;
	const struct device *dev3 = DEVICE_DT_GET(DT_NODELABEL(c));
	struct sensor_value val3;

	/*USB fuckery starts here*/
	const struct device *usb_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	uint32_t dtr = 0;

	if (usb_enable(NULL)) {
		return;
	}

	/* Poll if the DTR flag was set */
	// while (!dtr) {
	// 	uart_line_ctrl_get(usb_dev, UART_LINE_CTRL_DTR, &dtr);
	// 	/* Give CPU resources to low priority threads. */
	// 	k_sleep(K_MSEC(100));
	// }

	/*USB fuckery ends here*/

	if (!device_is_ready(thermocouple_1)) {
		printk("sensor: device not ready.\n");
		return;
	}

	int ret;

	if (!device_is_ready(led.port)) {
		return;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}

	float target;
	float error, previous_error;
	float p, i, d;
	i = 0; // set integral to 0 a t the beginning
	// Ku = 32.0
	float k_p = 19.2;
	float k_i = 0;
	float k_d = 6;
	float duty;

	float elapsed_time = 1.35+0.02+0.02+0.02;

	while (1) {
		/*Target temp retrieval*/
		target = retrieve_stored_target_temp();

		/* Getting the temperature */
		int ret1;

		ret1 = sensor_sample_fetch_chan(thermocouple_1, SENSOR_CHAN_AMBIENT_TEMP);
		if (ret1 < 0) {
			printk("Could not fetch temperature (%d)\n", ret);
			return;
		}

		ret1 = sensor_channel_get(thermocouple_1, SENSOR_CHAN_AMBIENT_TEMP, &val1);
		if (ret1 < 0) {
			printk("Could not get temperature (%d)\n", ret);
			return;
		}

		k_msleep(20); // Wait for 10ms for getting values

		double one = sensor_value_to_double(&val1);
		k_msleep(20); // Wait for 10ms for calculations

		// PID stuff
		error = target-one;
		p = error;
		i = i + error*elapsed_time; //dt = 20ms + 20ms + 20ms + 1350ms
		if (abs(i) > 30){
			if(i > 0)
				i = 30;
			else
				i = -30;
		}

		if(one > 50)
			i = 0; // reset i if comms error

		d = (error-previous_error)/elapsed_time;

		duty = k_p*p + k_i*i + k_d*d;
		if(duty >= 100)
			duty = 100;
		else if (duty < 0)
			duty = 0;

		previous_error = error;
		k_msleep(20); // Wait for 10ms for getting values

		heater_setting(heater1, duty, 0);
		heater_setting(heater1, duty, 1);
		heater_setting(heater1, duty, 2);

		LOG_INF("$%f %f %f;", one, target, duty);

		k_msleep(SLEEP_TIME_MS);
	}
}
