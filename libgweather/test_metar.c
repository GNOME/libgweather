/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Simple program to reproduce METAR parsing results from command line
 */

#include <glib.h>
#include <string.h>
#include <stdio.h>
#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include "weather-priv.h"

#ifndef BUFLEN
#define BUFLEN 4096
#endif /* BUFLEN */

int
main (int argc, char **argv)
{
    FILE*  stream = stdin;
    gchar* filename = NULL;
    GOptionEntry entries[] = {
	{ "file", 'f', 0, G_OPTION_ARG_FILENAME, &filename,
	  "file constaining metar observations", NULL },
	{ NULL }
    };
    GOptionContext* context;
    GError* error = NULL;
    char buf[BUFLEN];
    int len;
    GWeatherInfo *info;

    context = g_option_context_new ("- test libgweather metar parser");
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_parse (context, &argc, &argv, &error);

    if (error) {
	perror (error->message);
	return error->code;
    }
    if (filename) {
	stream = fopen (filename, "r");
	if (!stream) {
	    perror ("fopen");
	    return -1;
	}
    } else {
	fprintf (stderr, "Enter a METAR string...\n");
    }

    while (fgets (buf, sizeof (buf), stream)) {
	len = strlen (buf);
	if (buf[len - 1] == '\n') {
	    buf[--len] = '\0';
	}
	printf ("\n%s\n", buf);

	/* a bit hackish... */
	info = g_object_new (GWEATHER_TYPE_INFO, NULL);
	info->priv->valid = 1;
	metar_parse (buf, info);
	printf ("Returned info:\n");
	printf ("  update:   %s", ctime (&info->priv->update));
	printf ("  sky:      %s\n", gweather_info_get_sky (info));
	printf ("  cond:     %s\n", gweather_info_get_conditions (info));
	printf ("  temp:     %s\n", gweather_info_get_temp (info));
	printf ("  dewp:     %s\n", gweather_info_get_dew (info));
	printf ("  wind:     %s\n", gweather_info_get_wind (info));
	printf ("  pressure: %s\n", gweather_info_get_pressure (info));
	printf ("  vis:      %s\n", gweather_info_get_visibility (info));

	// TODO: retrieve location's lat/lon to display sunrise/set times
    }
    return 0;
}
