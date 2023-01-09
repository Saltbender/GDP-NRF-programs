/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>

#include <net/coap_utils.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/openthread.h>
#include <zephyr/net/socket.h>
#include <openthread/thread.h>

#include <stdlib.h>

#include <zephyr/debug/thread_analyzer.h>

#include "coap_server_client_interface.h"
#include "coap_client_utils.h"

LOG_MODULE_REGISTER(coap_client_utils, CONFIG_COAP_CLIENT_UTILS_LOG_LEVEL);

#define RESPONSE_POLL_PERIOD 100

static uint32_t poll_period;

static bool is_connected;

// static struct k_work send_sensor_data_work;
static struct k_work toggle_MTD_SED_work;
static struct k_work on_connect_work;
static struct k_work on_disconnect_work;

//Testing for implementing k_work object as a member of a parent struct
//Ease passing of data to the function
//Contains k_work object inside as well

static struct work_var_container provisioning_container;
static struct work_heater_container target_temp_data_container = {
	.target_temperature = 40.0
};

mtd_mode_toggle_cb_t on_mtd_mode_toggle;

/* Options supported by the server */
static const char *const node_option[] = { NODE1_URI_PATH, NULL };
static const char *const provisioning_option[] = { PROVISIONING_URI_PATH, NULL };
static const char *const heater_option[] = { HEATER_URI_PATH, NULL };

/* Thread multicast mesh local address */
static struct sockaddr_in6 multicast_local_addr = {
	.sin6_family = AF_INET6,
	.sin6_port = htons(COAP_PORT),
	.sin6_addr.s6_addr = { 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 },
	.sin6_scope_id = 0U
};

/* Variable for storing server address acquiring in provisioning handshake */
static char unique_local_addr_str[INET6_ADDRSTRLEN] = {0};
static struct sockaddr_in6 unique_local_addr = {
	.sin6_family = AF_INET6,
	.sin6_port = htons(COAP_PORT),
	.sin6_addr.s6_addr = { 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 },
	.sin6_scope_id = 0U
};

static bool is_mtd_in_med_mode(otInstance *instance)
{
	return otThreadGetLinkMode(instance).mRxOnWhenIdle;
}

static void poll_period_response_set(void)
{
	otError error;

	otInstance *instance = openthread_get_default_instance();

	if (is_mtd_in_med_mode(instance)) {
		return;
	}

	if (!poll_period) {
		poll_period = otLinkGetPollPeriod(instance);

		error = otLinkSetPollPeriod(instance, RESPONSE_POLL_PERIOD);
		__ASSERT(error == OT_ERROR_NONE, "Failed to set pool period");

		LOG_INF("Poll Period: %dms set", RESPONSE_POLL_PERIOD);
	}
}

static void poll_period_restore(void)
{
	otError error;
	otInstance *instance = openthread_get_default_instance();

	if (is_mtd_in_med_mode(instance)) {
		return;
	}

	if (poll_period) {
		error = otLinkSetPollPeriod(instance, poll_period);
		__ASSERT_NO_MSG(error == OT_ERROR_NONE);

		LOG_INF("Poll Period: %dms restored", poll_period);
		poll_period = 0;
	}
}

static int on_provisioning_reply(const struct coap_packet *response,
				 struct coap_reply *reply,
				 const struct sockaddr *from)
{
	int ret = 0;
	const uint8_t *payload;
	uint16_t payload_size = 0u;
	
	//Testing for printing out IPaddr from OT
	// otInstance *instance = openthread_get_default_instance();
	// void * ipaddr_ot = otThreadGetMeshLocalEid(instance);
	// int ipaddr_size = sizeof(otIp6Address);
	// LOG_HEXDUMP_INF(ipaddr_ot, ipaddr_size, "IP address");

	ARG_UNUSED(reply);
	ARG_UNUSED(from);

	payload = coap_packet_get_payload(response, &payload_size);

	if (payload == NULL) {
		LOG_ERR("No data received");
		ret = -EINVAL;
		goto exit;
	}
	
	// LOG_INF("Payload received: %s", payload);
	// LOG_INF("Payload size: %d", payload_size);

	//Convert the payload from ASCII encoded characters into literal integers
	//Process is performed in 2 byte char fragments turning into 1 byte uint8:

	// if (payload_size != 32) {
	// 	LOG_ERR("Questionable data format received");
	// 	ret = -EINVAL;
	// 	goto exit;
	// }

	// uint8_t ipaddr_int [16];
	// for(int i = 0; i < (payload_size/2); i++){
	// 	char* unused_ptr;
	// 	char payload_fragment [2];

	// 	memcpy(&payload_fragment, payload+(2*i), 2);
	// 	ipaddr_int[i] = strtol(payload_fragment, &unused_ptr, 16);
	// }

	// LOG_HEXDUMP_INF(ipaddr_int, 16, "Fully converted IP fragment");
	//LOG_INF("%s",payload);
	if (!inet_pton(AF_INET6, payload, &unique_local_addr.sin6_addr)){
		LOG_ERR("Received data is not IPv6 address: %d", errno);
		ret = -errno;
		goto exit;
	}
	memcpy(unique_local_addr_str, payload, payload_size);

	LOG_INF("Received peer address: %s", unique_local_addr_str);

exit:
	if (IS_ENABLED(CONFIG_OPENTHREAD_MTD_SED)) {
		poll_period_restore();
	}

	return ret;
}

static void send_provisioning_request(struct k_work *item)
{
	if (IS_ENABLED(CONFIG_OPENTHREAD_MTD_SED)) {
		/* decrease the polling period for higher responsiveness */
		poll_period_response_set();
	}

	LOG_INF("Send 'provisioning' request");
	coap_send_request(COAP_METHOD_GET,
			  (const struct sockaddr *)&multicast_local_addr,
			  provisioning_option, NULL, 0u, on_provisioning_reply);
}

static int on_get_new_target_reply(const struct coap_packet *response,
				 struct coap_reply *reply,
				 const struct sockaddr *from){
	int ret = 0;
	const uint8_t *payload;
	char new_target_string[8];
	uint16_t payload_size = 0u;
	
	ARG_UNUSED(reply);
	ARG_UNUSED(from);

	payload = coap_packet_get_payload(response, &payload_size);

	if (payload == NULL) {
		LOG_ERR("No data received");
		ret = -EINVAL;
		goto exit;
	}
	memcpy(new_target_string, payload, payload_size);
	target_temp_data_container.target_temperature = strtof(new_target_string,NULL);
	LOG_INF("retrieved string: %s, new target: %f", new_target_string, target_temp_data_container.target_temperature);
	exit:
	return ret;
}

static void send_new_target_request(struct k_work *item)
{
	if (unique_local_addr.sin6_addr.s6_addr16[0] == 0) {
		LOG_WRN("Peer address not set. Activate 'provisioning' option "
			"on the server side");
		return;
	}

	LOG_INF("Send 'new target temp' request to: %s", unique_local_addr_str);
	//thread_analyzer_print();
	coap_send_request(COAP_METHOD_GET,
			  (const struct sockaddr *)&unique_local_addr,
			  heater_option, NULL, 0u, on_get_new_target_reply);
}

static void toggle_minimal_sleepy_end_device(struct k_work *item)
{
	otError error;
	otLinkModeConfig mode;
	struct openthread_context *context = openthread_get_default_context();

	__ASSERT_NO_MSG(context != NULL);

	openthread_api_mutex_lock(context);
	mode = otThreadGetLinkMode(context->instance);
	mode.mRxOnWhenIdle = !mode.mRxOnWhenIdle;
	error = otThreadSetLinkMode(context->instance, mode);
	openthread_api_mutex_unlock(context);

	if (error != OT_ERROR_NONE) {
		LOG_ERR("Failed to set MLE link mode configuration");
	} else {
		on_mtd_mode_toggle(mode.mRxOnWhenIdle);
	}
}

static void update_device_state(void)
{
	struct otInstance *instance = openthread_get_default_instance();
	otLinkModeConfig mode = otThreadGetLinkMode(instance);
	on_mtd_mode_toggle(mode.mRxOnWhenIdle);
}

static void on_thread_state_changed(uint32_t flags, void *context)
{
	struct openthread_context *ot_context = context;

	if (flags & OT_CHANGED_THREAD_ROLE) {
		switch (otThreadGetDeviceRole(ot_context->instance)) {
		case OT_DEVICE_ROLE_CHILD:
		case OT_DEVICE_ROLE_ROUTER:
		case OT_DEVICE_ROLE_LEADER:
			k_work_submit(&on_connect_work);
			is_connected = true;
			break;

		case OT_DEVICE_ROLE_DISABLED:
		case OT_DEVICE_ROLE_DETACHED:
		default:
			k_work_submit(&on_disconnect_work);
			is_connected = false;
			break;
		}
	}
}

static void submit_work_if_connected(struct k_work *work)
{
	if (is_connected) {
		k_work_submit(work);
	} else {
		LOG_INF("Connection is broken");
	}
}

bool isProvisioned(){
	return unique_local_addr_str[0];
}

float coap_utils_retrieve_stored_target_temp(void){
	return target_temp_data_container.target_temperature;
}

void coap_client_utils_init(ot_connection_cb_t on_connect,
			    ot_disconnection_cb_t on_disconnect,
			    mtd_mode_toggle_cb_t on_toggle)
{
	on_mtd_mode_toggle = on_toggle;

	coap_init(AF_INET6, NULL);

	k_work_init(&on_connect_work, on_connect);
	k_work_init(&on_disconnect_work, on_disconnect);
	k_work_init(&target_temp_data_container.work_obj, send_new_target_request);
	k_work_init(&provisioning_container.work_obj, send_provisioning_request);

	openthread_set_state_changed_cb(on_thread_state_changed);
	openthread_start(openthread_get_default_context());

	if (IS_ENABLED(CONFIG_OPENTHREAD_MTD_SED)) {
		k_work_init(&toggle_MTD_SED_work,
			    toggle_minimal_sleepy_end_device);
		update_device_state();
	}
}

void coap_client_get_new_target_temp(uint16_t temperature_data, uint16_t humidity_data)
{
	submit_work_if_connected(&target_temp_data_container.work_obj);
}

void coap_client_send_provisioning_request(void)
{
	submit_work_if_connected(&provisioning_container.work_obj);
}

void coap_client_toggle_minimal_sleepy_end_device(void)
{
	if (IS_ENABLED(CONFIG_OPENTHREAD_MTD_SED)) {
		k_work_submit(&toggle_MTD_SED_work);
	}
}