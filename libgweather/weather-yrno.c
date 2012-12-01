/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* weather-yrno.c - Yr.no Weather service.
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

static struct {
    GWeatherSky sky;
    GWeatherConditionPhenomenon phenomenon;
    GWeatherConditionQualifier qualifier;
} symbols[] = {
    { GWEATHER_SKY_CLEAR, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE },
    { GWEATHER_SKY_BROKEN, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE },
    { GWEATHER_SKY_SCATTERED, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE },
    { GWEATHER_SKY_OVERCAST, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE },
    { GWEATHER_SKY_BROKEN, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_SHOWERS },
    { GWEATHER_SKY_BROKEN, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM },
    { GWEATHER_SKY_BROKEN, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_SHOWERS },
    { GWEATHER_SKY_OVERCAST, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_SHOWERS },
    { GWEATHER_SKY_OVERCAST, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_NONE },
    { GWEATHER_SKY_OVERCAST, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_HEAVY },
    { GWEATHER_SKY_OVERCAST, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM },
    { GWEATHER_SKY_OVERCAST, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_NONE },
    { GWEATHER_SKY_OVERCAST, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_NONE },
    { GWEATHER_SKY_OVERCAST, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_THUNDERSTORM },
    { GWEATHER_SKY_CLEAR, GWEATHER_PHENOMENON_FOG, GWEATHER_QUALIFIER_NONE },
    { GWEATHER_SKY_BROKEN, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_THUNDERSTORM },
    { GWEATHER_SKY_OVERCAST, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_HEAVY },
    { GWEATHER_SKY_OVERCAST, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_HEAVY }
};

static struct {
    const char *name;
    GWeatherWindDirection direction;
} wind_directions[] = {
    { "N", GWEATHER_WIND_N },
    { "NNE", GWEATHER_WIND_NNE },
    { "NE", GWEATHER_WIND_NE },
    { "ENE", GWEATHER_WIND_ENE },
    { "E", GWEATHER_WIND_E },
    { "ESE", GWEATHER_WIND_ESE },
    { "SE", GWEATHER_WIND_SE },
    { "SSE", GWEATHER_WIND_SSE },
    { "S", GWEATHER_WIND_S },
    { "SSW", GWEATHER_WIND_SSW },
    { "SW", GWEATHER_WIND_SW },
    { "WSW", GWEATHER_WIND_WSW },
    { "W", GWEATHER_WIND_W },
    { "WNW", GWEATHER_WIND_WNW },
    { "NW", GWEATHER_WIND_NW },
    { "NNW", GWEATHER_WIND_NNW },
};

static time_t
date_to_time_t (const xmlChar *str, const char * tzid)
{
    struct tm time = { 0 };
    GTimeZone *tz;
    GDateTime *dt;
    time_t rval;

    if (!strptime ((const char*) str, "%Y-%m-%dT%T", &time)) {
	g_warning ("Cannot parse date string \"%s\"", str);
	return 0;
    }

    tz = g_time_zone_new (tzid);
    dt = g_date_time_new (tz,
			  time.tm_year + 1900,
			  time.tm_mon + 1,
			  time.tm_mday,
			  time.tm_hour,
			  time.tm_min,
			  time.tm_sec);

    rval = g_date_time_to_unix (dt);

    g_time_zone_unref (tz);
    g_date_time_unref (dt);

    return rval;
}

static inline void
read_symbol (GWeatherInfo *info,
	     xmlNodePtr    node)
{
    xmlChar *val;
    int symbol;
    GWeatherInfoPrivate *priv = info->priv;

    val = xmlGetProp (node, XC("number"));

    symbol = strtol ((char*) val, NULL, 0);
    if (symbol >= 0 && symbol < G_N_ELEMENTS (symbols)) {
	priv->valid = TRUE;
	priv->sky = symbols[symbol].sky;
	priv->cond.phenomenon = symbols[symbol].phenomenon;
	priv->cond.qualifier = symbols[symbol].qualifier;
    }
}

static inline void
read_wind_direction (GWeatherInfo *info,
		     xmlNodePtr    node)
{
    xmlChar *val;
    int i;

    val = xmlGetProp (node, XC("code"));

    for (i = 0; i < G_N_ELEMENTS (wind_directions); i++) {
	if (strcmp ((char*) val, wind_directions[i].name) == 0) {
	    info->priv->wind = wind_directions[i].direction;
	    return;
	}
    }
}

static inline void
read_wind_speed (GWeatherInfo *info,
		 xmlNodePtr    node)
{
    xmlChar *val;
    double mps;

    val = xmlGetProp (node, XC("mps"));

    mps = g_ascii_strtod ((char*) val, NULL);
    info->priv->windspeed = WINDSPEED_MS_TO_KNOTS (mps);
}

static inline void
read_temperature (GWeatherInfo *info,
		  xmlNodePtr    node)
{
    xmlChar *val;
    double celsius;

    val = xmlGetProp (node, XC("value"));

    celsius = g_ascii_strtod ((char*) val, NULL);
    info->priv->temp = TEMP_C_TO_F (celsius);
}

static inline void
read_pressure (GWeatherInfo *info,
	       xmlNodePtr    node)
{
    xmlChar *val;
    double hpa;

    val = xmlGetProp (node, XC("value"));

    hpa = g_ascii_strtod ((char*) val, NULL);
    info->priv->pressure = PRESSURE_MBAR_TO_INCH (hpa);
}

static inline void
read_child_node (GWeatherInfo *info,
		 xmlNodePtr    node)
{
    if (strcmp ((char*) node->name, "symbol") == 0)
	read_symbol (info, node);
    else if (strcmp ((char*) node->name, "windDirection") == 0)
	read_wind_direction (info, node);
    else if (strcmp ((char*) node->name, "windSpeed") == 0)
	read_wind_speed (info, node);
    else if (strcmp ((char*) node->name, "temperature") == 0)
	read_temperature (info, node);
    else if (strcmp ((char*) node->name, "pressure") == 0)
	read_pressure (info, node);
}

static GWeatherInfo *
make_info_from_node (GWeatherInfo *master_info,
		     xmlNodePtr    node)
{
    GWeatherInfo *info;
    GWeatherInfoPrivate *priv;
    xmlChar *val;
    xmlNodePtr child;

    g_return_val_if_fail (node->type == XML_ELEMENT_NODE, NULL);

    info = _gweather_info_new_clone (master_info);
    priv = info->priv;

    val = xmlGetProp (node, XC("from"));
    priv->update = date_to_time_t (val, info->priv->location->tz_hint);
    xmlFree (val);

    for (child = node->children; child != NULL; child = child->next) {
	if (child->type == XML_ELEMENT_NODE)
	    read_child_node (info, child);
    }

    return info;
}

static char *
make_attribution_from_node (xmlNodePtr node)
{
    xmlChar *url;
    xmlChar *text;
    char *res;

    url = xmlGetProp (node, XC("url"));
    text = xmlGetProp (node, XC("text"));

    /* Small hack to avoid linking the entire label, and to have
       This is still compliant with the guidelines, as far as I
       understand it.
       The label is a legal attribution and cannot be translated.
    */
    if (strcmp ((char*) text,
		"Weather forecast from yr.no, delivered by the"
		" Norwegian Meteorological Institute and the NRK") == 0)
	res = g_strdup_printf ("Weather forecast from yr.no, delivered by"
			       " the <a href=\"%s\">Norwegian Meteorological"
			       " Institude and the NRK</a>", url);
    else
	res = g_strdup_printf ("<a href=\"%s\">%s</a>", url, text);

    xmlFree (url);
    xmlFree (text);

    return res;
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
    xpath_result = xmlXPathEval (XC("/weatherdata/forecast/tabular/time"), xpath_ctx);

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

    xpath_result = xmlXPathEval (XC("/weatherdata/credit/link"), xpath_ctx);
    if (!xpath_result || xpath_result->type != XPATH_NODESET)
	goto out;

    priv->forecast_attribution = make_attribution_from_node (xpath_result->nodesetval->nodeTab[0]);

 out:
    xmlXPathFreeContext (xpath_ctx);
    xmlFreeDoc (doc);
}

static void
yrno_finish (SoupSession *session,
	      SoupMessage *msg,
	      gpointer     user_data)
{
    GWeatherInfo *info = GWEATHER_INFO (user_data);

    if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
	/* forecast data is not really interesting anyway ;) */
	g_message ("Failed to get Yr.no forecast data: %d %s\n",
		   msg->status_code, msg->reason_phrase);
	request_done (info, FALSE);
	return;
    }

    parse_forecast_xml (info, msg->response_body);

    request_done (info, TRUE);
}

gboolean
yrno_start_open (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    gchar *url;
    SoupMessage *message;
    const char *country = NULL;
    const char *adm_division = NULL;
    char *city_name = NULL;
    GWeatherLocation *glocation;

    priv = info->priv;

    if (priv->forecast_type != GWEATHER_FORECAST_LIST)
	return FALSE;

    glocation = priv->glocation;
    while (glocation) {
	if (glocation->level == GWEATHER_LOCATION_CITY)
	    city_name = glocation->name;
	if (glocation->level == GWEATHER_LOCATION_ADM1 ||
	    glocation->level == GWEATHER_LOCATION_ADM2)
	    adm_division = glocation->name;
	if (glocation->level == GWEATHER_LOCATION_COUNTRY)
	    country = glocation->name;
	glocation = glocation->parent;
    }

    if (city_name == NULL || adm_division == NULL || country == NULL)
	return FALSE;

    url = g_strdup_printf("http://yr.no/place/%s/%s/%s/forecast.xml", country, adm_division, city_name);

    message = soup_message_new ("GET", url);
    soup_session_queue_message (priv->session, message, yrno_finish, info);

    priv->requests_pending++;

    g_free (url);

    return TRUE;
}
