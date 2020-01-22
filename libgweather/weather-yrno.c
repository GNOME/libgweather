/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* weather-yrno.c - MET Norway Weather service.
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

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <glib.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "gweather-private.h"

#define XC(t) ((const xmlChar *)(t))

/* Reference for symbols at http://om.yr.no/forklaring/symbol/ */
typedef struct {
    int code;
    GWeatherSky sky;
    GWeatherConditions condition;
} YrnoSymbol;

static YrnoSymbol symbols[] = {
    { 1,  GWEATHER_SKY_CLEAR,     { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Sun */
    { 2,  GWEATHER_SKY_BROKEN,    { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* LightCloud */
    { 3,  GWEATHER_SKY_SCATTERED, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* PartlyCloudy */
    { 4,  GWEATHER_SKY_OVERCAST,  { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Cloudy */
    { 5,  GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_LIGHT } }, /* LightRainSun */
    { 6,  GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* LightRainThunderSun */
    { 7,  GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_NONE } }, /* SleetSun */
    { 8,  GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_NONE } }, /* SnowSun */
    { 9,  GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_LIGHT } }, /* SnowSun */
    { 10, GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_NONE } }, /* Rain */
    { 11, GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* RainThunder */
    { 12, GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_NONE } }, /* Sleet */
    { 13, GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_NONE } }, /* Snow */
    { 14, GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* SnowThunder */
    { 15, GWEATHER_SKY_CLEAR,     { TRUE, GWEATHER_PHENOMENON_FOG, GWEATHER_QUALIFIER_NONE } }, /* Fog */
    { 20, GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* SleetSunThunder */
    { 21, GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* SnowSunThunder */
    { 22, GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* LightRainThunder */
    { 23, GWEATHER_SKY_OVERCAST,  { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* SleetThunder */
    { 24, GWEATHER_SKY_BROKEN,  { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* DrizzleThunderSun */
    { 25, GWEATHER_SKY_BROKEN,  { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* RainThunderSun */
    { 26, GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_LIGHT } }, /* LightSleetThunderSun */
    { 27, GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_HEAVY } }, /* HeavySleetThunderSun */
    { 28, GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_LIGHT } }, /* LightSnowThunderSun */
    { 29, GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_HEAVY } }, /* HeavySnowThunderSun */
    { 30, GWEATHER_SKY_OVERCAST,    { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* DrizzleThunder */
    { 31, GWEATHER_SKY_OVERCAST,    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_LIGHT } }, /* LightSleetThunder */
    { 32, GWEATHER_SKY_OVERCAST,    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_HEAVY } }, /* HeavySleetThunder */
    { 33, GWEATHER_SKY_OVERCAST,    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_LIGHT } }, /* LightSnowThunder */
    { 34, GWEATHER_SKY_OVERCAST,    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_HEAVY } }, /* HeavySnowThunder */
    { 40, GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_NONE } }, /* DrizzleSun */
    { 41, GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_NONE } }, /* RainSun */
    { 42, GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_LIGHT } }, /* LightSleetSun */
    { 43, GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_HEAVY } }, /* HeavySleetSun */
    { 44, GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_LIGHT } }, /* LightSnowSun */
    { 45, GWEATHER_SKY_BROKEN,    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_HEAVY } }, /* HeavySnowSun */
    { 46, GWEATHER_SKY_OVERCAST,    { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_NONE } }, /* Drizzle */
    { 47, GWEATHER_SKY_OVERCAST,    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_LIGHT } }, /* LightSleet */
    { 48, GWEATHER_SKY_OVERCAST,    { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_HEAVY } }, /* HeavySleet */
    { 49, GWEATHER_SKY_OVERCAST,    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_LIGHT } }, /* LightSnow */
    { 50, GWEATHER_SKY_OVERCAST,    { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_HEAVY } } /* HeavySnow */
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

static YrnoSymbol *
symbol_search (int code)
{
    int a = 0;
    int b = G_N_ELEMENTS (symbols);

    while (a < b) {
	int c = (a + b)/2;
	YrnoSymbol *yc = symbols + c;

	if (yc->code == code)
	    return yc;
	if (yc->code < code)
	    a = c+1;
	else
	    b = c;
    }

    return NULL;
}

static inline void
read_symbol (GWeatherInfo *info,
	     xmlNodePtr    node)
{
    xmlChar *val;
    YrnoSymbol* symbol;
    GWeatherInfoPrivate *priv = info->priv;

    val = xmlGetProp (node, XC("number"));

    symbol = symbol_search (strtol ((char*) val, NULL, 0));
    if (symbol != NULL) {
	priv->valid = TRUE;
	priv->sky = symbol->sky;
	priv->cond = symbol->condition;
    }
    xmlFree (val);
}

static inline void
read_wind_direction (GWeatherInfo *info,
		     xmlNodePtr    node)
{
    xmlChar *val;
    unsigned int i;

    val = xmlGetProp (node, XC("code"));
    if (val == NULL)
	val = xmlGetProp (node, XC("name"));
    if (val == NULL)
	return;

    for (i = 0; i < G_N_ELEMENTS (wind_directions); i++) {
	if (strcmp ((char*) val, wind_directions[i].name) == 0) {
	    info->priv->wind = wind_directions[i].direction;
            xmlFree (val);
	    return;
	}
    }
    xmlFree (val);
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
    xmlFree (val);
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
    xmlFree (val);
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
    xmlFree (val);
}

static inline void
read_humidity (GWeatherInfo *info,
	       xmlNodePtr    node)
{
    xmlChar *val;
    double percent;

    val = xmlGetProp (node, XC("value"));
    if (val == NULL)
	return;

    percent = g_ascii_strtod ((char*) val, NULL);
    info->priv->humidity = percent;
    info->priv->hasHumidity = TRUE;
    xmlFree (val);
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
    else if (strcmp ((char*) node->name, "humidity") == 0)
	read_humidity (info, node);
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
    priv->forecast_attribution = g_strdup(_("Weather data from the <a href=\"https://www.met.no/\">Norwegian Meteorological Institute</a>"));

 out:
    xmlXPathFreeContext (xpath_ctx);
    xmlFreeDoc (doc);
}

static void
yrno_finish_new (SoupSession *session,
		 SoupMessage *msg,
		 gpointer     user_data)
{
    GWeatherInfo *info;
    GWeatherInfoPrivate *priv;
    WeatherLocation *loc;
    guint num_forecasts;

    if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
	/* forecast data is not really interesting anyway ;) */
	if (msg->status_code == SOUP_STATUS_CANCELLED) {
	    g_debug ("Failed to get Yr.no forecast data: %d %s\n",
		     msg->status_code, msg->reason_phrase);
	    return;
	}
	g_message ("Failed to get Yr.no forecast data: %d %s\n",
		   msg->status_code, msg->reason_phrase);
	_gweather_info_request_done (user_data, msg);
	return;
    }

    info = user_data;
    priv = info->priv;
    loc = &priv->location;
    g_debug ("yrno data for %lf, %lf", loc->latitude, loc->longitude);
    g_debug ("%s", msg->response_body->data);

    parse_forecast_xml_new (info, msg->response_body);
    num_forecasts = g_slist_length (priv->forecast_list);
    g_debug ("yrno parsed %d forecast infos", num_forecasts);
    if (!priv->valid)
        priv->valid = (num_forecasts > 0);

    _gweather_info_request_done (info, msg);
}

gboolean
yrno_start_open (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    gchar *url;
    SoupMessage *message;
    WeatherLocation *loc;
    gchar latstr[G_ASCII_DTOSTR_BUF_SIZE], lonstr[G_ASCII_DTOSTR_BUF_SIZE];

    priv = info->priv;
    loc = &priv->location;

    if (!loc->latlon_valid)
	return FALSE;

    /* see the description here: https://api.met.no/ */

    g_ascii_dtostr (latstr, sizeof(latstr), RADIANS_TO_DEGREES (loc->latitude));
    g_ascii_dtostr (lonstr, sizeof(lonstr), RADIANS_TO_DEGREES (loc->longitude));

    url = g_strdup_printf("https://api.met.no/weatherapi/locationforecast/1.9/?lat=%s;lon=%s", latstr, lonstr);
    g_debug ("yrno_start_open, requesting: %s", url);

    message = soup_message_new ("GET", url);
    _gweather_info_begin_request (info, message);
    soup_session_queue_message (priv->session, message, yrno_finish_new, info);

    g_free (url);

    return TRUE;
}

