/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* weather-bom.c - Australian Bureau of Meteorology forecast source
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

#include <string.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include "weather-priv.h"

static void
bom_finish (SoupSession *session, SoupMessage *msg, gpointer data)
{
    GWeatherInfo *info = (GWeatherInfo *)data;
    char *p, *rp;

    g_return_if_fail (info != NULL);

    if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
        g_warning ("Failed to get BOM forecast data: %d %s.\n",
		   msg->status_code, msg->reason_phrase);
        _gweather_info_request_done (info);
	return;
    }

    p = strstr (msg->response_body->data, "Forecast for the rest");
    if (p != NULL) {
        rp = strstr (p, "The next routine forecast will be issued");
        if (rp == NULL)
            info->priv->forecast = g_strdup (p);
        else
            info->priv->forecast = g_strndup (p, rp - p);
    }

    if (info->priv->forecast == NULL)
        info->priv->forecast = g_strdup (msg->response_body->data);

    g_print ("%s\n",  info->priv->forecast);
    _gweather_info_request_done (info);
}

void
bom_start_open (GWeatherInfo *info)
{
    gchar *url;
    SoupMessage *msg;
    WeatherLocation *loc;

    loc = &info->priv->location;

    url = g_strdup_printf ("http://www.bom.gov.au/fwo/%s.txt",
			   loc->zone + 1);

    msg = soup_message_new ("GET", url);
    soup_session_queue_message (info->priv->session, msg, bom_finish, info);
    g_free (url);

    info->priv->requests_pending++;
}
