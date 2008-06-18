/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* $Id: weather-met.c 10124 2007-01-05 14:02:46Z kmaraas $ */

/*
 *  Papadimitriou Spiros <spapadim+@cs.cmu.edu>
 *
 *  This code released under the GNU GPL.
 *  Read the file COPYING for more information.
 *
 *  Weather server functions (MET)
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include <libgweather/weather.h>
#include "weather-priv.h"

static char *
met_reprocess (char *x, int len)
{
    char *p = x;
    char *o;
    int spacing = 0;
    static gchar *buf;
    static gint buflen = 0;
    gchar *lastspace = NULL;
    int count = 0;

    if (buflen < len)
    {
	if (buf)
	    g_free (buf);
	buf = g_malloc (len + 1);
	buflen = len;
    }

    o = buf;
    x += len;       /* End mark */

    while (*p && p < x) {
	if (isspace (*p)) {
	    if (!spacing) {
		spacing = 1;
		lastspace = o;
		count++;
		*o++ = ' ';
	    }
	    p++;
	    continue;
	}
	spacing = 0;
	if (count > 75 && lastspace) {
	    count = o - lastspace - 1;
	    *lastspace = '\n';
	    lastspace = NULL;
	}

	if (*p == '&') {
	    if (strncasecmp (p, "&amp;", 5) == 0) {
		*o++ = '&';
		count++;
		p += 5;
		continue;
	    }
	    if (strncasecmp (p, "&lt;", 4) == 0) {
		*o++ = '<';
		count++;
		p += 4;
		continue;
	    }
	    if (strncasecmp (p, "&gt;", 4) == 0) {
		*o++ = '>';
		count++;
		p += 4;
		continue;
	    }
	}
	if (*p == '<') {
	    if (strncasecmp (p, "<BR>", 4) == 0) {
		*o++ = '\n';
		count = 0;
	    }
	    if (strncasecmp (p, "<B>", 3) == 0) {
		*o++ = '\n';
		*o++ = '\n';
		count = 0;
	    }
	    p++;
	    while (*p && *p != '>')
		p++;
	    if (*p)
		p++;
	    continue;
	}
	*o++ = *p++;
	count++;
    }
    *o = 0;
    return buf;
}


/*
 * Parse the metoffice forecast info.
 * For gnome 3.0 we want to just embed an HTML bonobo component and
 * be done with this ;)
 */

static gchar *
met_parse (gchar *meto)
{
    gchar *p;
    gchar *rp;
    gchar *r = g_strdup ("Met Office Forecast\n");
    gchar *t;

    g_return_val_if_fail (meto != NULL, r);

    p = strstr (meto, "Summary: </b>");
    g_return_val_if_fail (p != NULL, r);

    rp = strstr (p, "Text issued at:");
    g_return_val_if_fail (rp != NULL, r);

    p += 13;
    /* p to rp is the text block we want but in HTML malformat */
    t = g_strconcat (r, met_reprocess (p, rp - p), NULL);
    g_free (r);

    return t;
}

static void
met_finish_read (GnomeVFSAsyncHandle *handle, GnomeVFSResult result,
		 gpointer buffer, GnomeVFSFileSize requested,
		 GnomeVFSFileSize body_len, gpointer data)
{
    WeatherInfo *info = (WeatherInfo *)data;
    WeatherLocation *loc;
    gchar *body, *forecast, *temp;

    g_return_if_fail (info != NULL);
    g_return_if_fail (handle == info->met_handle);

    info->forecast = NULL;
    loc = info->location;
    body = (gchar *)buffer;
    body[body_len] = '\0';

    if (info->met_buffer == NULL)
        info->met_buffer = g_strdup (body);
    else {
        temp = g_strdup (info->met_buffer);
	g_free (info->met_buffer);
	info->met_buffer = g_strdup_printf ("%s%s", temp, body);
	g_free (temp);
    }

    if (result == GNOME_VFS_ERROR_EOF) {
	forecast = met_parse (info->met_buffer);
        info->forecast = forecast;
    } else if (result != GNOME_VFS_OK) {
	g_print ("%s", gnome_vfs_result_to_string (result));
	info->met_handle = NULL;
	requests_done_check (info);
        g_warning ("Failed to get Met Office data.\n");
    } else {
        gnome_vfs_async_read (handle, body, DATA_SIZE - 1, met_finish_read, info);
        return;
    }

    request_done (info->met_handle, info);
    g_free (buffer);
    return;
}

static void
met_finish_open (GnomeVFSAsyncHandle *handle, GnomeVFSResult result, gpointer data)
{
    WeatherInfo *info = (WeatherInfo *)data;
    WeatherLocation *loc;
    gchar *body;

    g_return_if_fail (info != NULL);
    g_return_if_fail (handle == info->met_handle);

    body = g_malloc0 (DATA_SIZE);

    info->met_buffer = NULL;
    if (info->forecast)
    	g_free (info->forecast);
    info->forecast = NULL;
    loc = info->location;
    g_return_if_fail (loc != NULL);

    if (result != GNOME_VFS_OK) {
        g_warning ("Failed to get Met Office forecast data.\n");
        info->met_handle = NULL;
        requests_done_check (info);
        g_free (body);
    } else {
    	gnome_vfs_async_read (handle, body, DATA_SIZE - 1, met_finish_read, info);
    }
    return;
}

void
metoffice_start_open (WeatherInfo *info)
{
    gchar *url;
    WeatherLocation *loc;
    loc = info->location;

    url = g_strdup_printf ("http://www.metoffice.gov.uk/weather/europe/uk/%s.html", loc->zone + 1);

    gnome_vfs_async_open (&info->met_handle, url, GNOME_VFS_OPEN_READ,
			  0, met_finish_open, info);
    g_free (url);
}
