#include "oc_api.h"
#include "reader.h"

#define SAM_CSN { 0x44, 0x0A, 0x44, 0x00, 0x00, 0x00, 0xA0, 0x02, 0x96, 0x00 }
#define SAM_CSN_LEN 10

static bool card_present = true;
char *card_data[SAM_CSN_LEN] = SAM_CSN;
oc_string_t name;

static int reader_init(void)
{
	int ret = oc_init_platform("HID Global", NULL, NULL);
	ret |= oc_add_device("/oic/d", "oic.d.reader", "Reader", "ocf.1.1.0", "ocf.res.1.1.0", NULL, NULL);
	oc_new_string(&name, "SE Reader", 9);

	return ret;
}

static void reader_get(oc_request_t *request, oc_interface_mask_t interfaces, void *user_data)
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

// maybe long term this would be a "check for card" type of thing?
// all you can do for now is set the name
static void reader_post(oc_request_t *request, oc_interface_mask_t iface_mask, void *user_data)
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

static void reader_register(void)
{
	oc_resource_t *res = oc_new_resource(NULL, "/a/reader", 2, 0);
	oc_resource_bind_resource_type(res, "core.reader");
	oc_resource_bind_resource_interface(res, OC_IF_RW);
	oc_resource_set_default_interface(res, OC_IF_RW);
	oc_resource_set_discoverable(res, true);
	oc_resource_set_periodic_observable(res, 1);
	oc_resource_set_request_handler(res, OC_GET, reader_get, NULL);
	oc_resource_set_request_handler(res, OC_PUT, reader_post, NULL);
	oc_resource_set_request_handler(res, OC_POST, reader_post, NULL);
	oc_add_resource(res);
}

