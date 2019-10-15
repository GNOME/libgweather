/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* weather-owm.c - Open Weather Map backend
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <langinfo.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "gweather-private.h"

#define XC(t) ((const xmlChar *)(t))

/* Reference for symbols at http://bugs.openweathermap.org/projects/api/wiki/Weather_Condition_Codes */
/* FIXME: the libgweather API is not expressive enough */
static struct owm_symbol {
    int symbol;
    GWeatherSky sky;
    GWeatherConditions condition;
} symbols[] = {
    { 200, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Thunderstorm with light rain */
    { 201, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Thunderstorm with rain */
    { 202, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Thunderstorm with heavy rain */
    { 210, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Light thunderstorm */
    { 211, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Thunderstorm */
    { 212, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Heavy thunderstorm */
    { 221, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Ragged thunderstorm */
    { 230, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Thunderstorm with light drizzle */
    { 231, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Thunderstorm with drizzle */
    { 232, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Thunderstorm with heavy drizzle */

    { 300, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_LIGHT } }, /* Light intensity drizzle */
    { 301, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_NONE } }, /* Drizzle */
    { 302, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_HEAVY } }, /* Heavy intensity drizzle */
    { 310, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_LIGHT } }, /* Light intensity drizzle rain */
    { 311, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_NONE } }, /* Drizzle rain */
    { 312, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_HEAVY } }, /* Heavy intensity drizzle rain */
    { 321, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_SHOWERS } }, /* Drizzle showers */

    { 500, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_LIGHT } }, /* Light rain */
    { 501, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_MODERATE } }, /* Moderate rain */
    { 502, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_HEAVY } }, /* Heavy intensity rain */
    { 503, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_HEAVY } }, /* Very heavy rain */
    { 504, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_HEAVY } }, /* Extreme rain */
    { 511, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_FREEZING } }, /* Freezing rain */
    { 520, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_SHOWERS } }, /* Light intensity shower rain */
    { 521, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_SHOWERS } }, /* Shower rain */
    { 522, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_SHOWERS } }, /* Heavy intensity shower rain */

    { 600, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_LIGHT } }, /* Light snow */
    { 601, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_NONE } }, /* Snow */
    { 602, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_HEAVY } }, /* Heavy snow */
    { 611, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_NONE } }, /* Sleet */
    { 621, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_SHOWERS } }, /* Shower snow */

    { 701, GWEATHER_SKY_CLEAR, { TRUE, GWEATHER_PHENOMENON_MIST, GWEATHER_QUALIFIER_NONE } }, /* Mist */
    { 711, GWEATHER_SKY_CLEAR, { TRUE, GWEATHER_PHENOMENON_SMOKE, GWEATHER_QUALIFIER_NONE } }, /* Smoke */
    { 721, GWEATHER_SKY_CLEAR, { TRUE, GWEATHER_PHENOMENON_HAZE, GWEATHER_QUALIFIER_NONE } }, /* Haze */
    { 731, GWEATHER_SKY_CLEAR, { TRUE, GWEATHER_PHENOMENON_DUST_WHIRLS, GWEATHER_QUALIFIER_NONE } }, /* Dust/sand whirls */
    { 741, GWEATHER_SKY_CLEAR, { TRUE, GWEATHER_PHENOMENON_FOG, GWEATHER_QUALIFIER_NONE } }, /* Fog */

    { 800, GWEATHER_SKY_CLEAR, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Clear sky */
    { 801, GWEATHER_SKY_FEW, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Few clouds */
    { 802, GWEATHER_SKY_SCATTERED, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Scattered clouds */
    { 803, GWEATHER_SKY_BROKEN, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Broken clouds */
    { 804, GWEATHER_SKY_OVERCAST, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Overcast clouds */

    /* XXX: all these are a bit iffy */
    { 900, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_TORNADO, GWEATHER_QUALIFIER_NONE } }, /* Tornado */
    { 901, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_TORNADO, GWEATHER_QUALIFIER_NONE } }, /* Tropical storm */
    { 902, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_TORNADO, GWEATHER_QUALIFIER_NONE } }, /* Hurricane */
    { 903, GWEATHER_SKY_CLEAR, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Cold */
    { 904, GWEATHER_SKY_CLEAR, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Hot */
    { 905, GWEATHER_SKY_CLEAR, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Windy */
    { 906, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_HAIL, GWEATHER_QUALIFIER_NONE } }, /* Hail */
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

static int
symbol_compare (const void *key,
                const void *element)
{
    const struct owm_symbol *s_key = key;
    const struct owm_symbol *s_element = element;

    return s_key->symbol - s_element->symbol;
}

static inline void
read_symbol (GWeatherInfo *info,
	     xmlNodePtr    node)
{
    xmlChar *val;
    GWeatherInfoPrivate *priv = info->priv;
    struct owm_symbol *obj, ref;

    val = xmlGetProp (node, XC("number"));

    ref.symbol = strtol ((char*) val, NULL, 0) - 1;
    obj = bsearch (&ref, symbols, G_N_ELEMENTS (symbols),
                   sizeof (struct owm_symbol), symbol_compare);

    if (obj == NULL) {
        g_warning ("Unknown symbol %d returned from OpenWeatherMap",
                   ref.symbol);
        return;
    }

    priv->valid = TRUE;
    priv->sky = obj->sky;
    priv->cond = obj->condition;
}

static inline void
read_wind_direction (GWeatherInfo *info,
		     xmlNodePtr    node)
{
    xmlChar *val;
    unsigned int i;

    val = xmlGetProp (node, XC("code"));
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
    xmlChar *unit;
    xmlChar *val;
    double celsius;

    unit = xmlGetProp (node, XC("unit"));
    if (unit == NULL || strcmp ((char*)unit, "celsius"))
        return;

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
    xmlChar *unit;
    xmlChar *val;
    double hpa;

    /* hPa == mbar */
    unit = xmlGetProp (node, XC("unit"));
    if (unit == NULL || strcmp ((char*)unit, "hPa"))
        return;

    val = xmlGetProp (node, XC("value"));
    if (val == NULL)
	return;

    hpa = g_ascii_strtod ((char*) val, NULL);
    info->priv->pressure = PRESSURE_MBAR_TO_INCH (hpa);
}

static inline void
read_humidity (GWeatherInfo *info,
               xmlNodePtr    node)
{
    xmlChar *unit;
    xmlChar *val;
    double percent;

    unit = xmlGetProp (node, XC("unit"));
    if (unit == NULL || strcmp ((char*)unit, "%"))
        return;

    val = xmlGetProp (node, XC("value"));
    if (val == NULL)
	return;

    percent = g_ascii_strtod ((char*) val, NULL);
    info->priv->humidity = percent;
    info->priv->hasHumidity = TRUE;
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

static GWeatherInfo *
make_info_from_node (GWeatherInfo *master_info,
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
    xpath_result = xmlXPathEval (XC("/weatherdata/forecast/time"), xpath_ctx);

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

    priv->forecast_attribution = g_strdup(_("Weather data from the <a href=\"https://openweathermap.org\">Open Weather Map project</a>"));

 out:
    if (xpath_result)
	xmlXPathFreeObject (xpath_result);
    xmlXPathFreeContext (xpath_ctx);
    xmlFreeDoc (doc);
}

static void
owm_finish (SoupSession *session,
            SoupMessage *msg,
            gpointer     user_data)
{
    GWeatherInfo *info;
    GWeatherInfoPrivate *priv;
    WeatherLocation *loc;

    if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
	/* forecast data is not really interesting anyway ;) */
	if (msg->status_code == SOUP_STATUS_CANCELLED) {
	    g_debug ("Failed to get OpenWeatherMap forecast data: %d %s\n",
		     msg->status_code, msg->reason_phrase);
	    return;
	}
	g_warning ("Failed to get OpenWeatherMap forecast data: %d %s\n",
		   msg->status_code, msg->reason_phrase);
	_gweather_info_request_done (user_data, msg);
	return;
    }

    info = user_data;
    priv = info->priv;
    loc = &priv->location;
    g_debug ("owm data for %lf, %lf", loc->latitude, loc->longitude);
    g_debug ("%s", msg->response_body->data);

    parse_forecast_xml (info, msg->response_body);
    _gweather_info_request_done (info, msg);
}

gboolean
owm_start_open (GWeatherInfo *info)
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

    /* see the description here: http://bugs.openweathermap.org/projects/api/wiki/Api_2_5_forecast */

    g_ascii_dtostr (latstr, sizeof(latstr), RADIANS_TO_DEGREES (loc->latitude));
    g_ascii_dtostr (lonstr, sizeof(lonstr), RADIANS_TO_DEGREES (loc->longitude));

#define TEMPLATE_START "https://api.openweathermap.org/data/2.5/forecast?lat=%s&lon=%s&mode=xml&units=metric"
#ifdef OWM_APIKEY
 #define TEMPLATE TEMPLATE_START "&APPID=" OWM_APIKEY
#else
 #define TEMPLATE TEMPLATE_START
#endif

    url = g_strdup_printf (TEMPLATE, latstr, lonstr);
    g_debug ("owm_start_open, requesting: %s", url);

#undef TEMPLATE

    message = soup_message_new ("GET", url);
    _gweather_info_begin_request (info, message);
    soup_session_queue_message (priv->session, message, owm_finish, info);

    g_free (url);

    return TRUE;
}
