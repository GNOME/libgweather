/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* weather-priv.h - Private header for weather server functions.
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

#ifndef __WEATHER_PRIV_H_
#define __WEATHER_PRIV_H_

#include "config.h"

#include <time.h>
#include <libintl.h>
#include <math.h>
#include <gio/gio.h>
#include <libsoup/soup.h>
#include <glib/gi18n-lib.h>

#include "gweather-weather.h"
#include "gweather-location.h"

void        _gweather_gettext_init (void);

struct _GWeatherLocation {
    char *english_name, *local_name, *msgctxt, *local_sort_name, *english_sort_name;
    GWeatherLocation *parent, **children;
    GWeatherLocationLevel level;
    char *country_code, *tz_hint;
    char *station_code, *forecast_zone, *radar;
    double latitude, longitude;
    gboolean latlon_valid;
    GWeatherTimezone **zones;
    GHashTable *metar_code_cache;
    GHashTable *timezone_cache;
    GHashTable *country_code_cache;

    int ref_count;
};

#define WEATHER_LOCATION_CODE_LEN 4

typedef struct {
    gchar *name;
    gchar *code;
    gchar *zone;
    gchar *radar;
    gboolean latlon_valid;
    gdouble  latitude;
    gdouble  longitude;
    gchar *country_code;
    gchar *tz_hint;
} WeatherLocation;

GWeatherLocation *_gweather_location_new_detached (GWeatherLocation *nearest_station,
						   const char       *name,
						   gboolean          latlon_valid,
						   gdouble           latitude,
						   gdouble           longitude);

void              _gweather_location_update_weather_location (GWeatherLocation *gloc,
							      WeatherLocation  *loc);

/*
 * Weather information.
 */

typedef gdouble GWeatherTemperature;
typedef gdouble GWeatherHumidity;
typedef gdouble GWeatherWindSpeed;
typedef gdouble GWeatherPressure;
typedef gdouble GWeatherVisibility;
typedef time_t GWeatherUpdate;

struct _GWeatherInfoPrivate {
    GWeatherProvider providers;

    GSettings *settings;

    gboolean valid;
    gboolean network_error;
    gboolean sunriseValid;
    gboolean sunsetValid;
    gboolean midnightSun;
    gboolean polarNight;
    gboolean moonValid;
    gboolean tempMinMaxValid;

    /* TRUE if we don't need to calc humidity from
       temperature and dew point (and conversely, we
       need to calculate the dew point from temperature
       and humidity)
    */
    gboolean hasHumidity;

    WeatherLocation location;
    GWeatherLocation *glocation;
    GWeatherUpdate update;
    GWeatherUpdate current_time;
    GWeatherSky sky;
    GWeatherConditions cond;
    GWeatherTemperature temp;
    GWeatherTemperature temp_min;
    GWeatherTemperature temp_max;
    GWeatherTemperature dew;
    GWeatherHumidity humidity;
    GWeatherWindDirection wind;
    GWeatherWindSpeed windspeed;
    GWeatherPressure pressure;
    GWeatherVisibility visibility;
    GWeatherUpdate sunrise;
    GWeatherUpdate sunset;
    GWeatherMoonPhase moonphase;
    GWeatherMoonLatitude moonlatitude;
    GSList *forecast_list; /* list of GWeatherInfo* for the forecast, NULL if not available */
    gchar *forecast_attribution;
    gchar *radar_buffer;
    gchar *radar_url;
    GdkPixbufLoader *radar_loader;
    GdkPixbufAnimation *radar;
    SoupSession *session;
    GSList *requests_pending;
};

/* Values common to the parsing source files */

#define DATA_SIZE			5000

#define CONST_DIGITS			"0123456789"
#define CONST_ALPHABET			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"

/* Units conversions and names */

#define TEMP_F_TO_C(f)			(((f) - 32.0) * 0.555556)
#define TEMP_F_TO_K(f)			(TEMP_F_TO_C (f) + 273.15)
#define TEMP_C_TO_F(c)			(((c) * 1.8) + 32.0)

#define WINDSPEED_KNOTS_TO_KPH(knots)	((knots) * 1.851965)
#define WINDSPEED_KNOTS_TO_MPH(knots)	((knots) * 1.150779)
#define WINDSPEED_KNOTS_TO_MS(knots)	((knots) * 0.514444)
#define WINDSPEED_MS_TO_KNOTS(ms)	((ms) / 0.514444)
/* 1 bft ~= (1 m/s / 0.836) ^ (2/3) */
#define WINDSPEED_KNOTS_TO_BFT(knots)	(pow ((knots) * 0.615363, 0.666666))

#define PRESSURE_INCH_TO_KPA(inch)	((inch) * 3.386)
#define PRESSURE_INCH_TO_HPA(inch)	((inch) * 33.86)
#define PRESSURE_INCH_TO_MM(inch)	((inch) * 25.40005)
#define PRESSURE_INCH_TO_MB(inch)	(PRESSURE_INCH_TO_HPA (inch))
#define PRESSURE_INCH_TO_ATM(inch)	((inch) * 0.033421052)
#define PRESSURE_MBAR_TO_INCH(mbar)	((mbar) * 0.029533373)

#define VISIBILITY_SM_TO_KM(sm)		((sm) * 1.609344)
#define VISIBILITY_SM_TO_M(sm)		(VISIBILITY_SM_TO_KM (sm) * 1000)

#define DEGREES_TO_RADIANS(deg)		((fmod ((deg),360.) / 180.) * M_PI)
#define RADIANS_TO_DEGREES(rad)		((rad) * 180. / M_PI)
#define RADIANS_TO_HOURS(rad)		((rad) * 12. / M_PI)

/*
 * Planetary Mean Orbit and their progressions from J2000 are based on the
 * values in http://ssd.jpl.nasa.gov/txt/aprx_pos_planets.pdf
 * converting longitudes from heliocentric to geocentric coordinates (+180)
 */
#define EPOCH_TO_J2000(t)          ((gdouble)(t)-946727935.816)
#define MEAN_ECLIPTIC_LONGITUDE(d) (280.46457166 + (d)/36525.*35999.37244981)
#define SOL_PROGRESSION            (360./365.242191)
#define PERIGEE_LONGITUDE(d)       (282.93768193 + (d)/36525.*0.32327364)

void		metar_start_open	(GWeatherInfo *info);
gboolean	iwin_start_open		(GWeatherInfo *info);
void		wx_start_open		(GWeatherInfo *info);
gboolean        yrno_start_open         (GWeatherInfo *info);
gboolean        owm_start_open          (GWeatherInfo *info);

gboolean	metar_parse		(gchar *metar,
					 GWeatherInfo *info);

void            _gweather_info_begin_request (GWeatherInfo *info,
					      SoupMessage  *message);
void		_gweather_info_request_done (GWeatherInfo *info,
					     SoupMessage  *message);

void		ecl2equ			(gdouble t,
					 gdouble eclipLon,
					 gdouble eclipLat,
					 gdouble *ra,
					 gdouble *decl);
gdouble		sunEclipLongitude	(time_t t);

void            _gweather_info_ensure_sun  (GWeatherInfo *info);
void            _gweather_info_ensure_moon (GWeatherInfo *info);

void		free_forecast_list	  (GWeatherInfo *info);

GWeatherInfo   *_gweather_info_new_clone  (GWeatherInfo *info);

#endif /* __WEATHER_PRIV_H_ */

