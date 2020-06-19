/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* weather.c - Overall weather server functions
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
#include <errno.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>

#include "gweather-weather.h"
#include "gweather-private.h"
#include "gweather-enum-types.h"

#define MOON_PHASES 36


/**
 * SECTION:gweatherinfo
 * @Title: GWeatherInfo
 *
 * #GWeatherInfo provides a handy way to access weather conditions
 * and forecasts from a #GWeatherLocation, aggregating multiple
 * different web services.
 *
 * It includes also astronomical data such as sunrise times and
 * moon phases.
 */


#define TEMPERATURE_UNIT "temperature-unit"
#define DISTANCE_UNIT    "distance-unit"
#define SPEED_UNIT       "speed-unit"
#define PRESSURE_UNIT    "pressure-unit"
#define RADAR_KEY        "radar"
#define DEFAULT_LOCATION "default-location"

enum {
    PROP_0,
    PROP_LOCATION,
    PROP_ENABLED_PROVIDERS,
    PROP_LAST
};

enum {
    SIGNAL_UPDATED,
    SIGNAL_LAST
};

static guint gweather_info_signals[SIGNAL_LAST];

G_DEFINE_TYPE (GWeatherInfo, gweather_info, G_TYPE_OBJECT);

void
_gweather_gettext_init (void)
{
    static gsize gweather_gettext_initialized = FALSE;

    if (G_UNLIKELY (g_once_init_enter (&gweather_gettext_initialized))) {
        bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

        bindtextdomain ("libgweather-locations", GNOMELOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
        bind_textdomain_codeset ("libgweather-locations", "UTF-8");
#endif

        g_once_init_leave (&gweather_gettext_initialized, TRUE);
    }
}

static void
_weather_location_free (WeatherLocation *location)
{
    g_free (location->name);
    g_free (location->code);
    g_free (location->zone);
    g_free (location->radar);
    g_free (location->country_code);
    g_free (location->tz_hint);
}

static gboolean
should_use_caps (GWeatherFormatOptions options) {
    return options == GWEATHER_FORMAT_OPTION_DEFAULT ||
           (options & GWEATHER_FORMAT_OPTION_SENTENCE_CAPITALIZATION);
}

static const gchar *wind_direction_str[] = {
    N_("variable"),
    N_("north"), N_("north — northeast"), N_("northeast"), N_("east — northeast"),
    N_("east"), N_("east — southeast"), N_("southeast"), N_("south — southeast"),
    N_("south"), N_("south — southwest"), N_("southwest"), N_("west — southwest"),
    N_("west"), N_("west — northwest"), N_("northwest"), N_("north — northwest")
};

static const gchar *wind_direction_caps_str[] = {
    N_("Variable"),
    N_("North"), N_("North — Northeast"), N_("Northeast"), N_("East — Northeast"),
    N_("East"), N_("East — Southeast"), N_("Southeast"), N_("South — Southeast"),
    N_("South"), N_("South — Southwest"), N_("Southwest"), N_("West — Southwest"),
    N_("West"), N_("West — Northwest"), N_("Northwest"), N_("North — Northwest")
};

const gchar *
gweather_wind_direction_to_string_full (GWeatherWindDirection wind,
                                        GWeatherFormatOptions options)
{
    gboolean use_caps = should_use_caps (options);

    if (wind <= GWEATHER_WIND_INVALID || wind >= GWEATHER_WIND_LAST)
	return use_caps ? C_("wind direction", "Invalid")
	                : C_("wind direction", "invalid");

    return use_caps ? _(wind_direction_caps_str[(int)wind])
                    : _(wind_direction_str[(int)wind]);
}

const gchar *
gweather_wind_direction_to_string (GWeatherWindDirection wind)
{
    return gweather_wind_direction_to_string_full (wind, GWEATHER_FORMAT_OPTION_DEFAULT);
}

static const gchar *sky_str[] = {
    N_("clear sky"),
    N_("broken clouds"),
    N_("scattered clouds"),
    N_("few clouds"),
    N_("overcast")
};

static const gchar *sky_caps_str[] = {
    N_("Clear sky"),
    N_("Broken clouds"),
    N_("Scattered clouds"),
    N_("Few clouds"),
    N_("Overcast")
};

const char *
gweather_sky_to_string (GWeatherSky sky)
{
    return gweather_sky_to_string_full (sky, GWEATHER_FORMAT_OPTION_DEFAULT);
}

const gchar *
gweather_sky_to_string_full (GWeatherSky           sky,
                             GWeatherFormatOptions options)
{
    gboolean use_caps = should_use_caps (options);

    if (sky <= GWEATHER_SKY_INVALID || sky >= GWEATHER_SKY_LAST)
	return use_caps ? C_("sky conditions", "Invalid")
                        : C_("sky conditions", "invalid");

    return use_caps ? _(sky_caps_str[(int)sky])
                    : _(sky_str[(int)sky]);
}


/*
 * Even though tedious, I switched to a 2D array for weather condition
 * strings, in order to facilitate internationalization, esp. for languages
 * with genders.
 */

/*
 * Almost all reportable combinations listed in
 * http://www.crh.noaa.gov/arx/wx.tbl.php are entered below, except those
 * having 2 qualifiers mixed together [such as "Blowing snow in vicinity"
 * (VCBLSN), "Thunderstorm in vicinity" (VCTS), etc].
 * Combinations that are not possible are filled in with "??".
 * Some other exceptions not handled yet, such as "SN BLSN" which has
 * special meaning.
 */

/*
 * Note, magic numbers, when you change the size here, make sure to change
 * the below function so that new values are recognized
 */
/*                   NONE                         VICINITY                             LIGHT                      MODERATE                      HEAVY                      SHALLOW                      PATCHES                         PARTIAL                      THUNDERSTORM                    BLOWING                      SHOWERS                         DRIFTING                      FREEZING                      */
/*               *******************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************/
static const gchar *conditions_str[24][13] = {
/* TRANSLATOR: If you want to know what "blowing" "shallow" "partial"
 * etc means, you can go to http://www.weather.com/glossary/ and
 * http://www.crh.noaa.gov/arx/wx.tbl.php */
    /* NONE          */ {"??",                        "??",                                "??",                      "??",                         "??",                      "??",                        "??",                           "??",                        N_("thunderstorm"),             "??",                        "??",                           "??",                         "??"                         },
    /* DRIZZLE       */ {N_("drizzle"),               "??",                                N_("light drizzle"),       N_("moderate drizzle"),       N_("heavy drizzle"),       "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         N_("freezing drizzle")       },
    /* RAIN          */ {N_("rain"),                  "??",                                N_("light rain"),          N_("moderate rain"),          N_("heavy rain"),          "??",                        "??",                           "??",                        N_("thunderstorm"),             "??",                        N_("rain showers"),             "??",                         N_("freezing rain")          },
    /* SNOW          */ {N_("snow"),                  "??",                                N_("light snow"),          N_("moderate snow"),          N_("heavy snow"),          "??",                        "??",                           "??",                        N_("snowstorm"),                N_("blowing snowfall"),      N_("snow showers"),             N_("drifting snow"),          "??"                         },
    /* SNOW_GRAINS   */ {N_("snow grains"),           "??",                                N_("light snow grains"),   N_("moderate snow grains"),   N_("heavy snow grains"),   "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* ICE_CRYSTALS  */ {N_("ice crystals"),          "??",                                "??",                      N_("ice crystals"),           "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* ICE_PELLETS   */ {N_("sleet"),           "??",                                N_("little sleet"),     N_("moderate sleet"),   N_("heavy sleet"),   "??",                        "??",                           "??",                        N_("sleet storm"),         "??",                        N_("showers of sleet"),   "??",                         "??"                         },
    /* HAIL          */ {N_("hail"),                  "??",                                "??",                      N_("hail"),                   "??",                      "??",                        "??",                           "??",                        N_("hailstorm"),                "??",                        N_("hail showers"),             "??",                         "??",                        },
    /* SMALL_HAIL    */ {N_("small hail"),            "??",                                "??",                      N_("small hail"),             "??",                      "??",                        "??",                           "??",                        N_("small hailstorm"),          "??",                        N_("showers of small hail"),    "??",                         "??"                         },
    /* PRECIPITATION */ {N_("unknown precipitation"), "??",                                "??",                      "??",                         "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* MIST          */ {N_("mist"),                  "??",                                "??",                      N_("mist"),                   "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* FOG           */ {N_("fog"),                   N_("fog in the vicinity") ,          "??",                      N_("fog"),                    "??",                      N_("shallow fog"),           N_("patches of fog"),           N_("partial fog"),           "??",                           "??",                        "??",                           "??",                         N_("freezing fog")           },
    /* SMOKE         */ {N_("smoke"),                 "??",                                "??",                      N_("smoke"),                  "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* VOLCANIC_ASH  */ {N_("volcanic ash"),          "??",                                "??",                      N_("volcanic ash"),           "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* SAND          */ {N_("sand"),                  "??",                                "??",                      N_("sand"),                   "??",                      "??",                        "??",                           "??",                        "??",                           N_("blowing sand"),          "",                             N_("drifting sand"),          "??"                         },
    /* HAZE          */ {N_("haze"),                  "??",                                "??",                      N_("haze"),                   "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* SPRAY         */ {"??",                        "??",                                "??",                      "??",                         "??",                      "??",                        "??",                           "??",                        "??",                           N_("blowing sprays"),        "??",                           "??",                         "??"                         },
    /* DUST          */ {N_("dust"),                  "??",                                "??",                      N_("dust"),                   "??",                      "??",                        "??",                           "??",                        "??",                           N_("blowing dust"),          "??",                           N_("drifting dust"),          "??"                         },
    /* SQUALL        */ {N_("squall"),                "??",                                "??",                      N_("squall"),                 "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* SANDSTORM     */ {N_("sandstorm"),             N_("sandstorm in the vicinity") ,    "??",                      N_("sandstorm"),              N_("heavy sandstorm"),     "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* DUSTSTORM     */ {N_("duststorm"),             N_("duststorm in the vicinity") ,    "??",                      N_("duststorm"),              N_("heavy duststorm"),     "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* FUNNEL_CLOUD  */ {N_("funnel cloud"),          "??",                                "??",                      "??",                         "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* TORNADO       */ {N_("tornado"),               "??",                                "??",                      "??",                         "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* DUST_WHIRLS   */ {N_("dust whirls"),           N_("dust whirls in the vicinity") ,  "??",                      N_("dust whirls"),            "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         }
};

/*
 * Note, magic numbers, when you change the size here, make sure to change
 * the below function so that new values are recognized
 */
/*                   NONE                         VICINITY                             LIGHT                      MODERATE                      HEAVY                      SHALLOW                      PATCHES                         PARTIAL                      THUNDERSTORM                    BLOWING                      SHOWERS                         DRIFTING                      FREEZING                      */
/*               *******************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************/
static const gchar *conditions_caps_str[24][13] = {
/* TRANSLATOR: If you want to know what "blowing" "shallow" "partial"
 * etc means, you can go to http://www.weather.com/glossary/ and
 * http://www.crh.noaa.gov/arx/wx.tbl.php */
    /* NONE          */ {"??",                        "??",                                "??",                      "??",                         "??",                      "??",                        "??",                           "??",                        N_("Thunderstorm"),             "??",                        "??",                           "??",                         "??"                         },
    /* DRIZZLE       */ {N_("Drizzle"),               "??",                                N_("Light drizzle"),       N_("Moderate drizzle"),       N_("Heavy drizzle"),       "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         N_("Freezing drizzle")       },
    /* RAIN          */ {N_("Rain"),                  "??",                                N_("Light rain"),          N_("Moderate rain"),          N_("Heavy rain"),          "??",                        "??",                           "??",                        N_("Thunderstorm"),             "??",                        N_("Rain showers"),             "??",                         N_("Freezing rain")          },
    /* SNOW          */ {N_("Snow"),                  "??",                                N_("Light snow"),          N_("Moderate snow"),          N_("Heavy snow"),          "??",                        "??",                           "??",                        N_("Snowstorm"),                N_("Blowing snowfall"),      N_("Snow showers"),             N_("Drifting snow"),          "??"                         },
    /* SNOW_GRAINS   */ {N_("Snow grains"),           "??",                                N_("Light snow grains"),   N_("Moderate snow grains"),   N_("Heavy snow grains"),   "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* ICE_CRYSTALS  */ {N_("Ice crystals"),          "??",                                "??",                      N_("Ice crystals"),           "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* ICE_PELLETS   */ {N_("Sleet"),           "??",                                N_("Little sleet"),     N_("Moderate sleet"),   N_("Heavy sleet"),   "??",                        "??",                           "??",                        N_("Sleet storm"),         "??",                        N_("Showers of sleet"),   "??",                         "??"                         },
    /* HAIL          */ {N_("Hail"),                  "??",                                "??",                      N_("Hail"),                   "??",                      "??",                        "??",                           "??",                        N_("Hailstorm"),                "??",                        N_("Hail showers"),             "??",                         "??",                        },
    /* SMALL_HAIL    */ {N_("Small hail"),            "??",                                "??",                      N_("Small hail"),             "??",                      "??",                        "??",                           "??",                        N_("Small hailstorm"),          "??",                        N_("Showers of small hail"),    "??",                         "??"                         },
    /* PRECIPITATION */ {N_("Unknown precipitation"), "??",                                "??",                      "??",                         "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* MIST          */ {N_("Mist"),                  "??",                                "??",                      N_("Mist"),                   "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* FOG           */ {N_("Fog"),                   N_("Fog in the vicinity") ,          "??",                      N_("Fog"),                    "??",                      N_("Shallow fog"),           N_("Patches of fog"),           N_("Partial fog"),           "??",                           "??",                        "??",                           "??",                         N_("Freezing fog")           },
    /* SMOKE         */ {N_("Smoke"),                 "??",                                "??",                      N_("Smoke"),                  "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* VOLCANIC_ASH  */ {N_("Volcanic ash"),          "??",                                "??",                      N_("Volcanic ash"),           "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* SAND          */ {N_("Sand"),                  "??",                                "??",                      N_("Sand"),                   "??",                      "??",                        "??",                           "??",                        "??",                           N_("Blowing sand"),          "",                             N_("Drifting sand"),          "??"                         },
    /* HAZE          */ {N_("Haze"),                  "??",                                "??",                      N_("Haze"),                   "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* SPRAY         */ {"??",                        "??",                                "??",                      "??",                         "??",                      "??",                        "??",                           "??",                        "??",                           N_("Blowing sprays"),        "??",                           "??",                         "??"                         },
    /* DUST          */ {N_("Dust"),                  "??",                                "??",                      N_("Dust"),                   "??",                      "??",                        "??",                           "??",                        "??",                           N_("Blowing dust"),          "??",                           N_("Drifting dust"),          "??"                         },
    /* SQUALL        */ {N_("Squall"),                "??",                                "??",                      N_("Squall"),                 "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* SANDSTORM     */ {N_("Sandstorm"),             N_("Sandstorm in the vicinity") ,    "??",                      N_("Sandstorm"),              N_("Heavy sandstorm"),     "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* DUSTSTORM     */ {N_("Duststorm"),             N_("Duststorm in the vicinity") ,    "??",                      N_("Duststorm"),              N_("Heavy duststorm"),     "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* FUNNEL_CLOUD  */ {N_("Funnel cloud"),          "??",                                "??",                      "??",                         "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* TORNADO       */ {N_("Tornado"),               "??",                                "??",                      "??",                         "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         },
    /* DUST_WHIRLS   */ {N_("Dust whirls"),           N_("Dust whirls in the vicinity") ,  "??",                      N_("Dust whirls"),            "??",                      "??",                        "??",                           "??",                        "??",                           "??",                        "??",                           "??",                         "??"                         }
};

const gchar *
gweather_conditions_to_string_full (GWeatherConditions    *cond,
                                    GWeatherFormatOptions  options)
{
    gboolean use_caps = should_use_caps (options);

    const gchar *str;

    if (!cond->significant) {
	return "-";
    } else {
	if (cond->phenomenon > GWEATHER_PHENOMENON_INVALID &&
	    cond->phenomenon < GWEATHER_PHENOMENON_LAST &&
	    cond->qualifier > GWEATHER_QUALIFIER_INVALID &&
	    cond->qualifier < GWEATHER_QUALIFIER_LAST)
	    str = use_caps ? _(conditions_caps_str[(int)cond->phenomenon][(int)cond->qualifier])
	                   : _(conditions_str[(int)cond->phenomenon][(int)cond->qualifier]);
	else
	    str = use_caps ? C_("sky conditions", "Invalid")
	                   : C_("sky conditions", "invalid");
	return (strlen (str) > 0) ? str : "-";
    }
}

const gchar *
gweather_conditions_to_string (GWeatherConditions *cond)
{
    return gweather_conditions_to_string_full (cond, GWEATHER_FORMAT_OPTION_DEFAULT);
}

static gboolean
requests_init (GWeatherInfo *info)
{
    if (info->priv->requests_pending)
        return FALSE;

    return TRUE;
}

void
_gweather_info_begin_request (GWeatherInfo *info,
			      SoupMessage  *message)
{
    info->priv->requests_pending = g_slist_prepend (info->priv->requests_pending, message);
    g_object_ref (message);
}

static void
copy_weather_data (GWeatherInfo *src,
		   GWeatherInfo *dest)
{
  dest->priv->hasHumidity = src->priv->hasHumidity;
  dest->priv->update = src->priv->update;
  dest->priv->current_time = src->priv->current_time;
  dest->priv->sky = src->priv->sky;
  dest->priv->cond = src->priv->cond;
  dest->priv->temp = src->priv->temp;
  dest->priv->temp_min = src->priv->temp_min;
  dest->priv->temp_max = src->priv->temp_max;
  dest->priv->dew = src->priv->dew;
  dest->priv->humidity = src->priv->humidity;
  dest->priv->wind = src->priv->wind;
  dest->priv->windspeed = src->priv->windspeed;
  dest->priv->pressure = src->priv->pressure;
  dest->priv->visibility = src->priv->visibility;
}

static void
fixup_current_conditions (GWeatherInfo *info)
{
  GWeatherInfo *first_forecast;

  /* Current conditions already available */
  if (info->priv->update != 0) {
    g_debug ("Not fixing up current conditions, already valid");
    return;
  } else if (!info->priv->forecast_list ||
             !info->priv->forecast_list->data) {
    g_debug ("No forecast list available, not fixing up");
    return;
  }

  first_forecast = info->priv->forecast_list->data;
  /* Add current conditions from forecast if close enough */
  if (first_forecast->priv->update - time(NULL) > 60 * 60) {
    g_debug ("Forecast is too far in the future, ignoring");
    return;
  }

  copy_weather_data (first_forecast, info);
  g_debug ("Fixed up missing current weather with first forecast data");
}

void
_gweather_info_request_done (GWeatherInfo *info,
			     SoupMessage  *message)
{
    info->priv->requests_pending = g_slist_remove (info->priv->requests_pending, message);
    g_object_ref (message);

    if (info->priv->requests_pending == NULL) {
        fixup_current_conditions (info);
        g_signal_emit (info, gweather_info_signals[SIGNAL_UPDATED], 0);
    } else {
        g_debug ("Not emitting 'updated' as there are still %d requests pending",
                 g_slist_length (info->priv->requests_pending));
    }
}

/* it's OK to pass in NULL */
void
free_forecast_list (GWeatherInfo *info)
{
    if (!info)
	return;

    g_slist_free_full (info->priv->forecast_list, g_object_unref);
    info->priv->forecast_list = NULL;
}

/* Relative humidity computation - thanks to <Olof.Oberg@modopaper.modogroup.com>
   calc_dew is simply the inverse of calc_humidity */

static inline gdouble
calc_dew (gdouble temp, gdouble humidity)
{
    gdouble esat, esurf, tmp;

    if (temp > -500.0 && humidity > -1.0) {
	temp = TEMP_F_TO_C (temp);

	esat = 6.11 * pow (10.0, (7.5 * temp) / (237.7 + temp));
	esurf = (humidity / 100) * esat;
    } else {
	esurf = -1.0;
	esat = 1.0;
    }

    tmp = log10 (esurf / 6.11);
    return TEMP_C_TO_F (tmp * 237.7 / (tmp + 7.5));
}

static inline gdouble
calc_humidity (gdouble temp, gdouble dewp)
{
    gdouble esat, esurf;

    if (temp > -500.0 && dewp > -500.0) {
	temp = TEMP_F_TO_C (temp);
	dewp = TEMP_F_TO_C (dewp);

	esat = 6.11 * pow (10.0, (7.5 * temp) / (237.7 + temp));
	esurf = 6.11 * pow (10.0, (7.5 * dewp) / (237.7 + dewp));
    } else {
	esurf = -1.0;
	esat = 1.0;
    }
    return ((esurf/esat) * 100.0);
}

static inline gdouble
calc_apparent (GWeatherInfo *info)
{
    gdouble temp = info->priv->temp;
    gdouble wind = WINDSPEED_KNOTS_TO_MPH (info->priv->windspeed);
    gdouble apparent = -1000.;
    gdouble dew = info->priv->dew;
    gdouble humidity;

    if (info->priv->hasHumidity)
	humidity = info->priv->humidity;
    else
	humidity = calc_humidity (temp, dew);

    /*
     * Wind chill calculations as of 01-Nov-2001
     * http://www.nws.noaa.gov/om/windchill/index.shtml
     * Some pages suggest that the formula will soon be adjusted
     * to account for solar radiation (bright sun vs cloudy sky)
     */
    if (temp <= 50.0) {
        if (wind > 3.0) {
	    gdouble v = pow (wind, 0.16);
	    apparent = 35.74 + 0.6215 * temp - 35.75 * v + 0.4275 * temp * v;
	} else if (wind >= 0.) {
	    apparent = temp;
	}
    }
    /*
     * Heat index calculations:
     * http://www.srh.noaa.gov/fwd/heatindex/heat5.html
     */
    else if (temp >= 80.0) {
        if (temp >= -500. && humidity >= 0) {
	    gdouble t2, h2;
	    gdouble t3, h3;

	    t2 = temp * temp;
	    h2 = humidity * humidity;

#if 1
	    /*
	     * A really precise formula.  Note that overall precision is
	     * constrained by the accuracy of the instruments and that the
	     * we receive the temperature and dewpoints as integers.
	     */

	    t3 = t2 * temp;
	    h3 = h2 * temp;

	    apparent = 16.923
		+ 0.185212 * temp
		+ 5.37941 * humidity
		- 0.100254 * temp * humidity
		+ 9.41695e-3 * t2
		+ 7.28898e-3 * h2
		+ 3.45372e-4 * t2 * humidity
		- 8.14971e-4 * temp * h2
		+ 1.02102e-5 * t2 * h2
		- 3.8646e-5 * t3
		+ 2.91583e-5 * h3
		+ 1.42721e-6 * t3 * humidity
		+ 1.97483e-7 * temp * h3
		- 2.18429e-8 * t3 * h2
		+ 8.43296e-10 * t2 * h3
		- 4.81975e-11 * t3 * h3;
#else
	    /*
	     * An often cited alternative: values are within 5 degrees for
	     * most ranges between 10% and 70% humidity and to 110 degrees.
	     */
	    apparent = - 42.379
		+  2.04901523 * temp
		+ 10.14333127 * humidity
		-  0.22475541 * temp * humidity
		-  6.83783e-3 * t2
		-  5.481717e-2 * h2
		+  1.22874e-3 * t2 * humidity
		+  8.5282e-4 * temp * h2
		-  1.99e-6 * t2 * h2;
#endif
	}
    } else {
        apparent = temp;
    }

    return apparent;
}

static void
gweather_info_reset (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv = info->priv;

    g_free (priv->forecast_attribution);
    priv->forecast_attribution = NULL;

    free_forecast_list (info);

    if (priv->radar != NULL) {
	g_object_unref (priv->radar);
	priv->radar = NULL;
    }

    priv->update = 0;
    priv->current_time = time(NULL);
    priv->sky = -1;
    priv->cond.significant = FALSE;
    priv->cond.phenomenon = GWEATHER_PHENOMENON_NONE;
    priv->cond.qualifier = GWEATHER_QUALIFIER_NONE;
    priv->temp = -1000.0;
    priv->tempMinMaxValid = FALSE;
    priv->temp_min = -1000.0;
    priv->temp_max = -1000.0;
    priv->dew = -1000.0;
    priv->humidity = -1.0;
    priv->wind = -1;
    priv->windspeed = -1;
    priv->pressure = -1.0;
    priv->visibility = -1.0;
    priv->sunriseValid = FALSE;
    priv->sunsetValid = FALSE;
    priv->moonValid = FALSE;
    priv->sunrise = 0;
    priv->sunset = 0;
    priv->moonphase = 0;
    priv->moonlatitude = 0;
    priv->forecast_list = NULL;
    priv->radar = NULL;
}

static void
settings_changed_cb (GSettings    *settings,
		     const char   *key,
		     GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv = info->priv;

    /* Only emit the signal if no network requests are pending.
       Otherwise just wait for the update that will happen at
       the end
    */
    if (priv->requests_pending == NULL)
        g_signal_emit (info, gweather_info_signals[SIGNAL_UPDATED], 0);
}

void
gweather_info_init (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;

    priv = info->priv = G_TYPE_INSTANCE_GET_PRIVATE (info, GWEATHER_TYPE_INFO, GWeatherInfoPrivate);

    priv->providers = GWEATHER_PROVIDER_METAR | GWEATHER_PROVIDER_IWIN;
    priv->settings = g_settings_new ("org.gnome.GWeather");

    g_signal_connect_object (priv->settings, "changed",
			     G_CALLBACK (settings_changed_cb), info, 0);

    priv->radar_url = g_settings_get_string (priv->settings, RADAR_KEY);
    if (g_strcmp0 (priv->radar_url, "") == 0) {
	g_free (priv->radar_url);
	priv->radar_url = NULL;
    }

    gweather_info_reset (info);
}

static SoupCache *
get_cache (void)
{
    SoupCache *cache;
    char *filename;

    filename = g_build_filename (g_get_user_cache_dir (),
				 "libgweather", NULL);

    if (g_mkdir_with_parents (filename, 0700) < 0) {
	g_warning ("Failed to create libgweather cache directory: %s. Check your XDG_CACHE_HOME setting!", strerror (errno));
	g_free (filename);
	return NULL;
    }

    cache = soup_cache_new (filename, SOUP_CACHE_SINGLE_USER);

    g_free (filename);
    return cache;
}

static void
dump_and_unref_cache (SoupCache *cache)
{
    soup_cache_dump (cache);
    g_object_unref (cache);
}

static SoupSession *static_session;

static SoupSession *
ref_session (void)
{
    SoupSession *session;
    SoupCache *cache;

    session = static_session;

    if (session != NULL)
	return g_object_ref (session);

    session = soup_session_new ();
    g_object_set (G_OBJECT (session), SOUP_SESSION_USER_AGENT,
                  "libgweather/" LIBGWEATHER_VERSION " (+https://gitlab.gnome.org/GNOME/libgweather/) ", NULL);

    cache = get_cache ();
    if (cache != NULL) {
	soup_session_add_feature (session, SOUP_SESSION_FEATURE (cache));
	g_object_set_data_full (G_OBJECT (session), "libgweather-cache", g_object_ref (cache),
			        (GDestroyNotify) dump_and_unref_cache);

	soup_cache_load (cache);
	g_object_unref (cache);
    }

    static_session = session;
    g_object_add_weak_pointer (G_OBJECT (session), (void**) &static_session);

    return session;
}

/**
 * gweather_info_store_cache:
 *
 * Ensures that any data cached from the network is stored to disk.
 * Calling this is not necessary, as the cache will be saved when
 * the last reference to a #GWeatherInfo will be dropped.
 * On the other hand, it must be called if there is any chance that
 * the application will be closed without unreffing all objects, such
 * as when using a language binding that employs a GC.
 */
void
gweather_info_store_cache (void)
{
    SoupCache *cache;

    if (static_session == NULL)
	return;

    cache = g_object_get_data (G_OBJECT (static_session), "libgweather-cache");
    soup_cache_dump (cache);
}

/**
 * gweather_info_update:
 * @info: a #GWeatherInfo
 *
 * Requests a reload of weather conditions and forecast data from
 * enabled network services.
 * This call does no synchronous IO: rather, the result is delivered
 * by emitting the #GWeatherInfo::updated signal.
 * Note that if no network services are enabled, the signal will not
 * be emitted. See #GWeatherInfo:enabled-providers for details.
 */
void
gweather_info_update (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv = info->priv;
    gboolean ok;

    /* Update in progress */
    if (!requests_init (info))
        return ;

    gweather_info_reset (info);

    if (!priv->session)
	priv->session = ref_session ();

    if (priv->providers & GWEATHER_PROVIDER_METAR)
	metar_start_open (info);

    if (priv->radar) {
        wx_start_open (info);
    }

    ok = FALSE;
    /* Try national forecast services first */
    if (priv->providers & GWEATHER_PROVIDER_IWIN)
	ok = iwin_start_open (info);
    if (ok)
	return;

    /* Try yr.no next */
    if (priv->providers & GWEATHER_PROVIDER_YR_NO)
	ok = yrno_start_open (info);
    if (ok)
	return;

    /* Try OpenWeatherMap next */
    if (priv->providers & GWEATHER_PROVIDER_OWM)
	owm_start_open (info);
}

void
gweather_info_abort (GWeatherInfo *info)
{
    GSList *list, *iter;
    GSList dummy = { NULL, NULL };

    g_return_if_fail (GWEATHER_IS_INFO (info));

    if (info->priv->session == NULL) {
	g_assert (info->priv->requests_pending == NULL);
	return;
    }

    list = info->priv->requests_pending;
    /* to block updated signals */
    info->priv->requests_pending = &dummy;

    for (iter = list; iter; iter = iter->next)
	soup_session_cancel_message (info->priv->session, iter->data, SOUP_STATUS_CANCELLED);
    g_slist_free (list);

    info->priv->requests_pending = NULL;
}

static void
gweather_info_dispose (GObject *object)
{
    GWeatherInfo *info = GWEATHER_INFO (object);
    GWeatherInfoPrivate *priv = info->priv;

    gweather_info_abort (info);

    g_clear_object (&priv->session);

    free_forecast_list (info);

    if (priv->radar != NULL) {
        g_object_unref (priv->radar);
        priv->radar = NULL;
    }

    priv->valid = FALSE;

    G_OBJECT_CLASS (gweather_info_parent_class)->dispose (object);
}

static void
gweather_info_finalize (GObject *object)
{
    GWeatherInfo *info = GWEATHER_INFO (object);
    GWeatherInfoPrivate *priv = info->priv;

    _weather_location_free (&priv->location);
    g_clear_object (&priv->settings);

    if (priv->glocation)
	gweather_location_unref (priv->glocation);

    g_free (priv->radar_url);
    priv->radar_url = NULL;

    g_free (priv->forecast_attribution);

    G_OBJECT_CLASS (gweather_info_parent_class)->finalize (object);
}

gboolean
gweather_info_is_valid (GWeatherInfo *info)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    return info->priv->valid;
}

gboolean
gweather_info_network_error (GWeatherInfo *info)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    return info->priv->network_error;
}

const GWeatherLocation *
gweather_info_get_location (GWeatherInfo *info)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);
    return info->priv->glocation;
}

gchar *
gweather_info_get_location_name (GWeatherInfo *info)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);

    return g_strdup(info->priv->location.name);
}

gchar *
gweather_info_get_update (GWeatherInfo *info)
{
    char *out;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);

    if (!info->priv->valid)
        return g_strdup ("-");

    if (info->priv->update != 0) {
	GDateTime *now = g_date_time_new_from_unix_local (info->priv->update);

	out = g_date_time_format (now, _("%a, %b %d / %H∶%M"));
	if (!out)
	    out = g_strdup ("???");

	g_date_time_unref (now);
    } else
        out = g_strdup (_("Unknown observation time"));

    return out;
}

gchar *
gweather_info_get_sky (GWeatherInfo *info)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);
    if (!info->priv->valid)
        return g_strdup("-");
    if (info->priv->sky < 0)
	return g_strdup(C_("sky conditions", "Unknown"));
    return g_strdup(gweather_sky_to_string (info->priv->sky));
}

gchar *
gweather_info_get_conditions (GWeatherInfo *info)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);
    if (!info->priv->valid)
        return g_strdup("-");
    return g_strdup(gweather_conditions_to_string (&info->priv->cond));
}

static gboolean
is_locale_metric (void)
{
    /* Translate to the default units to use for presenting
     * lengths to the user. Translate to default:inch if you
     * want inches, otherwise translate to default:mm.
     * Do *not* translate it to "predefinito:mm", if it
     * it isn't default:mm or default:inch it will not work
     */
    gchar *e = _("default:mm");

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
    const char *fmt;
    fmt = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);

    if (fmt && *fmt == 2)
	return FALSE;
    else
	return TRUE;
#endif

    if (strcmp (e, "default:inch")==0)
        return FALSE;
    else if (strcmp (e, "default:mm"))
        g_warning ("Whoever translated default:mm did so wrongly.\n");

    return TRUE;
}

/**
 * gweather_temperature_unit_to_real:
 * @unit: a tempeature unit, or %GWEATHER_TEMP_UNIT_DEFAULT
 *
 * Resolve @unit into a real temperature unit, potentially considering
 * locale defaults.
 */
GWeatherTemperatureUnit
gweather_temperature_unit_to_real (GWeatherTemperatureUnit unit)
{
    if (G_UNLIKELY (unit == GWEATHER_TEMP_UNIT_INVALID)) {
	g_critical("Conversion to invalid temperature unit");
	unit = GWEATHER_TEMP_UNIT_DEFAULT;
    }

    if (unit == GWEATHER_TEMP_UNIT_DEFAULT)
	return is_locale_metric() ?
	    GWEATHER_TEMP_UNIT_CENTIGRADE :
	    GWEATHER_TEMP_UNIT_FAHRENHEIT;

    return unit;
}

static gchar *
temperature_string (gfloat temp_f, GWeatherTemperatureUnit to_unit, gboolean want_round)
{
    to_unit = gweather_temperature_unit_to_real (to_unit);

    switch (to_unit) {
    case GWEATHER_TEMP_UNIT_FAHRENHEIT:
	if (!want_round) {
	    /* TRANSLATOR: This is the temperature in degrees Fahrenheit (U+2109 DEGREE FAHRENHEIT)
	     * with a non-break space (U+00A0) between the digits and the degrees sign */
	    return g_strdup_printf (_("%.1f\u00A0\u2109"), temp_f);
	} else {
	    /* TRANSLATOR: This is the temperature in degrees Fahrenheit (U+2109 DEGREE FAHRENHEIT)i
	     * with a non-break space (U+00A0) between the digits and the degrees sign */
	    return g_strdup_printf (_("%d\u00A0\u2109"), (int)floor (temp_f + 0.5));
	}
	break;
    case GWEATHER_TEMP_UNIT_CENTIGRADE:
	if (!want_round) {
	    /* TRANSLATOR: This is the temperature in degrees Celsius (U+2103 DEGREE CELSIUS)
	     * with a non-break space (U+00A0) between the digits and the degrees sign */
	    return g_strdup_printf (_("%.1f\u00A0\u2103"), TEMP_F_TO_C (temp_f));
	} else {
	    /* TRANSLATOR: This is the temperature in degrees Celsius (U+2103 DEGREE CELSIUS)
	     * with a non-break space (U+00A0) between the digits and the degrees sign */
	    return g_strdup_printf (_("%d\u00A0\u2103"), (int)floor (TEMP_F_TO_C (temp_f) + 0.5));
	}
	break;
    case GWEATHER_TEMP_UNIT_KELVIN:
	if (!want_round) {
	    /* TRANSLATOR: This is the temperature in kelvin (U+212A KELVIN SIGN)
	     * with a non-break space (U+00A0) between the digits and the degrees sign */
	    return g_strdup_printf (_("%.1f\u00A0\u212A"), TEMP_F_TO_K (temp_f));
	} else {
	    /* TRANSLATOR: This is the temperature in kelvin (U+212A KELVIN SIGN)
	     * with a non-break space (U+00A0) between the digits and the degrees sign */
	    return g_strdup_printf (_("%d\u00A0\u212A"), (int)floor (TEMP_F_TO_K (temp_f)));
	}
	break;

    case GWEATHER_TEMP_UNIT_INVALID:
    case GWEATHER_TEMP_UNIT_DEFAULT:
	g_assert_not_reached ();
    }

    return NULL;
}

gchar *
gweather_info_get_temp (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);
    priv = info->priv;

    if (!priv->valid)
        return g_strdup("-");
    if (priv->temp < -500.0)
        return g_strdup(C_("temperature", "Unknown"));

    return temperature_string (priv->temp, g_settings_get_enum (priv->settings, TEMPERATURE_UNIT), FALSE);
}

gchar *
gweather_info_get_temp_min (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);
    priv = info->priv;

    if (!priv->valid || !priv->tempMinMaxValid)
        return g_strdup("-");
    if (priv->temp_min < -500.0)
        return g_strdup(C_("temperature", "Unknown"));

    return temperature_string (priv->temp_min, g_settings_get_enum (priv->settings, TEMPERATURE_UNIT), FALSE);
}

gchar *
gweather_info_get_temp_max (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);
    priv = info->priv;

    if (!priv->valid || !priv->tempMinMaxValid)
        return g_strdup("-");
    if (priv->temp_max < -500.0)
        return g_strdup(C_("temperature", "Unknown"));

    return temperature_string (priv->temp_max, g_settings_get_enum (priv->settings, TEMPERATURE_UNIT), FALSE);
}

gchar *
gweather_info_get_dew (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    gdouble dew;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);
    priv = info->priv;

    if (!priv->valid)
        return g_strdup("-");

    if (priv->hasHumidity)
	dew = calc_dew (priv->temp, priv->humidity);
    else
	dew = priv->dew;
    if (dew < -500.0)
        return g_strdup(C_("dew", "Unknown"));

    return temperature_string (priv->dew, g_settings_get_enum (priv->settings, TEMPERATURE_UNIT), FALSE);
}

gchar *
gweather_info_get_humidity (GWeatherInfo *info)
{
    gdouble humidity;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);

    if (!info->priv->valid)
        return g_strdup("-");

    if (info->priv->hasHumidity)
	humidity = info->priv->humidity;
    else
	humidity = calc_humidity (info->priv->temp, info->priv->dew);
    if (humidity < 0.0)
        return g_strdup(C_("humidity", "Unknown"));

    /* TRANSLATOR: This is the humidity in percent */
    return g_strdup_printf(_("%.f%%"), humidity);
}

gchar *
gweather_info_get_apparent (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    gdouble apparent;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);
    priv = info->priv;

    if (!priv->valid)
        return g_strdup("-");

    apparent = calc_apparent (info);
    if (apparent < -500.0)
        return g_strdup(C_("temperature", "Unknown"));

    return temperature_string (apparent, g_settings_get_enum (priv->settings, TEMPERATURE_UNIT), FALSE);
}

static GWeatherSpeedUnit
speed_unit_to_real (GWeatherSpeedUnit unit)
{
    if (G_UNLIKELY (unit == GWEATHER_SPEED_UNIT_INVALID)) {
	g_critical("Conversion to invalid speed unit");
	unit = GWEATHER_SPEED_UNIT_DEFAULT;
    }

    if (unit == GWEATHER_SPEED_UNIT_DEFAULT)
	return is_locale_metric() ?
	    GWEATHER_SPEED_UNIT_KPH :
	    GWEATHER_SPEED_UNIT_KNOTS;

    return unit;
}

static gchar *
windspeed_string (gfloat knots, GWeatherSpeedUnit to_unit)
{
    to_unit = speed_unit_to_real (to_unit);

    switch (to_unit) {
    case GWEATHER_SPEED_UNIT_KNOTS:
	/* TRANSLATOR: This is the wind speed in knots */
	return g_strdup_printf(_("%0.1f knots"), knots);
    case GWEATHER_SPEED_UNIT_MPH:
	/* TRANSLATOR: This is the wind speed in miles per hour */
	return g_strdup_printf(_("%.1f mph"), WINDSPEED_KNOTS_TO_MPH (knots));
    case GWEATHER_SPEED_UNIT_KPH:
	/* TRANSLATOR: This is the wind speed in kilometers per hour */
	return g_strdup_printf(_("%.1f km/h"), WINDSPEED_KNOTS_TO_KPH (knots));
    case GWEATHER_SPEED_UNIT_MS:
	/* TRANSLATOR: This is the wind speed in meters per second */
	return g_strdup_printf(_("%.1f m/s"), WINDSPEED_KNOTS_TO_MS (knots));
    case GWEATHER_SPEED_UNIT_BFT:
	/* TRANSLATOR: This is the wind speed as a Beaufort force factor
	 * (commonly used in nautical wind estimation).
	 */
	return g_strdup_printf(_("Beaufort force %.1f"), WINDSPEED_KNOTS_TO_BFT (knots));
    case GWEATHER_SPEED_UNIT_INVALID:
    case GWEATHER_SPEED_UNIT_DEFAULT:
	g_assert_not_reached ();
    }

    return NULL;
}

gchar *
gweather_info_get_wind (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);

    priv = info->priv;

    if (!priv->valid)
        return g_strdup("-");
    if (priv->windspeed < 0.0 || priv->wind < 0)
        return g_strdup(C_("wind speed", "Unknown"));
    if (priv->windspeed == 0.00) {
        return g_strdup(_("Calm"));
    } else {
	gchar *speed_string;
	gchar *wind_string;

	speed_string = windspeed_string (priv->windspeed, g_settings_get_enum (priv->settings, SPEED_UNIT));

        /* TRANSLATOR: This is 'wind direction' / 'wind speed' */
        wind_string = g_strdup_printf (_("%s / %s"), gweather_wind_direction_to_string (priv->wind), speed_string);

	g_free (speed_string);
	return wind_string;
    }
}

static GWeatherPressureUnit
pressure_unit_to_real (GWeatherPressureUnit unit)
{
    if (G_UNLIKELY (unit == GWEATHER_PRESSURE_UNIT_INVALID)) {
	g_critical("Conversion to invalid pressure unit");
	unit = GWEATHER_PRESSURE_UNIT_DEFAULT;
    }

    if (unit == GWEATHER_PRESSURE_UNIT_DEFAULT)
	return is_locale_metric() ?
	    GWEATHER_PRESSURE_UNIT_MM_HG :
	    GWEATHER_PRESSURE_UNIT_INCH_HG;

    return unit;
}

gchar *
gweather_info_get_pressure (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    GWeatherPressureUnit unit;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);

    priv = info->priv;

    if (!priv->valid)
        return g_strdup("-");
    if (priv->pressure < 0.0)
        return g_strdup(C_("pressure", "Unknown"));

    unit = pressure_unit_to_real (g_settings_get_enum (priv->settings, PRESSURE_UNIT));
    switch (unit) {
    case GWEATHER_PRESSURE_UNIT_INCH_HG:
	/* TRANSLATOR: This is pressure in inches of mercury */
	return g_strdup_printf(_("%.2f inHg"), priv->pressure);
    case GWEATHER_PRESSURE_UNIT_MM_HG:
	/* TRANSLATOR: This is pressure in millimeters of mercury */
	return g_strdup_printf(_("%.1f mmHg"), PRESSURE_INCH_TO_MM (priv->pressure));
    case GWEATHER_PRESSURE_UNIT_KPA:
	/* TRANSLATOR: This is pressure in kiloPascals */
	return g_strdup_printf(_("%.2f kPa"), PRESSURE_INCH_TO_KPA (priv->pressure));
    case GWEATHER_PRESSURE_UNIT_HPA:
	/* TRANSLATOR: This is pressure in hectoPascals */
	return g_strdup_printf(_("%.2f hPa"), PRESSURE_INCH_TO_HPA (priv->pressure));
    case GWEATHER_PRESSURE_UNIT_MB:
	/* TRANSLATOR: This is pressure in millibars */
	return g_strdup_printf(_("%.2f mb"), PRESSURE_INCH_TO_MB (priv->pressure));
    case GWEATHER_PRESSURE_UNIT_ATM:
	/* TRANSLATOR: This is pressure in atmospheres */
	return g_strdup_printf(_("%.3f atm"), PRESSURE_INCH_TO_ATM (priv->pressure));

    case GWEATHER_PRESSURE_UNIT_INVALID:
    case GWEATHER_PRESSURE_UNIT_DEFAULT:
	g_assert_not_reached ();
    }

    return NULL;
}

static GWeatherDistanceUnit
distance_unit_to_real (GWeatherDistanceUnit unit)
{
    if (G_UNLIKELY (unit == GWEATHER_DISTANCE_UNIT_INVALID)) {
	g_critical("Conversion to invalid distance unit");
	unit = GWEATHER_DISTANCE_UNIT_DEFAULT;
    }

    if (unit == GWEATHER_DISTANCE_UNIT_DEFAULT)
	return is_locale_metric() ?
	    GWEATHER_DISTANCE_UNIT_METERS :
	    GWEATHER_DISTANCE_UNIT_MILES;

    return unit;
}

gchar *
gweather_info_get_visibility (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    GWeatherDistanceUnit unit;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);
    priv = info->priv;

    if (!priv->valid)
        return g_strdup ("-");
    if (priv->visibility < 0.0)
        return g_strdup (C_("visibility", "Unknown"));

    unit = distance_unit_to_real (g_settings_get_enum (priv->settings, DISTANCE_UNIT));
    switch (unit) {
    case GWEATHER_DISTANCE_UNIT_MILES:
	/* TRANSLATOR: This is the visibility in miles */
	return g_strdup_printf (_("%.1f miles"), priv->visibility);
    case GWEATHER_DISTANCE_UNIT_KM:
	/* TRANSLATOR: This is the visibility in kilometers */
	return g_strdup_printf (_("%.1f km"), VISIBILITY_SM_TO_KM (priv->visibility));
    case GWEATHER_DISTANCE_UNIT_METERS:
	/* TRANSLATOR: This is the visibility in meters */
	return g_strdup_printf (_("%.0fm"), VISIBILITY_SM_TO_M (priv->visibility));

    case GWEATHER_DISTANCE_UNIT_INVALID:
    case GWEATHER_DISTANCE_UNIT_DEFAULT:
	g_assert_not_reached ();
    }

    return NULL;
}

gchar *
gweather_info_get_sunrise (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    GDateTime *sunrise;
    gchar *buf;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);

    priv = info->priv;

    _gweather_info_ensure_sun (info);

    if (!priv->sunriseValid)
        return g_strdup ("-");

    sunrise = g_date_time_new_from_unix_local (priv->sunrise);

    buf = g_date_time_format (sunrise, _("%H∶%M"));
    if (!buf)
        buf = g_strdup ("-");

    g_date_time_unref (sunrise);
    return buf;
}

gchar *
gweather_info_get_sunset (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    GDateTime *sunset;
    gchar *buf;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);

    priv = info->priv;

    _gweather_info_ensure_sun (info);

    if (!priv->sunsetValid)
        return g_strdup ("-");

    sunset = g_date_time_new_from_unix_local (priv->sunset);
    buf = g_date_time_format (sunset, _("%H∶%M"));
    if (!buf)
        buf = g_strdup ("-");

    g_date_time_unref (sunset);
    return buf;
}

/**
 * gweather_info_get_forecast_list:
 * @info: a #GWeatherInfo
 *
 * Returns: (transfer none) (element-type GWeather.Info): list
 * of GWeatherInfo* objects for the forecast.
 * The list is owned by the 'info' object thus is alive as long
 * as the 'info'. The 'update' property is the date/time when the
 * forecast info is used for.
 **/
GSList *
gweather_info_get_forecast_list (GWeatherInfo *info)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);

    if (!info->priv->valid)
	return NULL;

    return info->priv->forecast_list;
}

/**
 * gweather_info_get_radar:
 * @info: a #GWeatherInfo
 *
 * Returns: (transfer none): what?
 */
GdkPixbufAnimation *
gweather_info_get_radar (GWeatherInfo *info)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);
    return info->priv->radar;
}

/**
 * gweather_info_get_attribution:
 * @info: a #GWeatherInfo
 *
 * Some weather services require the application showing the
 * data to include an attribution text, possibly including links
 * to the service website.
 * This must be shown prominently toghether with the data.
 *
 * Returns: (transfer none): the required attribution text, in Pango
 *          markup form, or %NULL if not required
 */
const gchar *
gweather_info_get_attribution (GWeatherInfo *info)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);

    return info->priv->forecast_attribution;
}

gchar *
gweather_info_get_temp_summary (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);

    priv = info->priv;

    if (!priv->valid || priv->temp < -500.0)
        return g_strdup ("--");

    return temperature_string (priv->temp, g_settings_get_enum (priv->settings, TEMPERATURE_UNIT), TRUE);
}

/**
 * gweather_info_get_weather_summary:
 * @info: a #GWeatherInfo
 *
 * Returns: (transfer full): a summary for current weather conditions.
 */
gchar *
gweather_info_get_weather_summary (GWeatherInfo *info)
{
    gchar *buf;
    gchar *out;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);

    if (!info->priv->valid)
	return g_strdup (_("Retrieval failed"));
    buf = gweather_info_get_conditions (info);
    if (g_str_equal (buf, "-")) {
	g_free (buf);
        buf = gweather_info_get_sky (info);
    }

    out = g_strdup_printf ("%s: %s", gweather_info_get_location_name (info), buf);

    g_free (buf);
    return out;
}

/**
 * gweather_info_is_daytime:
 * @info: a #GWeatherInfo
 *
 * Returns: Whether it is daytime (that is, if the sun is visible)
 *   or not at the location and the point of time referred by @info.
 *   This is mostly equivalent to comparing the return value
 *   of gweather_info_get_value_sunrise() and
 *   gweather_info_get_value_sunset(), but it accounts also
 *   for midnight sun and polar night, for locations within
 *   the Artic and Antartic circles.
 */
gboolean
gweather_info_is_daytime (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    time_t current_time;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);

    priv = info->priv;

    _gweather_info_ensure_sun (info);

    if (priv->polarNight)
	return FALSE;
    if (priv->midnightSun)
	return TRUE;

    current_time = priv->current_time;
    return ( !priv->sunriseValid || (current_time >= priv->sunrise) ) &&
	( !priv->sunsetValid || (current_time < priv->sunset) );
}

const gchar *
gweather_info_get_icon_name (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    GWeatherConditions   cond;
    GWeatherSky          sky;
    gboolean             daytime;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);

    priv = info->priv;

    cond = priv->cond;
    sky = priv->sky;

    if (cond.significant) {
	if (cond.phenomenon != GWEATHER_PHENOMENON_NONE &&
	    cond.qualifier == GWEATHER_QUALIFIER_THUNDERSTORM)
            return "weather-storm";

        switch (cond.phenomenon) {
	case GWEATHER_PHENOMENON_INVALID:
	case GWEATHER_PHENOMENON_LAST:
	case GWEATHER_PHENOMENON_NONE:
	    break;

	case GWEATHER_PHENOMENON_DRIZZLE:
	case GWEATHER_PHENOMENON_RAIN:
	case GWEATHER_PHENOMENON_UNKNOWN_PRECIPITATION:
	case GWEATHER_PHENOMENON_HAIL:
	case GWEATHER_PHENOMENON_SMALL_HAIL:
	    return "weather-showers";

	case GWEATHER_PHENOMENON_SNOW:
	case GWEATHER_PHENOMENON_SNOW_GRAINS:
	case GWEATHER_PHENOMENON_ICE_PELLETS:
	case GWEATHER_PHENOMENON_ICE_CRYSTALS:
	    return "weather-snow";

	case GWEATHER_PHENOMENON_TORNADO:
	case GWEATHER_PHENOMENON_SQUALL:
	    return "weather-storm";

	case GWEATHER_PHENOMENON_MIST:
	case GWEATHER_PHENOMENON_FOG:
	case GWEATHER_PHENOMENON_SMOKE:
	case GWEATHER_PHENOMENON_VOLCANIC_ASH:
	case GWEATHER_PHENOMENON_SAND:
	case GWEATHER_PHENOMENON_HAZE:
	case GWEATHER_PHENOMENON_SPRAY:
	case GWEATHER_PHENOMENON_DUST:
	case GWEATHER_PHENOMENON_SANDSTORM:
	case GWEATHER_PHENOMENON_DUSTSTORM:
	case GWEATHER_PHENOMENON_FUNNEL_CLOUD:
	case GWEATHER_PHENOMENON_DUST_WHIRLS:
	    return "weather-fog";
        }
    }

    daytime = gweather_info_is_daytime (info);

    switch (sky) {
    case GWEATHER_SKY_INVALID:
    case GWEATHER_SKY_LAST:
    case GWEATHER_SKY_CLEAR:
	if (daytime)
	    return "weather-clear";
	else
	    return "weather-clear-night";

    case GWEATHER_SKY_BROKEN:
    case GWEATHER_SKY_SCATTERED:
    case GWEATHER_SKY_FEW:
	if (daytime)
	    return "weather-few-clouds";
	else
	    return "weather-few-clouds-night";

    case GWEATHER_SKY_OVERCAST:
	return "weather-overcast";

    default: /* unrecognized */
	return NULL;
    }
}

const gchar *
gweather_info_get_symbolic_icon_name (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    GWeatherConditions   cond;
    GWeatherSky          sky;
    gboolean             daytime;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), NULL);

    priv = info->priv;

    cond = priv->cond;
    sky = priv->sky;

    if (cond.significant) {
	if (cond.phenomenon != GWEATHER_PHENOMENON_NONE &&
	    cond.qualifier == GWEATHER_QUALIFIER_THUNDERSTORM)
            return "weather-storm-symbolic";

        switch (cond.phenomenon) {
	case GWEATHER_PHENOMENON_INVALID:
	case GWEATHER_PHENOMENON_LAST:
	case GWEATHER_PHENOMENON_NONE:
	    break;

	case GWEATHER_PHENOMENON_DRIZZLE:
	case GWEATHER_PHENOMENON_RAIN:
	case GWEATHER_PHENOMENON_UNKNOWN_PRECIPITATION:
	case GWEATHER_PHENOMENON_HAIL:
	case GWEATHER_PHENOMENON_SMALL_HAIL:
	    return "weather-showers-symbolic";

	case GWEATHER_PHENOMENON_SNOW:
	case GWEATHER_PHENOMENON_SNOW_GRAINS:
	case GWEATHER_PHENOMENON_ICE_PELLETS:
	case GWEATHER_PHENOMENON_ICE_CRYSTALS:
	    return "weather-snow-symbolic";

	case GWEATHER_PHENOMENON_TORNADO:
	case GWEATHER_PHENOMENON_SQUALL:
	    return "weather-storm-symbolic";

	case GWEATHER_PHENOMENON_MIST:
	case GWEATHER_PHENOMENON_FOG:
	case GWEATHER_PHENOMENON_SMOKE:
	case GWEATHER_PHENOMENON_VOLCANIC_ASH:
	case GWEATHER_PHENOMENON_SAND:
	case GWEATHER_PHENOMENON_HAZE:
	case GWEATHER_PHENOMENON_SPRAY:
	case GWEATHER_PHENOMENON_DUST:
	case GWEATHER_PHENOMENON_SANDSTORM:
	case GWEATHER_PHENOMENON_DUSTSTORM:
	case GWEATHER_PHENOMENON_FUNNEL_CLOUD:
	case GWEATHER_PHENOMENON_DUST_WHIRLS:
	    return "weather-fog-symbolic";
        }
    }

    daytime = gweather_info_is_daytime (info);

    switch (sky) {
    case GWEATHER_SKY_INVALID:
    case GWEATHER_SKY_LAST:
    case GWEATHER_SKY_CLEAR:
	if (daytime)
	    return "weather-clear-symbolic";
	else
	    return "weather-clear-night-symbolic";

    case GWEATHER_SKY_BROKEN:
    case GWEATHER_SKY_SCATTERED:
    case GWEATHER_SKY_FEW:
	if (daytime)
	    return "weather-few-clouds-symbolic";
	else
	    return "weather-few-clouds-night-symbolic";

    case GWEATHER_SKY_OVERCAST:
	return "weather-overcast-symbolic";

    default: /* unrecognized */
	return NULL;
    }
}

static gboolean
temperature_value (gdouble temp_f,
		   GWeatherTemperatureUnit to_unit,
		   gdouble *value,
		   GSettings *settings)
{
    gboolean ok = TRUE;

    *value = 0.0;
    if (temp_f < -500.0)
	return FALSE;

    if (to_unit == GWEATHER_TEMP_UNIT_DEFAULT)
	    to_unit = g_settings_get_enum (settings, TEMPERATURE_UNIT);
    to_unit = gweather_temperature_unit_to_real (to_unit);

    switch (to_unit) {
        case GWEATHER_TEMP_UNIT_FAHRENHEIT:
	    *value = temp_f;
	    break;
        case GWEATHER_TEMP_UNIT_CENTIGRADE:
	    *value = TEMP_F_TO_C (temp_f);
	    break;
        case GWEATHER_TEMP_UNIT_KELVIN:
	    *value = TEMP_F_TO_K (temp_f);
	    break;
        case GWEATHER_TEMP_UNIT_INVALID:
        case GWEATHER_TEMP_UNIT_DEFAULT:
	    g_assert_not_reached ();
    }

    return ok;
}

static gboolean
speed_value (gdouble            knots,
	     GWeatherSpeedUnit  to_unit,
	     gdouble           *value,
	     GSettings         *settings)
{
    gboolean ok = TRUE;

    *value = -1.0;

    if (knots < 0.0)
	return FALSE;

    if (to_unit == GWEATHER_SPEED_UNIT_DEFAULT)
	    to_unit = g_settings_get_enum (settings, SPEED_UNIT);
    to_unit = speed_unit_to_real (to_unit);

    switch (to_unit) {
        case GWEATHER_SPEED_UNIT_KNOTS:
            *value = knots;
	    break;
        case GWEATHER_SPEED_UNIT_MPH:
            *value = WINDSPEED_KNOTS_TO_MPH (knots);
	    break;
        case GWEATHER_SPEED_UNIT_KPH:
            *value = WINDSPEED_KNOTS_TO_KPH (knots);
	    break;
        case GWEATHER_SPEED_UNIT_MS:
            *value = WINDSPEED_KNOTS_TO_MS (knots);
	    break;
	case GWEATHER_SPEED_UNIT_BFT:
	    *value = WINDSPEED_KNOTS_TO_BFT (knots);
	    break;
        case GWEATHER_SPEED_UNIT_INVALID:
        case GWEATHER_SPEED_UNIT_DEFAULT:
	    g_assert_not_reached ();
    }

    return ok;
}

static gboolean
pressure_value (gdouble               inHg,
		GWeatherPressureUnit  to_unit,
		gdouble              *value,
		GSettings            *settings)
{
    gboolean ok = TRUE;

    *value = -1.0;

    if (inHg < 0.0)
	return FALSE;

    if (to_unit == GWEATHER_PRESSURE_UNIT_DEFAULT)
	    to_unit = g_settings_get_enum (settings, PRESSURE_UNIT);
    to_unit = pressure_unit_to_real (to_unit);

    switch (to_unit) {
        case GWEATHER_PRESSURE_UNIT_INCH_HG:
            *value = inHg;
	    break;
        case GWEATHER_PRESSURE_UNIT_MM_HG:
            *value = PRESSURE_INCH_TO_MM (inHg);
	    break;
        case GWEATHER_PRESSURE_UNIT_KPA:
            *value = PRESSURE_INCH_TO_KPA (inHg);
	    break;
        case GWEATHER_PRESSURE_UNIT_HPA:
            *value = PRESSURE_INCH_TO_HPA (inHg);
	    break;
        case GWEATHER_PRESSURE_UNIT_MB:
            *value = PRESSURE_INCH_TO_MB (inHg);
	    break;
        case GWEATHER_PRESSURE_UNIT_ATM:
            *value = PRESSURE_INCH_TO_ATM (inHg);
	    break;
        case GWEATHER_PRESSURE_UNIT_INVALID:
        case GWEATHER_PRESSURE_UNIT_DEFAULT:
	    g_assert_not_reached ();
    }

    return ok;
}

static gboolean
distance_value (gdouble               miles,
		GWeatherDistanceUnit  to_unit,
		gdouble              *value,
		GSettings            *settings)
{
    gboolean ok = TRUE;

    *value = -1.0;

    if (miles < 0.0)
	return FALSE;

    if (to_unit == GWEATHER_DISTANCE_UNIT_DEFAULT)
	    to_unit = g_settings_get_enum (settings, DISTANCE_UNIT);
    to_unit = distance_unit_to_real (to_unit);

    switch (to_unit) {
        case GWEATHER_DISTANCE_UNIT_MILES:
            *value = miles;
            break;
        case GWEATHER_DISTANCE_UNIT_KM:
            *value = VISIBILITY_SM_TO_KM (miles);
            break;
        case GWEATHER_DISTANCE_UNIT_METERS:
            *value = VISIBILITY_SM_TO_M (miles);
            break;
        case GWEATHER_DISTANCE_UNIT_INVALID:
        case GWEATHER_DISTANCE_UNIT_DEFAULT:
	    g_assert_not_reached ();
    }

    return ok;
}

/**
 * gweather_info_get_value_sky:
 * @info: a #GWeatherInfo
 * @sky: (out): a location for a #GWeatherSky.
 *
 * Fills out @sky with current sky conditions.
 * Returns: TRUE is @sky is valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_sky (GWeatherInfo *info, GWeatherSky *sky)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (sky != NULL, FALSE);

    if (!info->priv->valid)
	return FALSE;

    if (info->priv->sky <= GWEATHER_SKY_INVALID || info->priv->sky >= GWEATHER_SKY_LAST)
	return FALSE;

    *sky = info->priv->sky;

    return TRUE;
}

/**
 * gweather_info_get_value_conditions:
 * @info: a #GWeatherInfo
 * @phenomenon: (out): a location for a #GWeatherConditionPhenomenon.
 * @qualifier: (out): a location for a #GWeatherConditionQualifier.
 *
 * Fills out @phenomenon and @qualifier with current weather conditions.
 * Returns: TRUE is out arguments are valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_conditions (GWeatherInfo *info, GWeatherConditionPhenomenon *phenomenon, GWeatherConditionQualifier *qualifier)
{
    GWeatherInfoPrivate *priv;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (phenomenon != NULL, FALSE);
    g_return_val_if_fail (qualifier != NULL, FALSE);

    priv = info->priv;

    if (!priv->valid)
	return FALSE;

    if (!priv->cond.significant)
	return FALSE;

    if (!(priv->cond.phenomenon > GWEATHER_PHENOMENON_INVALID &&
	  priv->cond.phenomenon < GWEATHER_PHENOMENON_LAST &&
	  priv->cond.qualifier > GWEATHER_QUALIFIER_INVALID &&
	  priv->cond.qualifier < GWEATHER_QUALIFIER_LAST))
        return FALSE;

    *phenomenon = priv->cond.phenomenon;
    *qualifier = priv->cond.qualifier;

    return TRUE;
}

/**
 * gweather_info_get_value_temp:
 * @info: a #GWeatherInfo
 * @unit: the desired unit, as a #GWeatherTemperatureUnit
 * @value: (out): the temperature value
 *
 * Returns: TRUE is @value is valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_temp (GWeatherInfo *info, GWeatherTemperatureUnit unit, gdouble *value)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (value != NULL, FALSE);

    if (!info->priv->valid)
	return FALSE;

    return temperature_value (info->priv->temp, unit, value, info->priv->settings);
}

/**
 * gweather_info_get_value_temp_min:
 * @info: a #GWeatherInfo
 * @unit: the desired unit, as a #GWeatherTemperatureUnit
 * @value: (out): the minimum temperature value
 *
 * Returns: TRUE is @value is valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_temp_min (GWeatherInfo *info, GWeatherTemperatureUnit unit, gdouble *value)
{
    GWeatherInfoPrivate *priv;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (value != NULL, FALSE);

    priv = info->priv;

    if (!priv->valid || !priv->tempMinMaxValid)
	return FALSE;

    return temperature_value (priv->temp_min, unit, value, priv->settings);
}

/**
 * gweather_info_get_value_temp_max:
 * @info: a #GWeatherInfo
 * @unit: the desired unit, as a #GWeatherTemperatureUnit
 * @value: (out): the maximum temperature value
 *
 * Returns: TRUE is @value is valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_temp_max (GWeatherInfo *info, GWeatherTemperatureUnit unit, gdouble *value)
{
    GWeatherInfoPrivate *priv;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (value != NULL, FALSE);

    priv = info->priv;

    if (!priv->valid || !priv->tempMinMaxValid)
	return FALSE;

    return temperature_value (priv->temp_max, unit, value, priv->settings);
}

/**
 * gweather_info_get_value_dew:
 * @info: a #GWeatherInfo
 * @unit: the desired unit, as a #GWeatherTemperatureUnit
 * @value: (out): the dew point
 *
 * Returns: TRUE is @value is valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_dew (GWeatherInfo *info, GWeatherTemperatureUnit unit, gdouble *value)
{
    gdouble dew;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (value != NULL, FALSE);

    if (!info->priv->valid)
	return FALSE;

    if (info->priv->hasHumidity)
	dew = calc_dew (info->priv->temp, info->priv->humidity);
    else
	dew = info->priv->dew;

    return temperature_value (dew, unit, value, info->priv->settings);
}

/**
 * gweather_info_get_value_apparent:
 * @info: a #GWeatherInfo
 * @unit: the desired unit, as a #GWeatherTemperatureUnit
 * @value: (out): the apparent temperature
 *
 * Returns: TRUE is @value is valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_apparent (GWeatherInfo *info, GWeatherTemperatureUnit unit, gdouble *value)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (value != NULL, FALSE);

    if (!info->priv->valid)
	return FALSE;

    return temperature_value (calc_apparent (info), unit, value, info->priv->settings);
}

/**
 * gweather_info_get_value_update:
 * @info: a #GWeatherInfo
 * @value: (out) (type glong): the time @info was last updated
 *
 * Note that @value may be 0 if @info has not yet been updated.
 *
 * Returns: TRUE is @value is valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_update (GWeatherInfo *info, time_t *value)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (value != NULL, FALSE);

    if (!info->priv->valid)
	return FALSE;

    *value = info->priv->update;

    return TRUE;
}

/**
 * gweather_info_get_value_sunrise:
 * @info: a #GWeatherInfo
 * @value: (out) (type gulong): the time of sunrise
 *
 * Returns: TRUE is @value is valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_sunrise (GWeatherInfo *info, time_t *value)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (value != NULL, FALSE);

    _gweather_info_ensure_sun (info);

    if (!info->priv->sunriseValid)
	return FALSE;

    *value = info->priv->sunrise;

    return TRUE;
}

/**
 * gweather_info_get_value_sunset:
 * @info: a #GWeatherInfo
 * @value: (out) (type gulong): the time of sunset
 *
 * Returns: TRUE is @value is valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_sunset (GWeatherInfo *info, time_t *value)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (value != NULL, FALSE);

    _gweather_info_ensure_sun (info);

    if (!info->priv->sunsetValid)
	return FALSE;

    *value = info->priv->sunset;

    return TRUE;
}

/**
 * gweather_info_get_value_moonphase:
 * @info: a #GWeatherInfo
 * @value: (out): the current moon phase
 * @lat: (out): the moon declension
 *
 * Returns: TRUE is @value is valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_moonphase (GWeatherInfo      *info,
				   GWeatherMoonPhase *value,
				   GWeatherMoonLatitude *lat)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (value != NULL, FALSE);
    g_return_val_if_fail (lat != NULL, FALSE);

    _gweather_info_ensure_moon (info);

    if (!info->priv->moonValid)
	return FALSE;

    *value = info->priv->moonphase;
    *lat   = info->priv->moonlatitude;

    return TRUE;
}

/**
 * gweather_info_get_value_wind:
 * @info: a #GWeatherInfo
 * @unit: the desired unit, as a #GWeatherSpeedUnit
 * @speed: (out): forecasted wind speed
 * @direction: (out): forecasted wind direction
 *
 * Returns: TRUE if @speed and @direction are valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_wind (GWeatherInfo *info,
			      GWeatherSpeedUnit unit,
			      gdouble *speed,
			      GWeatherWindDirection *direction)
{
    GWeatherInfoPrivate *priv;
    gboolean res = FALSE;

    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (speed != NULL, FALSE);
    g_return_val_if_fail (direction != NULL, FALSE);

    priv = info->priv;

    if (!priv->valid)
	return FALSE;

    if (priv->windspeed < 0.0 || priv->wind <= GWEATHER_WIND_INVALID || priv->wind >= GWEATHER_WIND_LAST)
        return FALSE;

    res = speed_value (priv->windspeed, unit, speed, priv->settings);
    *direction = priv->wind;

    return res;
}

/**
 * gweather_info_get_value_pressure:
 * @info: a #GWeatherInfo
 * @unit: the desired unit, as a #GWeatherPressureUnit
 * @value: (out): forecasted pressure, expressed in @unit
 *
 * Returns: TRUE if @value is valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_pressure (GWeatherInfo *info,
				  GWeatherPressureUnit unit,
				  gdouble *value)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (value != NULL, FALSE);

    if (!info->priv->valid)
	return FALSE;

    return pressure_value (info->priv->pressure, unit, value, info->priv->settings);
}

/**
 * gweather_info_get_value_visibility:
 * @info: a #GWeatherInfo
 * @unit: the desired unit, as a #GWeatherDistanceUnit
 * @value: (out): forecasted visibility, expressed in @unit
 *
 * Returns: TRUE if @value is valid, FALSE otherwise.
 */
gboolean
gweather_info_get_value_visibility (GWeatherInfo *info,
				    GWeatherDistanceUnit unit,
				    gdouble *value)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info), FALSE);
    g_return_val_if_fail (value != NULL, FALSE);

    if (!info->priv->valid)
	return FALSE;

    return distance_value (info->priv->visibility, unit, value, info->priv->settings);
}

static void
gweather_info_set_location_internal (GWeatherInfo     *info,
                                     GWeatherLocation *location)
{
    GWeatherInfoPrivate *priv = info->priv;
    GVariant *default_loc = NULL;
    const gchar *name = NULL;
    gboolean latlon_override = FALSE;
    gdouble lat, lon;

    if (priv->glocation == location)
        return;

    if (priv->glocation)
	gweather_location_unref (priv->glocation);

    priv->glocation = location;

    if (priv->glocation) {
        gweather_location_ref (location);
    } else {
        GWeatherLocation *world;
        const gchar *station_code;

        default_loc = g_settings_get_value (priv->settings, DEFAULT_LOCATION);

        g_variant_get (default_loc, "(&s&sm(dd))", &name, &station_code, &latlon_override, &lat, &lon);

	if (strcmp(name, "") == 0)
	    name = NULL;

	world = gweather_location_get_world ();
	priv->glocation = gweather_location_find_by_station_code (world, station_code);
	if (priv->glocation)
	    gweather_location_ref (priv->glocation);
    }

    if (priv->glocation) {
        _weather_location_free (&priv->location);
        _gweather_location_update_weather_location (priv->glocation,
						    &priv->location);
    }

    if (name) {
	g_free (priv->location.name);
	priv->location.name = g_strdup (name);
    }

    if (latlon_override) {
	priv->location.latlon_valid = TRUE;
	priv->location.latitude = DEGREES_TO_RADIANS (lat);
	priv->location.longitude = DEGREES_TO_RADIANS (lon);
    }

    if (default_loc)
	g_variant_unref (default_loc);
}

/**
 * gweather_info_set_location:
 * @info: a #GWeatherInfo
 * @location: (allow-none): a location for which weather is desired
 *
 * Changes @info to report weather for @location.
 * Note that this will clear any forecast or current conditions from
 * @info, you must call gweather_info_update() to obtain the new data.
 */
void
gweather_info_set_location (GWeatherInfo     *info,
			    GWeatherLocation *location)
{
    g_return_if_fail (GWEATHER_IS_INFO (info));

    gweather_info_set_location_internal (info, location);
    gweather_info_reset (info);
}

GWeatherProvider
gweather_info_get_enabled_providers (GWeatherInfo *info)
{
    g_return_val_if_fail (GWEATHER_IS_INFO (info),
			  GWEATHER_PROVIDER_NONE);

    return info->priv->providers;
}

void
gweather_info_set_enabled_providers (GWeatherInfo     *info,
				     GWeatherProvider  providers)
{
    g_return_if_fail (GWEATHER_IS_INFO (info));

    if (info->priv->providers == providers)
	return;

    info->priv->providers = providers;

    gweather_info_abort (info);
    gweather_info_update (info);
    g_object_notify (G_OBJECT (info), "enabled-providers");
}


static void
gweather_info_set_property (GObject *object,
			    guint property_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
    GWeatherInfo *self = GWEATHER_INFO (object);

    switch (property_id) {
    case PROP_LOCATION:
	gweather_info_set_location_internal (self, (GWeatherLocation*) g_value_get_boxed (value));
	break;
    case PROP_ENABLED_PROVIDERS:
	gweather_info_set_enabled_providers (self, g_value_get_flags (value));
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gweather_info_get_property (GObject    *object,
			    guint       property_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
    GWeatherInfo *self = GWEATHER_INFO (object);
    GWeatherInfoPrivate *priv = self->priv;

    switch (property_id) {
    case PROP_LOCATION:
	g_value_set_boxed (value, priv->glocation);
	break;
    case PROP_ENABLED_PROVIDERS:
	g_value_set_flags (value, priv->providers);
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

void
gweather_info_class_init (GWeatherInfoClass *klass)
{
    GParamSpec *pspec;
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof(GWeatherInfoPrivate));

    gobject_class->dispose = gweather_info_dispose;
    gobject_class->finalize = gweather_info_finalize;
    gobject_class->set_property = gweather_info_set_property;
    gobject_class->get_property = gweather_info_get_property;

    pspec = g_param_spec_boxed ("location",
				"Location",
				"The location this info represents",
				GWEATHER_TYPE_LOCATION,
				G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    g_object_class_install_property (gobject_class, PROP_LOCATION, pspec);

    pspec = g_param_spec_flags ("enabled-providers",
				"Enabled providers",
				"A bitmask of enabled weather service providers",
				GWEATHER_TYPE_PROVIDER,
				GWEATHER_PROVIDER_METAR | GWEATHER_PROVIDER_IWIN,
				G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class, PROP_ENABLED_PROVIDERS, pspec);

    /**
     * GWeatherInfo::updated:
     * @object: the emitter of the signal.
     *
     * This signal is emitted after the initial fetch of the weather
     * data from upstream services, and after every successful call
     * to @gweather_info_update().
     */
    gweather_info_signals[SIGNAL_UPDATED] = g_signal_new ("updated",
							  GWEATHER_TYPE_INFO,
							  G_SIGNAL_RUN_FIRST,
							  G_STRUCT_OFFSET (GWeatherInfoClass, updated),
							  NULL, /* accumulator */
							  NULL, /* accu_data */
							  g_cclosure_marshal_VOID__VOID,
							  G_TYPE_NONE, 0);

    _gweather_gettext_init ();
}

/**
 * gweather_info_new:
 * @location: (allow-none): the desidered #GWeatherLocation (%NULL for default)
 *
 * Builds a new #GWeatherInfo that will provide weather information about
 * @location.
 *
 * Returns: (transfer full): a new #GWeatherInfo
 */
GWeatherInfo *
gweather_info_new (GWeatherLocation     *location)
{
    GWeatherInfo *self;

    if (location != NULL)
	self = g_object_new (GWEATHER_TYPE_INFO, "location", location, NULL);
    else
	self = g_object_new (GWEATHER_TYPE_INFO, NULL);

    gweather_info_update (self);
    return self;
}

GWeatherInfo *
_gweather_info_new_clone (GWeatherInfo *other)
{
    return g_object_new (GWEATHER_TYPE_INFO, "location", other->priv->glocation, NULL);
}

