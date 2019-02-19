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
#include "oc_obt.h"
#include "port/oc_clock.h"
#include <signal.h>
#include <windows.h>
#include <string.h>

int quit = 0;

static CONDITION_VARIABLE cv;
static CRITICAL_SECTION cs;

static int
app_init(void)
{
  int ret = oc_init_platform("HID Global", NULL, NULL);
  ret |= oc_add_device("/oic/d", "oic.d.sig", "SIG", "ocf.1.0.0",
                       "ocf.res.1.0.0", NULL, NULL);
  return ret;
}

#define MAX_URI_LENGTH (30)
#define CSN_LEN 10
static char a_reader[MAX_URI_LENGTH];
static oc_endpoint_t *reader_server;
static oc_uuid_t *reader_uuid;

static bool card_present;
static oc_string_t card_data;
static oc_string_t name;

static oc_event_callback_retval_t stop_observe(void *data)
{
  (void)data;
  PRINT("Stopping OBSERVE\n");
  oc_stop_observe(a_reader, reader_server);
  return OC_EVENT_DONE;
}

static void handle_reader_payload(oc_client_response_t *data) {
	PRINT("GET_reader:\n");
	oc_rep_t *rep = data->payload;
	while (rep != NULL) {
		PRINT("key %s, value ", oc_string(rep->name));
		switch (rep->type) {
		case OC_REP_BOOL:
			PRINT("%d\n", rep->value.boolean);
			card_present = rep->value.boolean;
			break;
		case OC_REP_STRING:
			PRINT("%s\n", oc_string(rep->value.string));
			if (oc_string(rep->name) == "card_data") {
				if (oc_string_len(card_data))
					oc_free_string(&card_data);
				oc_new_string(&card_data, oc_string(rep->value.string),
					oc_string_len(rep->value.string));
			}
			else {
				if (oc_string_len(name))
					oc_free_string(&name);
				oc_new_string(&name, oc_string(rep->value.string),
					oc_string_len(rep->value.string));
			}
			break;
		default:
			break;
		}
		rep = rep->next;
	}
}

static oc_discovery_flags_t discovery(const char *anchor, const char *uri, oc_string_array_t types, oc_interface_mask_t iface_mask, oc_endpoint_t *endpoint,
          oc_resource_properties_t bm, void *user_data)
{
  (void)anchor;
  (void)user_data;
  (void)iface_mask;
  (void)bm;
  int i;
  size_t uri_len = strlen(uri);
  uri_len = (uri_len >= MAX_URI_LENGTH) ? MAX_URI_LENGTH - 1 : uri_len;
  PRINT("\n\nDISCOVERYCB %s %s %zd\n\n", anchor, uri,
        oc_string_array_get_allocated_size(types));
  for (i = 0; i < (int)oc_string_array_get_allocated_size(types); i++) {
    char *t = oc_string_array_get_item(types, i);
    PRINT("\n\nDISCOVERED RES %s\n\n\n", t);
    if (strlen(t) == 11 && strncmp(t, "core.reader", 11) == 0) {
      reader_server = endpoint;
      strncpy(a_reader, uri, uri_len);
      a_reader[uri_len] = '\0';

      PRINT("Resource %s hosted at endpoints:\n", a_reader);
      oc_endpoint_t *ep = endpoint;
      while (ep != NULL) {
        PRINTipaddr(*ep);
        PRINT("\n");
        ep = ep->next;
      }

      oc_do_get(a_reader, reader_server, NULL, &handle_reader_payload, LOW_QOS, NULL);

      return OC_STOP_DISCOVERY;
    }
  }
  oc_free_server_endpoints(endpoint);
  return OC_CONTINUE_DISCOVERY;
}

static void reader_status_changed(oc_uuid_t *device_id, int status, void *data) {
	(void)data;
	// probably should make sure this is the right uuid for the reader...
	PRINT("Reader status changed to %d\n", status);
}

static oc_discovery_flags_t discovered_devices(oc_uuid_t *uuid, oc_endpoint_t *endpoint, void *data) {
	(void)data;

	reader_server = endpoint;
	reader_uuid = uuid;
	PRINT("Resource %s hosted at endpoints:\n", a_reader);
	oc_endpoint_t *ep = endpoint;
	while (ep != NULL) {
		PRINTipaddr(*ep);
		PRINT("\n");
		ep = ep->next;
	}

	oc_obt_perform_just_works_otm(reader_uuid, &reader_status_changed, NULL);

	oc_free_server_endpoints(endpoint);

	return OC_CONTINUE_DISCOVERY;
}

static void issue_requests(void)
{
  //oc_do_ip_discovery("core.reader", &discovery, NULL);
	oc_obt_discover_unowned_devices(&discovered_devices, NULL);
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

  static const oc_handler_t handler = {.init = app_init,
                                       .signal_event_loop = signal_event_loop,
                                       .register_resources = 0,
                                       .requests_entry = issue_requests };

  oc_clock_time_t next_event;

#ifdef OC_SECURITY
  //oc_storage_config("./simpleclient_creds/");
#endif /* OC_SECURITY */

  init = oc_main_init(&handler);
  if (init < 0)
    return init;

  while (quit != 1) {
    next_event = oc_main_poll();
    if (next_event == 0) {
      SleepConditionVariableCS(&cv, &cs, INFINITE);
    } else {
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
