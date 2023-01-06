/**
 * @file
 * @defgroup coap_client_utils API for coap_client_* samples
 * @{
 */

/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef __COAP_CLIENT_UTILS_H__
#define __COAP_CLIENT_UTILS_H__

/** @brief Struct for implementing k_work object as a member of a parent struct 
 * to enable passing of data to the function
 *
 *  
 */

struct work_var_container{
	struct k_work 	work_obj; 		/*Store the work object here*/
	char* 		testing_data;	/*Store the testing data here*/
};

struct work_sensor_container{
	struct k_work 	work_obj; 		/*Store the work object here*/
	float 			temperature;	/*Store the 'float' temperature data here*/
	float			humidity;		/*Store the 'float' humidity data here*/
};

/** @brief Type indicates function called when OpenThread connection
 *         is established.
 *
 * @param[in] item pointer to work item.
 */
typedef void (*ot_connection_cb_t)(struct k_work *item);

/** @brief Type indicates function called when OpenThread connection is ended.
 *
 * @param[in] item pointer to work item.
 */
typedef void (*ot_disconnection_cb_t)(struct k_work *item);

/** @brief Type indicates function called when the MTD modes are toggled.
 *
 * @param[in] val 1 if the MTD is in MED mode
 *                0 if the MTD is in SED mode
 */
typedef void (*mtd_mode_toggle_cb_t)(uint32_t val);

/** @brief Initialize CoAP client utilities.
 */
void coap_client_utils_init(ot_connection_cb_t on_connect,
			    ot_disconnection_cb_t on_disconnect,
			    mtd_mode_toggle_cb_t on_toggle);

/** @brief Toggle light on the CoAP server node.
 *
 * @note The CoAP server should be paired before to have an effect.
 */
void coap_client_send_sensor_data (uint16_t temperature_data, uint16_t humidity_data);


/** @brief Request for the CoAP server address to pair.
 *
 * @note Enable paring on the CoAP server to get the address.
 */
void coap_client_send_provisioning_request(void);

/** @brief Function to determine if successful provisioning was performed
 *  
 * @note Determined by checking byte-0 of IP address string, which is initialized to 0
 * After provisioning, the first byte is always set.
*/

bool isProvisioned(void);

/** @brief Toggle SED to MED and MED to SED modes.
 *
 * @note Active when the device is working as Minimal Thread Device.
 */
void coap_client_toggle_minimal_sleepy_end_device(void);

#endif

/**
 * @}
 */
