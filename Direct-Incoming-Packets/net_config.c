#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "net_config.h"

struct gatekeeperd_server_conf *
gatekeeperd_get_server_conf(const char *filename, const char *server_name)
{
	FILE *fp = fopen(filename, "r");
	if (!fp)
	{
		fprintf(stderr, "cannot open %s\n", filename);
		return NULL;
	}

	struct gatekeeperd_server_conf *conf = malloc(sizeof(struct gatekeeperd_server_conf));
	memset(conf, 0, sizeof(struct gatekeeperd_server_conf));

    // TODO: fill up the gatekeeperd configurations

	fclose(fp);
	return conf;
}
