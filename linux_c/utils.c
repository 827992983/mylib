#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "utils.h"

char * trim (char *string)
{
	register char *s, *t;

	if (string == NULL)
	{
		return NULL;
	}

	for (s = string; isspace (*s); s++)
		;

	if (*s == 0)
		return s;

	t = s + strlen (s) - 1;
	while (t > s && isspace (*t))
		t--;
	*++t = '\0';

	return s;
}

int strsplit (const char *str, const char *delim,  char ***tokens, int *token_count)
{
	char *_running = NULL;
	char *running = NULL;
	char *token = NULL;
	char **token_list = NULL;
	int count = 0;
	int i = 0;
	int j = 0;

	if (str == NULL || delim == NULL || tokens == NULL || token_count == NULL)
	{
		return -1;
	}

	if ((_running = strdup (str)) == NULL)
	{
		return -1;
	}
	running = _running;

	while ((token = strsep (&running, delim)) != NULL)
	{
		if (token[0] != '\0')
			count++;
	}
	free (_running);

	if ((_running = strdup (str)) == NULL)
	{
		return -1;
	}
	running = _running;

	if ((token_list = (char **)calloc(count, sizeof(char))) == NULL)
	{
		free (_running);
		return -1;
	}

	while ((token = strsep (&running, delim)) != NULL)
	{
		if (token[0] == '\0')
			continue;

		if ((token_list[i++] = strdup (token)) == NULL)
			goto free_exit;
	}

	free (_running);

	*tokens = token_list;
	*token_count = count;
	return 0;

free_exit:
	free (_running);
	for (j = 0; j < i; j++)
	{
		free (token_list[j]);
	}
	free (token_list);
	return -1;
}


