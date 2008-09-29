/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* weather-iwin.c - US National Weather Service IWIN forecast source
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include "weather.h"
#include "weather-priv.h"

/**
 *  Humans don't deal well with .MONDAY...SUNNY AND BLAH BLAH.TUESDAY...THEN THIS AND THAT.WEDNESDAY...RAINY BLAH BLAH.
 *  This function makes it easier to read.
 */
static gchar *
formatWeatherMsg (gchar *forecast)
{
    gchar *ptr = forecast;
    gchar *startLine = NULL;

    while (0 != *ptr) {
        if (ptr[0] == '\n' && ptr[1] == '.') {
  	    /* This removes the preamble by shifting the relevant data
	     * down to the start of the buffer. */
            if (NULL == startLine) {
                memmove (forecast, ptr, strlen (ptr) + 1);
                ptr = forecast;
                ptr[0] = ' ';
            }
            ptr[1] = '\n';
            ptr += 2;
            startLine = ptr;
        } else if (ptr[0] == '.' && ptr[1] == '.' && ptr[2] == '.' && NULL != startLine) {
            memmove (startLine + 2, startLine, (ptr - startLine) * sizeof (gchar));
            startLine[0] = ' ';
            startLine[1] = '\n';
            ptr[2] = '\n';

            ptr += 3;

        } else if (ptr[0] == '$' && ptr[1] == '$') {
            ptr[0] = ptr[1] = ' ';

        } else {
            ptr++;
        }
    }

    return forecast;
}

static void
iwin_finish (SoupSession *session, SoupMessage *msg, gpointer data)
{
    WeatherInfo *info = (WeatherInfo *)data;

    g_return_if_fail (info != NULL);

    if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
        /* forecast data is not really interesting anyway ;) */
	g_warning ("Failed to get IWIN forecast data: %d %s\n",
		   msg->status_code, msg->reason_phrase);
        request_done (info, FALSE);
	return;
    }

    info->forecast = formatWeatherMsg (g_strdup (msg->response_body->data));
    request_done (info, TRUE);
}

/* Get forecast into newly alloc'ed string */
void
iwin_start_open (WeatherInfo *info)
{
    gchar *url, *state, *zone;
    WeatherLocation *loc;
    SoupMessage *msg;

    g_return_if_fail (info != NULL);
    loc = info->location;
    g_return_if_fail (loc != NULL);

    if (loc->zone[0] == '-')
        return;

    if (info->forecast) {
	g_free (info->forecast);
	info->forecast = NULL;
    }

    if (loc->zone[0] == ':') {
	/* Met Office Region Names */
    	metoffice_start_open (info);
    	return;
    } else if (loc->zone[0] == '@') {
	/* Australian BOM forecasts */
    	bom_start_open (info);
    	return;
    }

    /* The zone for Pittsburgh (for example) is given as PAZ021 in the locations
    ** file (the PA stands for the state pennsylvania). The url used wants the state
    ** as pa, and the zone as lower case paz021.
    */
    zone = g_ascii_strdown (loc->zone, -1);
    state = g_strndup (zone, 2);

    url = g_strdup_printf ("http://weather.noaa.gov/pub/data/forecasts/zone/%s/%s.txt",
        		   state, zone);
    g_free (zone);
    g_free (state);
    
    msg = soup_message_new ("GET", url);
    g_free (url);
    soup_session_queue_message (info->session, msg, iwin_finish, info);

    info->requests_pending++;
}
