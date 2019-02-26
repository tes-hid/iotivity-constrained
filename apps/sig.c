#include "sig.h"
#include "oc_api.h"
#include "util/oc_memb.h"

// arbitrary, just a signal that the status state hasn't been setup yet
#define READER_INIT_STATE -99

static char a_reader[MAX_URI_LENGTH];
static oc_endpoint_t *reader_server;
static oc_uuid_t *reader_uuid;

static bool card_present;
static oc_string_t card_data;
static oc_string_t reader_name;
static int device_state = READER_INIT_STATE;

static oc_string_t name;

int sig_init(void)
{
	int ret = oc_init_platform("HID Global", NULL, NULL);
	ret |= oc_add_device("/oic/d", "oic.d.sig", "SIG", "ocf.1.0.0", "ocf.res.1.0.0", NULL, NULL);
	oc_new_string(&name, "Secure Gateway", 14);
	return ret;
}

static void sig_get(oc_request_t *request, oc_interface_mask_t interfaces, void *user_data)
{
	(void)user_data;

	PRINT("GET_sig:\n");
	oc_rep_start_root_object();

	switch (interfaces) {
		case OC_IF_BASELINE:
			oc_process_baseline_interface(request->resource);
		case OC_IF_RW:
			// should return info on all of the readers disovered
			oc_rep_set_text_string(root, name, oc_string(name));
			break;
		default:
			break;
	}
	oc_rep_end_root_object();
	oc_send_response(request, OC_STATUS_OK);
}

// all you can do for now is set the name
static void sig_post(oc_request_t *request, oc_interface_mask_t iface_mask, void *user_data)
{
	(void)user_data;
	(void)iface_mask;

	PRINT("POST_reader:\n");
	oc_rep_t *rep = request->request_payload;

	while (rep != NULL) {
		PRINT("key: %s ", oc_string(rep->name));
		switch (rep->type) {
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

static oc_event_callback_retval_t start_observe(void *data) {
	(void)data;
	PRINT("Start OBSERVE\n");
	oc_do_observe(&a_reader, reader_server, NULL, &handle_reader_payload, LOW_QOS, data);
	return OC_EVENT_CONTINUE;
}

static oc_event_callback_retval_t stop_observe(void *data)
{
	(void)data;
	PRINT("Stopping OBSERVE\n");
	oc_stop_observe(a_reader, reader_server);
	return OC_EVENT_DONE;
}

static void device_status_changed(oc_uuid_t *device_id, int status, void *data) {
	(void)data;

	// probably should make sure this is the right uuid for the reader...

	if (device_state == READER_INIT_STATE) {
		OC_DBG("Device is being initialized");
	}

	switch (status) {
		case 0:
			// 0 means we're on-boarded and everything is fine (pretty sure)
			start_observe(data);
			break;
		case -1:
			// something has gone wrong
			stop_observe(data);
			break;
		default:
			PRINT("Device state changed from %d to %d\n", device_state, status);
			break;
	}

	device_state = status;
}

// this is standard resource discovery, using the unowned device discovery below
//static oc_discovery_flags_t sig_ip_discovery(const char *anchor, const char *uri, oc_string_array_t types, oc_interface_mask_t iface_mask, oc_endpoint_t *endpoint,
//	oc_resource_properties_t bm, void *user_data)
//{
//	(void)anchor;
//	(void)user_data;
//	(void)iface_mask;
//	(void)bm;
//	int i;
//	size_t uri_len = strlen(uri);
//	uri_len = (uri_len >= MAX_URI_LENGTH) ? MAX_URI_LENGTH - 1 : uri_len;
//	PRINT("\n\nDISCOVERYCB %s %s %zd\n\n", anchor, uri,
//		oc_string_array_get_allocated_size(types));
//	for (i = 0; i < (int)oc_string_array_get_allocated_size(types); i++) {
//		char *t = oc_string_array_get_item(types, i);
//		PRINT("\n\nDISCOVERED RES %s\n\n\n", t);
//		if (strlen(t) == 11 && strncmp(t, "core.reader", 11) == 0) {
//			reader_server = endpoint;
//			strncpy(a_reader, uri, uri_len);
//			a_reader[uri_len] = '\0';
//
//			PRINT("Resource %s hosted at endpoints:\n", a_reader);
//			oc_endpoint_t *ep = endpoint;
//			while (ep != NULL) {
//				PRINTipaddr(*ep);
//				PRINT("\n");
//				ep = ep->next;
//			}
//
//			oc_do_get(a_reader, reader_server, NULL, &handle_reader_payload, LOW_QOS, NULL);
//
//			return OC_STOP_DISCOVERY;
//		}
//	}
//	oc_free_server_endpoints(endpoint);
//	return OC_CONTINUE_DISCOVERY;
//}

static oc_discovery_flags_t discovered_devices(oc_uuid_t *uuid, oc_endpoint_t *endpoint, void *data) {
	(void)data;
	// TODO: handle multiple devices
	reader_server = endpoint;
	reader_uuid = uuid;
	PRINT("Resource %s hosted at endpoints:\n", a_reader);
	oc_endpoint_t *ep = endpoint;
	while (ep != NULL) {
		PRINTipaddr(*ep);
		PRINT("\n");
		ep = ep->next;
	}

	oc_obt_perform_just_works_otm(reader_uuid, &device_status_changed, NULL);

	return OC_STOP_DISCOVERY;
}

void sig_discovery(void) {
	//oc_do_ip_discovery("core.reader", &sig_ip_discovery, NULL);
	oc_obt_discover_unowned_devices(&discovered_devices, NULL);
}

void sig_register(void)
{
	oc_resource_t *res = oc_new_resource(NULL, "/a/sig", 2, 0);
	oc_resource_bind_resource_type(res, "core.sig");
	oc_resource_bind_resource_interface(res, OC_IF_RW);
	oc_resource_set_default_interface(res, OC_IF_RW);
	oc_resource_set_discoverable(res, true);
	oc_resource_set_periodic_observable(res, 1);
	oc_resource_set_request_handler(res, OC_GET, sig_get, NULL);
	oc_resource_set_request_handler(res, OC_PUT, sig_post, NULL);
	oc_resource_set_request_handler(res, OC_POST, sig_post, NULL);
	oc_add_resource(res);
}
