/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* weather-yahoo.c - Yahoo! Weather service.
 *
 * Copyright 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
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

#define _GNU_SOURCE /* for strptime */
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <glib.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include "weather-priv.h"

#define XC(t) ((const xmlChar *)(t))

static GWeatherConditions condition_codes[] = {
    { TRUE, GWEATHER_PHENOMENON_TORNADO, GWEATHER_QUALIFIER_NONE }, /* tornado */
    { TRUE, GWEATHER_PHENOMENON_INVALID, GWEATHER_QUALIFIER_NONE }, /* FIXME: tropical storm */
    { TRUE, GWEATHER_PHENOMENON_INVALID, GWEATHER_QUALIFIER_NONE }, /* FIXME: hurricane */
    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM }, /* FIXME: severe thunderstorms */
    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM }, /* thunderstorms */
    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_NONE }, /* FIXME: mixed rain and snow */
    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_NONE }, /* FIXME: mixed rain and sleet */
    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_NONE }, /* FIXME: mixed snow and sleet */
    { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_FREEZING }, /* freezing drizzle */
    { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_NONE }, /* drizzle */
    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_FREEZING }, /* freezing rain */
    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_SHOWERS }, /* showers */
    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_SHOWERS }, /* showers */
    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_BLOWING }, /* FIXME: snow flurries */
    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_LIGHT }, /* FIXME: light snow showers */
    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_BLOWING }, /* blowing snow */
    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_NONE }, /* snow */
    { TRUE, GWEATHER_PHENOMENON_HAIL, GWEATHER_QUALIFIER_NONE }, /* hail */
    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_NONE }, /* sleet */
    { TRUE, GWEATHER_PHENOMENON_DUST, GWEATHER_QUALIFIER_NONE }, /* dust */
    { TRUE, GWEATHER_PHENOMENON_FOG, GWEATHER_QUALIFIER_NONE }, /* foggy */
    { TRUE, GWEATHER_PHENOMENON_HAZE, GWEATHER_QUALIFIER_NONE }, /* haze */
    { TRUE, GWEATHER_PHENOMENON_SMOKE, GWEATHER_QUALIFIER_NONE }, /* smoky */
    { TRUE, GWEATHER_PHENOMENON_INVALID, GWEATHER_QUALIFIER_NONE }, /* FIXME: blustery */
    { TRUE, GWEATHER_PHENOMENON_INVALID, GWEATHER_QUALIFIER_NONE }, /* FIXME: windy */
    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE }, /* cold */
    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE }, /* cloudy */
    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE }, /* mostly cloudy (night) */
    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE }, /* mostly cloudy (day) */
    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE }, /* partly cloudy (night) */
    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE }, /* partly cloudy (day) */
    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE }, /* clear (night) */
    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE }, /* sunny */
    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE }, /* fair (night) */
    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE }, /* fair (day) */
    { TRUE, GWEATHER_PHENOMENON_HAIL, GWEATHER_QUALIFIER_NONE }, /* FIXME: mixed_rain_and_hail */
    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE }, /* hot */
    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM }, /* FIXME: isolated thunderstorms */
    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM }, /* FIXME: scattered thunderstorms */
    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM }, /* FIXME: scattered thunderstorms */
    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_SHOWERS }, /* FIXME: scattered showers */
    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_HEAVY }, /* heavy snow */
    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_SHOWERS }, /* FIXME: scattered snow showers */
    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_HEAVY }, /* heavy snow */
    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE }, /* partly cloudy */
    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM }, /* FIXME: thundershowers */
    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_SHOWERS }, /* snow showers */
    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM }, /* FIXME: isolated thundershowers */
};

/* FIXME: check sky values for codes that have a phenomenon too
   (scattered is what weather-iwin.c does for rain and snow)
*/
static GWeatherSky sky_codes[] = {
    GWEATHER_SKY_INVALID, /* tornado */
    GWEATHER_SKY_SCATTERED, /* tropical storm */
    GWEATHER_SKY_SCATTERED, /* hurricane */
    GWEATHER_SKY_SCATTERED, /* severe thunderstorms */
    GWEATHER_SKY_SCATTERED, /* thunderstorms */
    GWEATHER_SKY_SCATTERED, /* mixed rain and snow */
    GWEATHER_SKY_SCATTERED, /* mixed rain and sleet */
    GWEATHER_SKY_SCATTERED, /* mixed snow and sleet */
    GWEATHER_SKY_SCATTERED, /* freezing drizzle */
    GWEATHER_SKY_SCATTERED, /* drizzle */
    GWEATHER_SKY_SCATTERED, /* freezing rain */
    GWEATHER_SKY_SCATTERED, /* showers */
    GWEATHER_SKY_SCATTERED, /* showers */
    GWEATHER_SKY_SCATTERED, /* snow flurries */
    GWEATHER_SKY_SCATTERED, /* light snow showers */
    GWEATHER_SKY_SCATTERED, /* blowing snow */
    GWEATHER_SKY_SCATTERED, /* snow */
    GWEATHER_SKY_INVALID, /* hail */
    GWEATHER_SKY_INVALID, /* sleet */
    GWEATHER_SKY_INVALID, /* dust */
    GWEATHER_SKY_INVALID, /* foggy */
    GWEATHER_SKY_INVALID, /* haze */
    GWEATHER_SKY_INVALID, /* smoky */
    GWEATHER_SKY_INVALID, /* blustery */
    GWEATHER_SKY_INVALID, /* windy */
    GWEATHER_SKY_CLEAR, /* cold */
    GWEATHER_SKY_OVERCAST, /* cloudy */
    GWEATHER_SKY_FEW, /* mostly cloudy (night) */
    GWEATHER_SKY_FEW, /* mostly cloudy (day) */
    GWEATHER_SKY_BROKEN, /* partly cloudy (night) */
    GWEATHER_SKY_BROKEN, /* partly cloudy (day) */
    GWEATHER_SKY_CLEAR, /* clear (night) */
    GWEATHER_SKY_CLEAR, /* sunny */
    GWEATHER_SKY_CLEAR, /* fair (night) */
    GWEATHER_SKY_CLEAR, /* fair (day) */
    GWEATHER_SKY_SCATTERED, /* mixed rain and hail */
    GWEATHER_SKY_CLEAR, /* hot */
    GWEATHER_SKY_SCATTERED, /* isolated thunderstorms */
    GWEATHER_SKY_SCATTERED, /* scattered thunderstorms */
    GWEATHER_SKY_SCATTERED, /* scattered thunderstorms */
    GWEATHER_SKY_SCATTERED, /* scattered showers */
    GWEATHER_SKY_SCATTERED, /* heavy snow */
    GWEATHER_SKY_SCATTERED, /* scattered snow showers */
    GWEATHER_SKY_SCATTERED, /* heavy snow */
    GWEATHER_SKY_BROKEN, /* partly cloudy */
    GWEATHER_SKY_SCATTERED, /* thundershowers */
    GWEATHER_SKY_SCATTERED, /* snow showers */
    GWEATHER_SKY_SCATTERED, /* isolated thundershowers */
};

G_STATIC_ASSERT (G_N_ELEMENTS(condition_codes) == G_N_ELEMENTS(sky_codes));

static time_t
date_to_time_t (const xmlChar *str)
{
    struct tm time = { 0 };

    if (!strptime ((const char*) str, "%d %b %Y", &time)) {
	g_warning ("Cannot parse date string \"%s\"", str);
	return 0;
    }

    return mktime(&time);
}

static GWeatherInfo *
make_info_from_node (GWeatherInfo *master_info,
		     xmlNodePtr    node)
{
    GWeatherInfo *info;
    GWeatherInfoPrivate *priv;
    xmlChar *val;
    int code;

    g_return_val_if_fail (node->type == XML_ELEMENT_NODE, NULL);

    info = _gweather_info_new_clone (master_info);
    priv = info->priv;

    val = xmlGetProp (node, XC("date"));
    priv->current_time = priv->update = date_to_time_t (val);
    xmlFree (val);

    val = xmlGetProp (node, XC("high"));
    priv->temp_max = g_ascii_strtod ((const char*) val, NULL);
    xmlFree (val);

    val = xmlGetProp (node, XC("low"));
    priv->temp_min = g_ascii_strtod ((const char*) val, NULL);
    xmlFree (val);

    priv->tempMinMaxValid = priv->tempMinMaxValid || (priv->temp_max > -999.0 && priv->temp_min > -999.0);
    priv->valid = priv->tempMinMaxValid;

    val = xmlGetProp (node, XC("text"));
    priv->forecast = g_strdup ((const char*) val);
    xmlFree (val);

    val = xmlGetProp (node, XC("code"));
    code = strtol((const char*) val, NULL, 0);
    if (code >= 0 && code < G_N_ELEMENTS (condition_codes)) {
	priv->cond = condition_codes[code];
	priv->sky = sky_codes[code];
    } else
	priv->valid = FALSE;
    xmlFree (val);

    return info;
}

static void
parse_forecast_xml (GWeatherInfo    *master_info,
		    SoupMessageBody *body)
{
    GWeatherInfoPrivate *priv;
    xmlDocPtr doc;
    xmlXPathContextPtr xpath_ctx;
    xmlXPathObjectPtr xpath_result;
    int i;

    priv = master_info->priv;

    doc = xmlParseMemory (body->data, body->length);
    if (!doc)
	return;

    xpath_ctx = xmlXPathNewContext (doc);
    xmlXPathRegisterNs (xpath_ctx, XC("yweather"), XC("http://xml.weather.yahoo.com/ns/rss/1.0"));
    xpath_result = xmlXPathEval (XC("/rss/channel/item/yweather:forecast"), xpath_ctx);

    if (!xpath_result || xpath_result->type != XPATH_NODESET)
	goto out;

    for (i = 0; i < xpath_result->nodesetval->nodeNr; i++) {
	xmlNodePtr node;
	GWeatherInfo *info;

	node = xpath_result->nodesetval->nodeTab[i];
	info = make_info_from_node (master_info, node);

	priv->forecast_list = g_slist_append (priv->forecast_list, info);
    }

    xmlXPathFreeObject (xpath_result);

 out:
    xmlXPathFreeContext (xpath_ctx);
    xmlFreeDoc (doc);
}

static void
yahoo_finish (SoupSession *session,
	      SoupMessage *msg,
	      gpointer     user_data)
{
    GWeatherInfo *info = GWEATHER_INFO (user_data);

    if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
	/* forecast data is not really interesting anyway ;) */
	g_warning ("Failed to get Yahoo! Weather forecast data: %d %s\n",
		   msg->status_code, msg->reason_phrase);
	_gweather_info_request_done (info);
	return;
    }

    parse_forecast_xml (info, msg->response_body);
    _gweather_info_request_done (info);
}

gboolean
yahoo_start_open (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    WeatherLocation *loc;
    gchar *url;
    SoupMessage *message;

    priv = info->priv;
    loc = &priv->location;

    if (!loc->yahoo_id)
	return FALSE;

    /* Yahoo! Weather only supports forecast list
       (and really, the other types only make sense with national
       weather offices that cannot return structured data)
    */
    if (!priv->forecast_type != GWEATHER_FORECAST_LIST)
	return FALSE;

    /* u=f means that the values are in imperial system (which is what
       weather.c expects). They're converted to user preferences before
       displaying.
    */
    url = g_strdup_printf("http://weather.yahooapis.com/forecastrss?w=%s&u=f", loc->yahoo_id);

    message = soup_message_new ("GET", url);
    soup_session_queue_message (priv->session, message, yahoo_finish, info);

    priv->requests_pending++;

    g_free (url);

    return TRUE;
}
