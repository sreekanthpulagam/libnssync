#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include <jansson.h>

#include "request.h"
#include "storage.h"

struct nssync_storage {
	char *username;
	char *password;

	char *base;
};

struct nssync_storage_obj {
	char *id;
	time_t modified;
	int sortindex;
	int ttl;
	uint8_t *payload;
};

static int saprintf(char **str_out, const char *format, ...)
{
	va_list ap;
	int slen = 0;
	char *str = NULL;

	va_start(ap, format);
	slen = vsnprintf(str, slen, format, ap);
	va_end(ap);

	str = malloc(slen + 1);
	if (str == NULL) {
		return -1;
	}

	va_start(ap, format);
	slen = vsnprintf(str, slen + 1, format, ap);
	va_end(ap);

	if (slen < 0) {
		free(str);
	} else {
		*str_out = str;
	}

	return slen;
}

struct nssync_storage *
nssync_storage_new(const char *server,
		   const char *pathname,
		   const char *username,
		   const char *password)
{
	struct nssync_storage *newstore; /* new storage service */
	const char *fmt;

	newstore = calloc(1, sizeof(*newstore));
	if (newstore == NULL) {
		return NULL;
	}

	newstore->username = strdup(username);
	newstore->password = strdup(password);

	/* alter the format specifier depending on separator requirements */
	if (server[strlen(server) - 1] ==  '/') {
		if ((pathname[0] == 0) || 
		    (pathname[strlen(pathname) - 1] == '/')) {
			fmt = "%s%s1.1/%s";
		} else {
			fmt = "%s%s/1.1/%s";			
		}
	} else {
		if ((pathname[0] == 0) || 
		    (pathname[strlen(pathname) - 1] == '/')) {
			fmt = "%s/%s1.1/%s";
		} else {
			fmt = "%s/%s/1.1/%s";			
		}
	}

	if (saprintf(&newstore->base, fmt, server, pathname, username) < 0) {
		free(newstore);
		return NULL;
	}

	return newstore;

}

int
nssync_storage_free(struct nssync_storage *store)
{
	free(store->base);
	free(store->username);
	free(store->password);
	free(store);
	return 0;
}

struct nssync_storage_obj *
nssync_storage_obj_fetch(struct nssync_storage *store, const char *path)
{
	struct nssync_storage_obj *obj;
	char *reply;
	char *url;
	json_t *root;
	json_error_t error;
	json_t *payload;

	if (saprintf(&url, "%s/%s",
		     store->base,path) < 0) {
		return NULL;
	}

	/* printf("requesting:%s\n", url); */

	reply = nssync__request(url, store->username, store->password);
	free(url);
	if (reply == NULL) {
		return NULL;
	}
	/* printf("%s\n\n", reply); */

	obj = calloc(1, sizeof(*obj));
	if (obj == NULL) {
		free(reply);
		return NULL;
	}

	root = json_loads(reply, 0, &error);
	free(reply);

	if (!root) {
		fprintf(stderr, "error: on line %d of reply: %s\n",
			error.line, error.text);
		return NULL;
	}

	if(!json_is_object(root)) {
		fprintf(stderr, "error: root is not an object\n");
		json_decref(root);
		return NULL;
	}

	payload = json_object_get(root, "payload");
	if (!json_is_string(payload)) {
		fprintf(stderr, "error: payload is not a string\n");
		json_decref(root);
		return NULL;
	}

	obj->payload = strdup(json_string_value(payload));

	json_decref(root);

	return obj;
}

int
nssync_storage_obj_free(struct nssync_storage_obj *obj)
{
	if (obj->payload) {
		free(obj->payload);
	}
	free(obj);
	return 0;
}

uint8_t *
nssync_storage_obj_payload(struct nssync_storage_obj *obj)
{
	return obj->payload;
}
