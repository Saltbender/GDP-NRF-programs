/*
 * Copyright (c) 2019 Infineon Technologies AG
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdlib.h>

#include <zephyr/drivers/i2c.h>

/* Included these for USB bla bla*/

#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/printk.h>

#define SENSOR_ADDRESS 0x5C

/* Series of commands that need to be provided to the sensor
* Meaning of values:
	0x03: function code for sensor
	0x00: starting address of register to be read from
	0x04: read 4 registers from the sensor
*/
static uint8_t humidity_temp_send_buffer[3] = {0x03, 0x00, 0x04};

//Receive buffer needs to be length 7
int getSensorValues(const struct device* dev, uint16_t* humidity, uint16_t* temperature ){
	uint8_t empty_buffer = 0;
	uint8_t wake_byte_count = 0;
	uint8_t error;

	uint8_t receive_buffer[8];

	//Wake up the device
	error = i2c_write(dev, &empty_buffer, wake_byte_count, SENSOR_ADDRESS);

	k_sleep(K_MSEC(1));
	error = i2c_write(dev, humidity_temp_send_buffer, sizeof(humidity_temp_send_buffer)/sizeof(uint8_t), SENSOR_ADDRESS);

	k_sleep(K_MSEC(1));
	error = i2c_read(dev, receive_buffer, sizeof(receive_buffer)/sizeof(uint8_t), SENSOR_ADDRESS);

	/*Humidity and temperature is receive as 2 bytes, first one for higher byte and second one for lower byte
	Humidity is received first, then temperature
	Full list of bytes received:
	0: Function code
	1: Number of bytes to read
	2,3: Humidity
	4,5: Temperature
	6,7: CRC checksum
	*/
	*humidity = receive_buffer[2] << 8 | receive_buffer[3];
	*temperature = receive_buffer[4] << 8 | receive_buffer[5];

	return 0;
}

int initI2C (struct device* dev){
	dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	if (!i2c_configure(dev, I2C_SPEED_STANDARD)){
		printk("Error configuring the I2C device");
		return -1;
	}

	if (!device_is_ready(dev)) {
		printk("Device %s is not ready\n", dev->name);
		return -1;
	}

	return 0;
}

// void main(void)
// {
// 	/*Initialise USB for printing*/
// 	/*USB fuckery starts here*/
// 	const struct device *usb_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));

// 	if (usb_enable(NULL)) {
// 		return;
// 	}
	
// 	/*USB fuckery complete
// 	Now entering main application*/

// 	printk("Hello Am2320\n");
// 	const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
// 	if (!i2c_configure(i2c_dev, I2C_SPEED_STANDARD)){
// 		printk("Error configuring the I2C device");
// 	}

// 	if (!device_is_ready(i2c_dev)) {
// 		printk("Device %s is not ready\n", i2c_dev->name);
// 		return;
// 	}

// 	printk("dev %p name %s\n", i2c_dev, i2c_dev->name);

// 	uint16_t humidity, temperature;

// 	while (1) {
// 		getSensorValues(i2c_dev, &humidity, &temperature);
// 		//printk("temp: %d.%06d; humidity: %d.%06d\n",
// 		     // temp.val1, abs(temp.val2), press.val1, press.val2);
// 		printk("temp: %d; humidity: %d \%\n",
// 		      temperature, humidity);
// 		k_sleep(K_MSEC(1000));
// 	}
// }

