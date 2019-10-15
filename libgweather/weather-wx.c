/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* weather-wx.c - Weather server functions (WX Radar)
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

#include "gweather-private.h"

static void
wx_finish (SoupSession *session, SoupMessage *msg, gpointer data)
{
    GWeatherInfo *info;
    GWeatherInfoPrivate *priv;
    GdkPixbufAnimation *animation;

    if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
	if (msg->status_code == SOUP_STATUS_CANCELLED) {
	    g_debug ("Failed to get radar map image: %d %s.\n",
		       msg->status_code, msg->reason_phrase);
	    return;
	}
	g_warning ("Failed to get radar map image: %d %s.\n",
		   msg->status_code, msg->reason_phrase);
	info = data;
	g_object_unref (info->priv->radar_loader);
	_gweather_info_request_done (info, msg);
	return;
    }

    info = data;
    priv = info->priv;

    gdk_pixbuf_loader_close (priv->radar_loader, NULL);
    animation = gdk_pixbuf_loader_get_animation (priv->radar_loader);
    if (animation != NULL) {
	if (priv->radar)
	    g_object_unref (priv->radar);
	priv->radar = animation;
	g_object_ref (priv->radar);
    }
    g_object_unref (priv->radar_loader);

    _gweather_info_request_done (info, msg);
}

static void
wx_got_chunk (SoupMessage *msg, SoupBuffer *chunk, gpointer data)
{
    GWeatherInfo *info = (GWeatherInfo *)data;
    GError *error = NULL;

    g_return_if_fail (info != NULL);

    gdk_pixbuf_loader_write (info->priv->radar_loader, (guchar *)chunk->data,
			     chunk->length, &error);
    if (error) {
	g_print ("%s \n", error->message);
	g_error_free (error);
    }
}

/* Get radar map and into newly allocated pixmap */
void
wx_start_open (GWeatherInfo *info)
{
    gchar *url;
    SoupMessage *msg;
    WeatherLocation *loc;
    GWeatherInfoPrivate *priv;

    g_return_if_fail (info != NULL);
    priv = info->priv;

    priv->radar = NULL;
    priv->radar_loader = gdk_pixbuf_loader_new ();
    loc = &priv->location;

    if (!loc->latlon_valid)
	return;

    if (priv->radar_url)
	url = g_strdup (priv->radar_url);
    else {
	if (loc->radar[0] == '-')
	    return;
	url = g_strdup_printf ("http://image.weather.com/web/radar/us_%s_closeradar_medium_usen.jpg", loc->radar);
    }
 
    msg = soup_message_new ("GET", url);
    if (!msg) {
	g_warning ("Invalid radar URL: %s\n", url);
	g_free (url);
	return;
    }

    g_signal_connect (msg, "got-chunk", G_CALLBACK (wx_got_chunk), info);
    soup_message_body_set_accumulate (msg->response_body, FALSE);
    _gweather_info_begin_request (info, msg);
    soup_session_queue_message (priv->session, msg, wx_finish, info);

    g_free (url);
}
