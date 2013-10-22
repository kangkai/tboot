/*
 * This is a config parser for simple config file, it has the below format.
 * comments start with #
 * key<delimeters>value
 * the default delimeters are: space, tab and equality sign.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cutils/config_parser.h"

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

void config_parser_display(struct config_parser *cp)
{
	if (!cp || !cp->data)
		return;
	printf("config parser: %p\n", cp);
	printf("file name: |%s|\n", cp->path);
	printf("delimeters: |%s|\n", cp->delim);
	printf("%s", (char *)cp->data);
}

/*
 * destroy a config parser
 */
void config_parser_free(struct config_parser *cp)
{
	if (cp) {
		if (cp->data)
			free(cp->data);
		free(cp);
	}
}

/*
 * create a config parser from config file
 */
struct config_parser *config_parser_init(const char *fn)
{
	struct stat st;
	int fd;
	struct config_parser *cp;

	if (!fn || strlen(fn) == 0)
		return NULL;

	fd = open(fn, O_RDONLY);
	if (fd == -1) {
		perror("open file failed.");
		return NULL;
	}

	if (fstat(fd, &st) == -1) {
		perror("fstat failed.");
		goto err;
	}

	if (st.st_size <= 0)
		goto err;

	cp = malloc(sizeof(*cp));
	if (!cp) {
		perror("malloc failed.");
		goto err;
	}

	strncpy(cp->path, fn, sizeof(cp->path));
	cp->data = malloc(st.st_size);
	if (!cp->data) {
		perror("malloc failed.");
		goto err;
	}

	/* set default delimeters */
	if (snprintf(cp->delim, sizeof(cp->delim), "= \t") < 0) {
		fprintf(stderr, "snprintf failed.\n");
		goto err;
	}

	if ((cp->len = read(fd, cp->data, st.st_size)) <= 0) {
		perror("read file.");
		goto err;
	}

	close(fd);

	/* remove comments */
	{
		char *src, *dst;

		dst = cp->data;
		while (1) {
			dst = strchr(dst, '#');
			if (!dst)
				break;
			src = strchr(dst, '\n');
			if (!src) {
				*dst = '\0';
				break;
			}
			/* move the last '\0' also */
			/* FIXME: cp->len lost */
			memmove(dst, src, MIN(strlen(src) + 1, cp->len));
		}
	}

	return cp;
err:
	config_parser_free(cp);
	close(fd);
	return NULL;
}

char *config_parser_get(struct config_parser *cp, const char *key)
{
	char *line;
	char *end;
	char *tok1, *tok2;

	if (!key || strlen(key) <= 0)
		return NULL;

	if (!cp || !cp->data) {
		fprintf(stderr, "please init config parser first.\n");
		return NULL;
	}

	line = cp->data;
	while (1) {
		end = strchr(line, '\n');
		if (end)
			*end = '\0';
		/*
		 * strtok will change line in place, and this isn't what we want
		 * so dump it.
		 */
		if (tok1 = strtok(strdup(line), cp->delim)) {
			if (!strncmp(tok1, key, MAX(strlen(tok1), strlen(key)))) {
				tok2 = strtok(NULL, cp->delim);
				/* restore before return, otherwise the data been truncated. */
				if (end)
					*end = '\n';
				return tok2;
			}
		}
		if (!end)
			break;
		*end = '\n';
		line = end + 1;
	}

	fprintf(stderr, "not found value for %s\n", key);
	return NULL;
}

int config_parser_setdelim(struct config_parser *cp, const char *delim)
{
	if (!cp) {
		fprintf(stderr, "please init config parser first.\n");
		return -1;
	}

	if (!delim || strlen(delim) == 0)
		return -1;

	if (snprintf(cp->delim, sizeof(cp->delim), "%s", delim) < 0)
		return -1;

	return 0;
}
