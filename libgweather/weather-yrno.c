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

/* Reference for symbols at http://om.yr.no/forklaring/symbol/ */
static struct {
    GWeatherSky sky;
    GWeatherConditions condition;
} symbols[] = {
    { GWEATHER_SKY_CLEAR,     { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Sun / clear sky */
    { GWEATHER_SKY_BROKEN,    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Fair */
    { GWEATHER_SKY_SCATTERED, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Partly cloudy */
    { GWEATHER_SKY_OVERCAST,  { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Cloudy */
    { GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_SHOWERS } }, /* Rain showers */
    { GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Rain showers with thunder */
    { GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_SHOWERS } }, /* Sleet showers */
    { GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_SHOWERS } }, /* Snow showers */
    { GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_NONE } }, /* Rain */
    { GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_HEAVY } }, /* Heavy rain */
    { GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Rain and thunder */
    { GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_NONE } }, /* Sleet */
    { GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_NONE } }, /* Snow */
    { GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Snow and thunder */
    { GWEATHER_SKY_CLEAR,     { TRUE, GWEATHER_PHENOMENON_FOG, GWEATHER_QUALIFIER_NONE } }, /* Fog */
    { GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Sleet showers and thunder */
    { GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Snow showers and thunder */
    { GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_HEAVY } }, /* Rain and thunder */
    { GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_HEAVY } } /* Sleet and thunder */
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
    char *after;

    after = strptime ((const char*) str, "%Y-%m-%dT%T", &time);
    if (after == NULL) {
	g_warning ("Cannot parse date string \"%s\"", str);
	return 0;
    }

    if (*after == 'Z')
	tzid = "UTC";

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

    symbol = strtol ((char*) val, NULL, 0) - 1;
    if (symbol >= 0 && symbol < G_N_ELEMENTS (symbols)) {
	priv->valid = TRUE;
	priv->sky = symbols[symbol].sky;
	priv->cond = symbols[symbol].condition;
    }
}

static inline void
read_wind_direction (GWeatherInfo *info,
		     xmlNodePtr    node)
{
    xmlChar *val;
    int i;

    val = xmlGetProp (node, XC("code"));
    if (val == NULL)
	val = xmlGetProp (node, XC("name"));
    if (val == NULL)
	return;

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
    if (val == NULL)
	return;

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
    if (val == NULL)
	return;

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
    if (val == NULL)
	return;

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

static inline void
fill_info_from_node (GWeatherInfo *info,
		     xmlNodePtr    node)
{
    xmlNodePtr child;

    for (child = node->children; child != NULL; child = child->next) {
	if (child->type == XML_ELEMENT_NODE)
	    read_child_node (info, child);
    }
}

static GWeatherInfo *
make_info_from_node_old (GWeatherInfo *master_info,
			 xmlNodePtr    node)
{
    GWeatherInfo *info;
    GWeatherInfoPrivate *priv;
    xmlChar *val;

    g_return_val_if_fail (node->type == XML_ELEMENT_NODE, NULL);

    info = _gweather_info_new_clone (master_info);
    priv = info->priv;

    val = xmlGetProp (node, XC("from"));
    priv->current_time = priv->update = date_to_time_t (val, info->priv->location.tz_hint);
    xmlFree (val);

    fill_info_from_node (info, node);

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
parse_forecast_xml_old (GWeatherInfo    *master_info,
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
	info = make_info_from_node_old (master_info, node);

	priv->forecast_list = g_slist_append (priv->forecast_list, info);
    }

    xmlXPathFreeObject (xpath_result);

    xpath_result = xmlXPathEval (XC("/weatherdata/credit/link"), xpath_ctx);
    if (!xpath_result || xpath_result->type != XPATH_NODESET)
	goto out;

    priv->forecast_attribution = make_attribution_from_node (xpath_result->nodesetval->nodeTab[0]);

 out:
    if (xpath_result)
	xmlXPathFreeObject (xpath_result);
    xmlXPathFreeContext (xpath_ctx);
    xmlFreeDoc (doc);
}



static void
parse_forecast_xml_new (GWeatherInfo    *master_info,
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
    xpath_result = xmlXPathEval (XC("/weatherdata/product/time"), xpath_ctx);

    if (!xpath_result || xpath_result->type != XPATH_NODESET)
	goto out;

    for (i = 0; i < xpath_result->nodesetval->nodeNr; i++) {
	xmlNodePtr node;
	GWeatherInfo *info;
	xmlChar *val;
	time_t from_time, to_time;
	xmlNode *location;

	node = xpath_result->nodesetval->nodeTab[i];

	val = xmlGetProp (node, XC("from"));
	from_time = date_to_time_t (val, priv->location.tz_hint);
	xmlFree (val);

	val = xmlGetProp (node, XC("to"));
	to_time = date_to_time_t (val, priv->location.tz_hint);
	xmlFree (val);

	/* New API has forecast in a list of "master" elements
	   with details (indicated by from==to) and "slave" elements
	   that hold only precipitation and symbol. For our purpose,
	   the master element is enough, except that we actually
	   want that symbol. So pick the symbol from the next element.
	   Additionally, compared to the old API the new API has one
	   <location> element inside each <time> element.
	*/
	if (from_time == to_time) {
	    info = _gweather_info_new_clone (master_info);
	    info->priv->current_time = info->priv->update = from_time;

	    for (location = node->children;
		 location && location->type != XML_ELEMENT_NODE;
		 location = location->next);
	    if (location)
		fill_info_from_node (info, location);

	    if (i < xpath_result->nodesetval->nodeNr - 1) {
		i++;
		node = xpath_result->nodesetval->nodeTab[i];

		for (location = node->children;
		     location && location->type != XML_ELEMENT_NODE;
		     location = location->next);
		if (location)
		    fill_info_from_node (info, location);
	    }

	    priv->forecast_list = g_slist_append (priv->forecast_list, info);
	}
    }

    xmlXPathFreeObject (xpath_result);

    /* The new (documented but not advertised) API is less strict in the
       format of the attribution, and just requires a generic CC-BY compatible
       attribution with a link to their service.

       That's very nice of them!
    */
    priv->forecast_attribution = g_strdup(_("Weather data from the <a href=\"http://yr.no/\">Norwegian Meteorological Institute</a>"));

 out:
    xmlXPathFreeContext (xpath_ctx);
    xmlFreeDoc (doc);
}

static void
yrno_finish_old (SoupSession *session,
		 SoupMessage *msg,
		 gpointer     user_data)
{
    GWeatherInfo *info = GWEATHER_INFO (user_data);

    if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
	/* forecast data is not really interesting anyway ;) */
	g_message ("Failed to get Yr.no forecast data: %d %s\n",
		   msg->status_code, msg->reason_phrase);
	_gweather_info_request_done (info);
	return;
    }

    parse_forecast_xml_old (info, msg->response_body);
    _gweather_info_request_done (info);
}

static gboolean
yrno_start_open_old (GWeatherInfo *info)
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
    soup_session_queue_message (priv->session, message, yrno_finish_old, info);

    priv->requests_pending++;

    g_free (url);

    return TRUE;
}

static void
yrno_finish_new (SoupSession *session,
		 SoupMessage *msg,
		 gpointer     user_data)
{
    GWeatherInfo *info = GWEATHER_INFO (user_data);

    if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
	/* forecast data is not really interesting anyway ;) */
	g_message ("Failed to get Yr.no forecast data: %d %s\n",
		   msg->status_code, msg->reason_phrase);
	_gweather_info_request_done (info);
	return;
    }

    parse_forecast_xml_new (info, msg->response_body);

    _gweather_info_request_done (info);
}

static gboolean
yrno_start_open_new (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    gchar *url;
    SoupMessage *message;
    WeatherLocation *loc;
    gchar latstr[G_ASCII_DTOSTR_BUF_SIZE], lonstr[G_ASCII_DTOSTR_BUF_SIZE];

    priv = info->priv;
    loc = &priv->location;

    if (!loc->latlon_valid ||
	priv->forecast_type != GWEATHER_FORECAST_LIST)
	return FALSE;

    /* see the description here: http://api.yr.no/weatherapi/ */

    g_ascii_dtostr (latstr, sizeof(latstr), RADIANS_TO_DEGREES (loc->latitude));
    g_ascii_dtostr (lonstr, sizeof(lonstr), RADIANS_TO_DEGREES (loc->longitude));

    url = g_strdup_printf("http://api.yr.no/weatherapi/locationforecast/1.8/?lat=%s;lon=%s", latstr, lonstr);

    message = soup_message_new ("GET", url);
    soup_session_queue_message (priv->session, message, yrno_finish_new, info);

    priv->requests_pending++;

    g_free (url);

    return TRUE;
}

gboolean
yrno_start_open (GWeatherInfo *info)
{
    if (yrno_start_open_new (info))
	return TRUE;

    return yrno_start_open_old (info);
}
