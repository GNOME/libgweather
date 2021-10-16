/* test_metar.c: Simple program to reproduce METAR parsing results from command line
 *
 * SPDX-FileCopyrightText: The GWeather authors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "gweather-private.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

#ifndef BUFLEN
#define BUFLEN 4096
#endif /* BUFLEN */

static void
print_info (GWeatherInfo *info)
{
    if (gweather_info_is_valid (info)) {
        g_message ("Weather updated successfully for %s", info->location.code);
    } else {
        g_warning ("Failed to parse weather for %s", info->location.code);
        return;
    }

    printf ("Returned info:\n");
    printf ("  update:   %s", ctime (&info->update));
    printf ("  sky:      %s\n", gweather_info_get_sky (info));
    printf ("  cond:     %s\n", gweather_info_get_conditions (info));
    printf ("  temp:     %s\n", gweather_info_get_temp (info));
    printf ("  dewp:     %s\n", gweather_info_get_dew (info));
    printf ("  wind:     %s\n", gweather_info_get_wind (info));
    printf ("  pressure: %s\n", gweather_info_get_pressure (info));
    printf ("  vis:      %s\n", gweather_info_get_visibility (info));

    // TODO: retrieve location's lat/lon to display sunrise/set times
}

static void
weather_updated_cb (GWeatherInfo *info,
                    gpointer user_data)
{
    print_info (info);
    g_main_loop_quit (user_data);
}

int
main (int argc, char **argv)
{
    FILE *stream = stdin;
    gchar *filename = NULL;
    gchar *code = NULL;
    GOptionEntry entries[] = {
        { "file", 'f', 0, G_OPTION_ARG_FILENAME, &filename, "file containing METAR observations", NULL },
        { "code", 'c', 0, G_OPTION_ARG_STRING, &code, "ICAO code to get METAR observations from", NULL },
        { NULL }
    };
    GOptionContext *context;
    GError *error = NULL;
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

    if (code) {
        GMainLoop *loop;

        loop = g_main_loop_new (NULL, TRUE);

        info = g_object_new (GWEATHER_TYPE_INFO, NULL);
        info->location.code = g_strdup (code);
        info->location.latlon_valid = TRUE;
        info->session = soup_session_new ();
        g_signal_connect (G_OBJECT (info), "updated", G_CALLBACK (weather_updated_cb), loop);

        metar_start_open (info);

        g_main_loop_run (loop);

        return 0;
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
        info->valid = 1;
        metar_parse (buf, info);
        print_info (info);
    }
    return 0;
}
