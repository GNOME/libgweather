/* gweather-private.h - Private header for weather server functions
 *
 * SPDX-FileCopyrightText: The GWeather authors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <time.h>
#include <libintl.h>
#include <math.h>
#include <gio/gio.h>
#include <libsoup/soup.h>
#include <glib/gi18n-lib.h>

#include "gweather-info.h"
#include "gweather-location.h"
#include "gweather-db.h"

#include "kdtree.h"

#define INVALID_IDX G_MAXUINT16
#define IDX_VALID(idx) ((idx) < 0xffff)
#define EMPTY_TO_NULL(s) ((s)[0] == '\0' ? NULL : (s))

G_BEGIN_DECLS

void
_gweather_gettext_init (void);

typedef struct {
    GMappedFile *map;
    DbWorldRef world;
    DbArrayofLocationRef locations_ref;
    DbWorldTimezonesRef timezones_ref;

    GPtrArray *locations;
    GPtrArray *timezones;

    struct kdtree *cities_kdtree;
} GWeatherDb;

struct _GWeatherLocation {
    GObject parent_instance;

    GWeatherDb *db;
    guint db_idx;
    DbLocationRef ref;

    /* Attributes with _ may be fetched/filled from the database on the fly */
    char *_english_name;
    char *_local_name;
    char *_local_sort_name;
    char *_english_sort_name;

    /* From the DB, except for nearest clones */
    guint16 parent_idx;

    GWeatherLocation *_parent;
    GWeatherLocation **_children;

    GTimeZone *_timezone;
    GWeatherLocationLevel level;
    char *_country_code;
    guint16 tz_hint_idx;
    char *_station_code;

    double latitude;
    double longitude;

    gboolean latlon_valid;
};

#define WEATHER_LOCATION_CODE_LEN 4

typedef struct {
    char *name;
    char *code;
    char *zone;
    char *radar;
    gboolean latlon_valid;
    double  latitude;
    double  longitude;
    char *country_code;
    char *tz_hint;
} WeatherLocation;

void
_gweather_location_update_weather_location (GWeatherLocation *gloc,
                                            WeatherLocation *loc);

void
gweather_location_ensure_world (void);

GWeatherDb *
gweather_get_db (void);

/*
 * Weather information.
 */

typedef double GWeatherTemperature;
typedef double GWeatherHumidity;
typedef double GWeatherWindSpeed;
typedef double GWeatherPressure;
typedef double GWeatherVisibility;
typedef time_t GWeatherUpdate;

struct _GWeatherInfo {
    GObject parent_instance;

    GWeatherProvider providers;
    GSettings *settings;
    char *application_id;
    char *contact_info;

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
    char *forecast_attribution;
    SoupSession *session;
    GSList *requests_pending;
};

/* Values common to the parsing source files */

#define DATA_SIZE 5000

#define CONST_DIGITS "0123456789"
#define CONST_ALPHABET "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

/* Units conversions and names */

#define TEMP_F_TO_C(f)                  (((f) - 32.0) * 0.555556)
#define TEMP_F_TO_K(f)                  (TEMP_F_TO_C (f) + 273.15)
#define TEMP_C_TO_F(c)                  (((c) * 1.8) + 32.0)

#define WINDSPEED_KNOTS_TO_KPH(knots)   ((knots) * 1.851965)
#define WINDSPEED_KNOTS_TO_MPH(knots)   ((knots) * 1.150779)
#define WINDSPEED_KNOTS_TO_MS(knots)    ((knots) * 0.514444)
#define WINDSPEED_MS_TO_KNOTS(ms)       ((ms) / 0.514444)
/* 1 bft ~= (1 m/s / 0.836) ^ (2/3) */
#define WINDSPEED_KNOTS_TO_BFT(knots)   (pow ((knots) * 0.615363, 0.666666))

#define PRESSURE_INCH_TO_KPA(inch)      ((inch) * 3.386)
#define PRESSURE_INCH_TO_HPA(inch)      ((inch) * 33.86)
#define PRESSURE_INCH_TO_MM(inch)       ((inch) * 25.40005)
#define PRESSURE_INCH_TO_MB(inch)       (PRESSURE_INCH_TO_HPA (inch))
#define PRESSURE_INCH_TO_ATM(inch)      ((inch) * 0.033421052)
#define PRESSURE_MBAR_TO_INCH(mbar)     ((mbar) * 0.029533373)

#define VISIBILITY_SM_TO_KM(sm)         ((sm) * 1.609344)
#define VISIBILITY_SM_TO_M(sm)          (VISIBILITY_SM_TO_KM (sm) * 1000)

#define DEGREES_TO_RADIANS(deg)         ((fmod ((deg), 360.) / 180.) * M_PI)
#define RADIANS_TO_DEGREES(rad)         ((rad) * 180. / M_PI)
#define RADIANS_TO_HOURS(rad)           ((rad) * 12. / M_PI)

char *
_radians_to_degrees_str (double radians);

/*
 * Planetary Mean Orbit and their progressions from J2000 are based on the
 * values in http://ssd.jpl.nasa.gov/txt/aprx_pos_planets.pdf
 * converting longitudes from heliocentric to geocentric coordinates (+180)
 */
#define EPOCH_TO_J2000(t)               ((double) (t) - 946727935.816)
#define MEAN_ECLIPTIC_LONGITUDE(d)      (280.46457166 + (d) / 36525. * 35999.37244981)
#define SOL_PROGRESSION                 (360. / 365.242191)
#define PERIGEE_LONGITUDE(d)            (282.93768193 + (d) / 36525. * 0.32327364)

void
metar_start_open (GWeatherInfo *info);

gboolean
iwin_start_open (GWeatherInfo *info);

gboolean
metno_start_open (GWeatherInfo *info);

gboolean
owm_start_open (GWeatherInfo *info);

gboolean
nws_start_open (GWeatherInfo *info);

gboolean
metar_parse (char *metar,
             GWeatherInfo *info);

void
_gweather_info_begin_request (GWeatherInfo *info,
                              SoupMessage *message);

void
_gweather_info_request_done (GWeatherInfo *info,
                             SoupMessage *message);

void        _gweather_info_queue_request (GWeatherInfo *info,
                                          SoupMessage *message,
                                          GAsyncReadyCallback callback);

void
ecl2equ (double t,
         double eclipLon,
         double eclipLat,
         double *ra,
         double *decl);

double
sunEclipLongitude (time_t t);

void
_gweather_info_ensure_sun (GWeatherInfo *info);

void
_gweather_info_ensure_moon (GWeatherInfo *info);

void
free_forecast_list (GWeatherInfo *info);

GWeatherInfo *
_gweather_info_new_clone (GWeatherInfo *original);

gssize
_gweather_find_nearest_city_index (double lat,
                                   double lon);

G_END_DECLS
