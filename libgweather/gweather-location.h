/* gweather-location.h - Location-handling code
 *
 * SPDX-FileCopyrightText: 2008, Red Hat, Inc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !(defined(IN_GWEATHER_H) || defined(GWEATHER_COMPILATION))
#error "gweather-location.h must not be included individually, include gweather.h instead"
#endif

#include <gio/gio.h>
#include <libgweather/gweather-version.h>

G_BEGIN_DECLS

/**
 * GWeatherLocation:
 *
 * A `GWeatherLocation` represents a "location" of some type known to
 * libgweather; anything from a single weather station to the entire
 * world.
 *
 * See [enum@GWeather.LocationLevel] for information about how the
 * hierarchy of locations works.
 */
typedef struct _GWeatherLocation GWeatherLocation;

/**
 * GWeatherFilterFunc:
 * @location: the location to check
 * @user_data: data passed to [method@GWeather.Location.find_nearest_city_full]
 *
 * A filter function for locations.
 *
 * Returns: `FALSE` if the location should be skipped, and `TRUE` otherwise
 */
typedef gboolean (* GWeatherFilterFunc) (GWeatherLocation* location,
                                         gpointer user_data);

/**
 * GWeatherLocationLevel:
 * @GWEATHER_LOCATION_WORLD: A location representing the entire world
 * @GWEATHER_LOCATION_REGION: A location representing a continent or other
 *   top-level region
 * @GWEATHER_LOCATION_COUNTRY: A location representing a "country" (or other
 *   geographic unit that has an ISO-3166 country code)
 * @GWEATHER_LOCATION_ADM1: A location representing a "first-level
 *   administrative division"; ie, a state, province, or similar division
 * @GWEATHER_LOCATION_CITY: A location representing a city
 * @GWEATHER_LOCATION_WEATHER_STATION: A location representing a weather
 *   station
 * @GWEATHER_LOCATION_DETACHED: A location that is detached from the database,
 *   for example because it was loaded from external storage and could not be
 *   fully recovered. The parent of this location is the nearest weather
 *   station
 * @GWEATHER_LOCATION_NAMED_TIMEZONE: A location representing a named or
 *   special timezone in the world, such as UTC
 *
 * The size/scope of a particular [class@GWeather.Location].
 *
 * Locations form a hierarchy, with a `GWEATHER_LOCATION_WORLD` location
 * at the top, divided into regions or countries, and so on.
 *
 * Countries may or may not be divided into "adm1"s, and "adm1"s may or
 * may not be divided into "adm2"s. A city will have at least one, and
 * possibly several, weather stations inside it. Weather stations will
 * never appear outside of cities.
 *
 * Building a database with [func@GWeather.Location.get_world] will never
 * create detached instances, but deserializing might.
 */
typedef enum { /*< underscore_name=gweather_location_level >*/
    GWEATHER_LOCATION_WORLD,
    GWEATHER_LOCATION_REGION,
    GWEATHER_LOCATION_COUNTRY,
    GWEATHER_LOCATION_ADM1,
    GWEATHER_LOCATION_CITY,
    GWEATHER_LOCATION_WEATHER_STATION,
    GWEATHER_LOCATION_DETACHED,
    GWEATHER_LOCATION_NAMED_TIMEZONE
} GWeatherLocationLevel;

#define GWEATHER_TYPE_LOCATION (gweather_location_get_type ())

GWEATHER_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GWeatherLocation, gweather_location, GWEATHER, LOCATION, GObject)

GWEATHER_AVAILABLE_IN_ALL
GWeatherLocation *      gweather_location_get_world             (void);

GWEATHER_AVAILABLE_IN_ALL
const char *            gweather_location_get_name              (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
const char *            gweather_location_get_sort_name         (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
const char *            gweather_location_get_english_name      (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
const char *            gweather_location_get_english_sort_name (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
GWeatherLocationLevel   gweather_location_get_level             (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
GWeatherLocation *      gweather_location_get_parent            (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
GWeatherLocation *      gweather_location_next_child            (GWeatherLocation  *loc,
                                                                 GWeatherLocation  *child);

GWEATHER_AVAILABLE_IN_ALL
gboolean                gweather_location_has_coords            (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
void                    gweather_location_get_coords            (GWeatherLocation  *loc,
                                                                 double            *latitude,
                                                                 double            *longitude);
GWEATHER_AVAILABLE_IN_ALL
double                  gweather_location_get_distance          (GWeatherLocation  *loc,
                                                                 GWeatherLocation  *loc2);
GWEATHER_AVAILABLE_IN_ALL
GWeatherLocation *      gweather_location_find_nearest_city     (GWeatherLocation  *loc,
                                                                 double             lat,
                                                                 double             lon);

GWEATHER_AVAILABLE_IN_ALL
GWeatherLocation *      gweather_location_find_nearest_city_full        (GWeatherLocation   *loc,
                                                                         double              lat,
                                                                         double              lon,
                                                                         GWeatherFilterFunc  func,
                                                                         gpointer            user_data,
                                                                         GDestroyNotify      destroy);
GWEATHER_AVAILABLE_IN_ALL
void                    gweather_location_detect_nearest_city           (GWeatherLocation   *loc,
                                                                         double              lat,
                                                                         double              lon,
                                                                         GCancellable       *cancellable,
                                                                         GAsyncReadyCallback callback,
                                                                         gpointer            user_data);
GWEATHER_AVAILABLE_IN_ALL
GWeatherLocation *      gweather_location_detect_nearest_city_finish    (GAsyncResult       *result,
                                                                         GError            **error);

GWEATHER_AVAILABLE_IN_ALL
const char *            gweather_location_get_country           (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
gboolean                gweather_location_has_timezone          (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
GTimeZone *             gweather_location_get_timezone          (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
const char *            gweather_location_get_timezone_str      (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
GTimeZone **            gweather_location_get_timezones         (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
void                    gweather_location_free_timezones        (GWeatherLocation  *loc,
                                                                 GTimeZone        **zones);
GWEATHER_AVAILABLE_IN_ALL
const char *            gweather_location_get_code              (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
char *                  gweather_location_get_city_name         (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
char *                  gweather_location_get_country_name      (GWeatherLocation  *loc);

GWEATHER_AVAILABLE_IN_ALL
GWeatherLocation *      gweather_location_find_by_station_code  (GWeatherLocation  *world,
                                                                 const char        *station_code);
GWEATHER_AVAILABLE_IN_ALL
GWeatherLocation *      gweather_location_find_by_country_code  (GWeatherLocation  *world,
                                                                 const char        *country_code);

GWEATHER_AVAILABLE_IN_ALL
gboolean                gweather_location_equal                 (GWeatherLocation  *one,
                                                                 GWeatherLocation  *two);

GWEATHER_AVAILABLE_IN_ALL
GVariant *              gweather_location_serialize             (GWeatherLocation  *loc);
GWEATHER_AVAILABLE_IN_ALL
GWeatherLocation *      gweather_location_deserialize           (GWeatherLocation  *world,
                                                                 GVariant          *serialized);

GWEATHER_AVAILABLE_IN_ALL
GWeatherLocation *      gweather_location_new_detached          (const char        *name,
                                                                 const char        *icao,
                                                                 double             latitude,
                                                                 double             longitude);

GWEATHER_AVAILABLE_IN_ALL
const char *            gweather_location_level_to_string       (GWeatherLocationLevel level);

G_END_DECLS
