/* weather-owm.c - Open Weather Map backend
 *
 * SPDX-FileCopyrightText: The GWeather authors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "gweather-private.h"

#include <assert.h>
#include <ctype.h>
#include <langinfo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#define XC(t) ((const xmlChar *) (t))

/* Reference for symbols at http://bugs.openweathermap.org/projects/api/wiki/Weather_Condition_Codes */
/* FIXME: the libgweather API is not expressive enough */
static struct owm_symbol
{
    int symbol;
    GWeatherSky sky;
    GWeatherConditions condition;
} symbols[] = {
    { 200, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } },    /* Thunderstorm with light rain */
    { 201, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } },    /* Thunderstorm with rain */
    { 202, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } },    /* Thunderstorm with heavy rain */
    { 210, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } },    /* Light thunderstorm */
    { 211, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } },    /* Thunderstorm */
    { 212, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } },    /* Heavy thunderstorm */
    { 221, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_THUNDERSTORM } },    /* Ragged thunderstorm */
    { 230, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Thunderstorm with light drizzle */
    { 231, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Thunderstorm with drizzle */
    { 232, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_THUNDERSTORM } }, /* Thunderstorm with heavy drizzle */

    { 300, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_LIGHT } },   /* Light intensity drizzle */
    { 301, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_NONE } },    /* Drizzle */
    { 302, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_HEAVY } },   /* Heavy intensity drizzle */
    { 310, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_LIGHT } },   /* Light intensity drizzle rain */
    { 311, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_NONE } },    /* Drizzle rain */
    { 312, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_HEAVY } },   /* Heavy intensity drizzle rain */
    { 321, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_DRIZZLE, GWEATHER_QUALIFIER_SHOWERS } }, /* Drizzle showers */

    { 500, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_LIGHT } },    /* Light rain */
    { 501, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_MODERATE } }, /* Moderate rain */
    { 502, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_HEAVY } },    /* Heavy intensity rain */
    { 503, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_HEAVY } },    /* Very heavy rain */
    { 504, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_HEAVY } },    /* Extreme rain */
    { 511, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_FREEZING } }, /* Freezing rain */
    { 520, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_SHOWERS } },  /* Light intensity shower rain */
    { 521, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_SHOWERS } },  /* Shower rain */
    { 522, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_RAIN, GWEATHER_QUALIFIER_SHOWERS } },  /* Heavy intensity shower rain */

    { 600, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_LIGHT } },       /* Light snow */
    { 601, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_NONE } },        /* Snow */
    { 602, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_HEAVY } },       /* Heavy snow */
    { 611, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_ICE_PELLETS, GWEATHER_QUALIFIER_NONE } }, /* Sleet */
    { 621, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_SNOW, GWEATHER_QUALIFIER_SHOWERS } },     /* Shower snow */

    { 701, GWEATHER_SKY_CLEAR, { TRUE, GWEATHER_PHENOMENON_MIST, GWEATHER_QUALIFIER_NONE } },        /* Mist */
    { 711, GWEATHER_SKY_CLEAR, { TRUE, GWEATHER_PHENOMENON_SMOKE, GWEATHER_QUALIFIER_NONE } },       /* Smoke */
    { 721, GWEATHER_SKY_CLEAR, { TRUE, GWEATHER_PHENOMENON_HAZE, GWEATHER_QUALIFIER_NONE } },        /* Haze */
    { 731, GWEATHER_SKY_CLEAR, { TRUE, GWEATHER_PHENOMENON_DUST_WHIRLS, GWEATHER_QUALIFIER_NONE } }, /* Dust/sand whirls */
    { 741, GWEATHER_SKY_CLEAR, { TRUE, GWEATHER_PHENOMENON_FOG, GWEATHER_QUALIFIER_NONE } },         /* Fog */

    { 800, GWEATHER_SKY_CLEAR, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } },     /* Clear sky */
    { 801, GWEATHER_SKY_FEW, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } },       /* Few clouds */
    { 802, GWEATHER_SKY_SCATTERED, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } }, /* Scattered clouds */
    { 803, GWEATHER_SKY_BROKEN, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } },    /* Broken clouds */
    { 804, GWEATHER_SKY_OVERCAST, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } },  /* Overcast clouds */

    /* XXX: all these are a bit iffy */
    { 900, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_TORNADO, GWEATHER_QUALIFIER_NONE } }, /* Tornado */
    { 901, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_TORNADO, GWEATHER_QUALIFIER_NONE } }, /* Tropical storm */
    { 902, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_TORNADO, GWEATHER_QUALIFIER_NONE } }, /* Hurricane */
    { 903, GWEATHER_SKY_CLEAR, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } },      /* Cold */
    { 904, GWEATHER_SKY_CLEAR, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } },      /* Hot */
    { 905, GWEATHER_SKY_CLEAR, { FALSE, GWEATHER_PHENOMENON_NONE, GWEATHER_QUALIFIER_NONE } },      /* Windy */
    { 906, GWEATHER_SKY_OVERCAST, { TRUE, GWEATHER_PHENOMENON_HAIL, GWEATHER_QUALIFIER_NONE } },    /* Hail */
};

static struct
{
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
date_to_time_t (const xmlChar *str, const char *tzid)
{
    struct tm time = { 0 };
    GTimeZone *tz;
    GDateTime *dt;
    time_t rval;
    char *after;

    after = strptime ((const char *) str, "%Y-%m-%dT%T", &time);
    if (after == NULL) {
        g_warning ("Cannot parse date string \"%s\"", str);
        return 0;
    }

    if (*after == 'Z')
        tzid = "UTC";

    tz = g_time_zone_new_identifier (tzid);
    if (tz == NULL)
        tz = g_time_zone_new_utc ();

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
             xmlNodePtr node)
{
    xmlChar *val;
    struct owm_symbol *obj, ref;

    val = xmlGetProp (node, XC ("number"));

    ref.symbol = strtol ((char *) val, NULL, 0) - 1;
    xmlFree (val);
    obj = bsearch (&ref, symbols, G_N_ELEMENTS (symbols), sizeof (struct owm_symbol), symbol_compare);

    if (obj == NULL) {
        g_warning ("Unknown symbol %d returned from OpenWeatherMap",
                   ref.symbol);
        return;
    }

    info->valid = TRUE;
    info->sky = obj->sky;
    info->cond = obj->condition;
}

static inline void
read_wind_direction (GWeatherInfo *info,
                     xmlNodePtr node)
{
    xmlChar *val;
    unsigned int i;

    val = xmlGetProp (node, XC ("code"));
    if (val == NULL)
        return;

    for (i = 0; i < G_N_ELEMENTS (wind_directions); i++) {
        if (strcmp ((char *) val, wind_directions[i].name) == 0) {
            info->wind = wind_directions[i].direction;
            xmlFree (val);
            return;
        }
    }
    xmlFree (val);
}

static inline void
read_wind_speed (GWeatherInfo *info,
                 xmlNodePtr node)
{
    xmlChar *val;
    double mps;

    val = xmlGetProp (node, XC ("mps"));
    if (val == NULL)
        return;

    mps = g_ascii_strtod ((char *) val, NULL);
    info->windspeed = WINDSPEED_MS_TO_KNOTS (mps);
    xmlFree (val);
}

static inline void
read_temperature (GWeatherInfo *info,
                  xmlNodePtr node)
{
    xmlChar *unit;
    xmlChar *val;
    double celsius;

    unit = xmlGetProp (node, XC ("unit"));
    if (unit == NULL || strcmp ((char *) unit, "celsius")) {
        xmlFree (unit);
        return;
    }

    xmlFree (unit);
    val = xmlGetProp (node, XC ("value"));
    if (val == NULL)
        return;

    celsius = g_ascii_strtod ((char *) val, NULL);
    info->temp = TEMP_C_TO_F (celsius);
    xmlFree (val);
}

static inline void
read_pressure (GWeatherInfo *info,
               xmlNodePtr node)
{
    xmlChar *unit;
    xmlChar *val;
    double hpa;

    /* hPa == mbar */
    unit = xmlGetProp (node, XC ("unit"));
    if (unit == NULL || strcmp ((char *) unit, "hPa")) {
        xmlFree (unit);
        return;
    }

    xmlFree (unit);
    val = xmlGetProp (node, XC ("value"));
    if (val == NULL)
        return;

    hpa = g_ascii_strtod ((char *) val, NULL);
    info->pressure = PRESSURE_MBAR_TO_INCH (hpa);
    xmlFree (val);
}

static inline void
read_humidity (GWeatherInfo *info,
               xmlNodePtr node)
{
    xmlChar *unit;
    xmlChar *val;
    double percent;

    unit = xmlGetProp (node, XC ("unit"));
    if (unit == NULL || strcmp ((char *) unit, "%")) {
        xmlFree (unit);
        return;
    }

    xmlFree (unit);
    val = xmlGetProp (node, XC ("value"));
    if (val == NULL)
        return;

    percent = g_ascii_strtod ((char *) val, NULL);
    info->humidity = percent;
    info->hasHumidity = TRUE;
    xmlFree (val);
}

static inline void
read_child_node (GWeatherInfo *info,
                 xmlNodePtr node)
{
    if (strcmp ((char *) node->name, "symbol") == 0)
        read_symbol (info, node);
    else if (strcmp ((char *) node->name, "windDirection") == 0)
        read_wind_direction (info, node);
    else if (strcmp ((char *) node->name, "windSpeed") == 0)
        read_wind_speed (info, node);
    else if (strcmp ((char *) node->name, "temperature") == 0)
        read_temperature (info, node);
    else if (strcmp ((char *) node->name, "pressure") == 0)
        read_pressure (info, node);
    else if (strcmp ((char *) node->name, "humidity") == 0)
        read_humidity (info, node);
}

static inline void
fill_info_from_node (GWeatherInfo *info,
                     xmlNodePtr node)
{
    xmlNodePtr child;

    for (child = node->children; child != NULL; child = child->next) {
        if (child->type == XML_ELEMENT_NODE)
            read_child_node (info, child);
    }
}

static GWeatherInfo *
make_info_from_node (GWeatherInfo *original_info,
                     xmlNodePtr node)
{
    GWeatherInfo *info;
    xmlChar *val;

    g_return_val_if_fail (node->type == XML_ELEMENT_NODE, NULL);

    info = _gweather_info_new_clone (original_info);

    val = xmlGetProp (node, XC ("from"));
    info->current_time = info->update = date_to_time_t (val, info->location.tz_hint);
    xmlFree (val);

    fill_info_from_node (info, node);

    return info;
}

static void
parse_forecast_xml (GWeatherInfo *original_info,
                    const char *data,
                    gsize length)
{
    xmlDocPtr doc;
    xmlXPathContextPtr xpath_ctx;
    xmlXPathObjectPtr xpath_result;
    int i;

    doc = xmlParseMemory (data, length);
    if (!doc)
        return;

    xpath_ctx = xmlXPathNewContext (doc);
    xpath_result = xmlXPathEval (XC ("/weatherdata/forecast/time"), xpath_ctx);

    if (!xpath_result || xpath_result->type != XPATH_NODESET)
        goto out;

    for (i = 0; i < xpath_result->nodesetval->nodeNr; i++) {
        xmlNodePtr node;
        GWeatherInfo *info;

        node = xpath_result->nodesetval->nodeTab[i];
        info = make_info_from_node (original_info, node);

        info->forecast_list = g_slist_append (info->forecast_list, info);
    }

    xmlXPathFreeObject (xpath_result);

    xpath_result = xmlXPathEval (XC ("/weatherdata/credit/link"), xpath_ctx);
    if (!xpath_result || xpath_result->type != XPATH_NODESET)
        goto out;

    original_info->forecast_attribution = g_strdup (_ ("Weather data from the <a href=\"https://openweathermap.org\">Open Weather Map project</a>"));

out:
    if (xpath_result)
        xmlXPathFreeObject (xpath_result);
    xmlXPathFreeContext (xpath_ctx);
    xmlFreeDoc (doc);
}

static void
owm_finish (GObject *source,
            GAsyncResult *result,
            gpointer data)
{
    GWeatherInfo *info;
    WeatherLocation *loc;
    SoupSession *session = SOUP_SESSION (source);
    SoupMessage *msg = soup_session_get_async_result_message (session, result);
    GBytes *body;
    GError *error = NULL;
    const char *content;
    gsize length;

    body = soup_session_send_and_read_finish (session, result, &error);

    if (!body) {
        /* forecast data is not really interesting anyway ;) */
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_debug ("Failed to get OpenWeatheRMap forecast data: %s",
                     error->message);
            return;
        }
        g_warning ("Failed to get OpenWeatherMap forecast data: %s",
                   error->message);
        g_clear_error (&error);
        _gweather_info_request_done (data, msg);
        return;
    } else if (!SOUP_STATUS_IS_SUCCESSFUL (soup_message_get_status (msg))) {
        g_bytes_unref (body);
        g_warning ("Failed to get OpenWeatherMap forecast data: [status: %d]: %s",
                   soup_message_get_status (msg),
                   soup_message_get_reason_phrase (msg));
        _gweather_info_request_done (data, msg);
        return;
    }

    content = g_bytes_get_data (body, &length);

    info = data;
    loc = &info->location;
    g_debug ("owm data for %lf, %lf", loc->latitude, loc->longitude);
    g_debug ("%s", content);

    parse_forecast_xml (info, content, length);
    g_bytes_unref (body);
    _gweather_info_request_done (info, msg);
}

gboolean
owm_start_open (GWeatherInfo *info)
{
    gchar *url;
    SoupMessage *message;
    WeatherLocation *loc;
    g_autofree char *latstr = NULL;
    g_autofree char *lonstr = NULL;

    loc = &info->location;

    if (!loc->latlon_valid)
        return FALSE;

    /* see the description here: http://bugs.openweathermap.org/projects/api/wiki/Api_2_5_forecast */

    latstr = _radians_to_degrees_str (loc->latitude);
    lonstr = _radians_to_degrees_str (loc->longitude);

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
    _gweather_info_queue_request (info, message, owm_finish);

    g_free (url);

    return TRUE;
}
