#ifndef __CONFIG_PARSER_H
#define __CONFIG_PARSER_H
#include <limits.h>

struct config_parser {
	char path[PATH_MAX];
	void *data;
	size_t len;	// valid data len
	char delim[8];	// delim characters
};

struct config_parser *config_parser_init(const char *fn);
void config_parser_exit(struct config_parser *cp);
char *config_parser_get(struct config_parser *cp, const char *key);
void config_parser_display(struct config_parser *cp);
int config_parser_setdelim(struct config_parser *cp, const char *delim);

#endif
