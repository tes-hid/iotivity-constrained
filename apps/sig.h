#ifndef SIG_H
#define SIG_H

#include "oc_api.h"

#define MAX_URI_LENGTH (30)
#define CSN_LEN 10

#ifdef __cplusplus
extern "C"
{
#endif
	typedef struct discovered_device {
		struct discovered_reader *next;
		struct oc_uuid_t *dev_id;
		struct oc_endpoint_t *server;
		char uri[MAX_URI_LENGTH];
	} discovered_device_t;


	int sig_init(void);
	void sig_discovery(void);
	void sig_register(void);

#ifdef __cplusplus
}
#endif

#endif
