/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "oc_api.h"
#include "port/oc_clock.h"
#include <signal.h>
#include <windows.h>

int quit = 0;

#define SAM_CSN { 0x44, 0x0A, 0x44, 0x00, 0x00, 0x00, 0xA0, 0x02, 0x96, 0x00 }
#define SAM_CSN_LEN 10

static CONDITION_VARIABLE cv;
static CRITICAL_SECTION cs;

static bool card_present = true;
char *card_data[SAM_CSN_LEN] = SAM_CSN;
oc_string_t name;

static int app_init(void)
{
	int ret = oc_init_platform("HID Global", NULL, NULL);
	ret |= oc_add_device("/oic/d", "oic.d.reader", "Reader", "ocf.1.1.0",
		"ocf.res.1.1.0", NULL, NULL);
	oc_new_string(&name, "SE Reader", 9);

	return ret;
}


static void get_reader(oc_request_t *request, oc_interface_mask_t interfaces, void *user_data)
{
	(void)user_data;

	PRINT("GET_reader:\n");
	oc_rep_start_root_object();

	switch (interfaces) {
	case OC_IF_BASELINE:
		oc_process_baseline_interface(request->resource);
	case OC_IF_RW:
		//oc_rep_start_object(root, card);
		oc_rep_set_boolean(root, card_present, card_present);
		oc_rep_set_byte_string(root, card_data, &card_data, SAM_CSN_LEN);
		//oc_rep_end_object(root, card);

		oc_rep_set_text_string(root, name, oc_string(name));
		break;
	default:
		break;
	}
	oc_rep_end_root_object();
	oc_send_response(request, OC_STATUS_OK);
}

static void post_reader(oc_request_t *request, oc_interface_mask_t iface_mask, void *user_data)
{
	(void)user_data;
	(void)iface_mask;

	PRINT("POST_reader:\n");
	oc_rep_t *rep = request->request_payload;

	while (rep != NULL) {
		PRINT("key: %s ", oc_string(rep->name));
		switch (rep->type) {
		case OC_REP_BOOL:
			PRINT("Cannot set card_present...\n");
			break;
		case OC_REP_STRING:
			oc_free_string(&name);
			oc_new_string(&name, oc_string(rep->value.string), oc_string_len(rep->value.string));
			PRINT("name: %s\n", oc_string(name));
			break;
		default:
			oc_send_response(request, OC_STATUS_BAD_REQUEST);
			return;
			break;
		}

		rep = rep->next;
	}

	oc_send_response(request, OC_STATUS_CHANGED);
}


static void register_resources(void)
{
	oc_resource_t *res = oc_new_resource(NULL, "/a/reader", 2, 0);
	oc_resource_bind_resource_type(res, "core.reader");
	oc_resource_bind_resource_interface(res, OC_IF_RW);
	oc_resource_set_default_interface(res, OC_IF_RW);
	oc_resource_set_discoverable(res, true);
	oc_resource_set_periodic_observable(res, 1);
	oc_resource_set_request_handler(res, OC_GET, get_reader, NULL);
	oc_resource_set_request_handler(res, OC_PUT, post_reader, NULL);
	oc_resource_set_request_handler(res, OC_POST, post_reader, NULL);
	oc_add_resource(res);
}

static void signal_event_loop(void)
{
	WakeConditionVariable(&cv);
}

void handle_signal(int signal)
{
	signal_event_loop();
	quit = 1;
}

int main(void)
{
	InitializeCriticalSection(&cs);
	InitializeConditionVariable(&cv);

	int init;

	signal(SIGINT, handle_signal);

	static const oc_handler_t handler = { .init = app_init,
										 .signal_event_loop = signal_event_loop,
										 .register_resources = register_resources,
										 .requests_entry = 0 };

	oc_clock_time_t next_event;

	//#ifdef OC_SECURITY
	//  oc_storage_config("./simpleserver_creds/");
	//#endif /* OC_SECURITY */

	init = oc_main_init(&handler);
	if (init < 0)
		return init;

	while (quit != 1) {
		next_event = oc_main_poll();
		if (next_event == 0) {
			SleepConditionVariableCS(&cv, &cs, INFINITE);
		}
		else {
			oc_clock_time_t now = oc_clock_time();
			if (now < next_event) {
				SleepConditionVariableCS(&cv, &cs,
					(DWORD)((next_event - now) * 1000 / OC_CLOCK_SECOND));
			}
		}
	}

	oc_main_shutdown();
	return 0;
}