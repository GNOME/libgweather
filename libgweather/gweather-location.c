/* gweather-location.c - Location-handling code
 *
 * SPDX-FileCopyrightText: 2008, Red Hat, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gweather-location.h"

#include "gweather-private.h"

#include <geocode-glib/geocode-glib.h>
#include <glib/gi18n-lib.h>
#include <libxml/xmlreader.h>
#include <locale.h>
#include <math.h>
#include <string.h>

/* This is the precision of coordinates in the database */
#define EPSILON 0.000001

/* This is the maximum distance for which we will attach an
 * airport to a city, see also test_distance() */
#define AIRPORT_MAX_DISTANCE 100.0

G_DEFINE_TYPE (GWeatherLocation, gweather_location, G_TYPE_OBJECT)

static gboolean env_no_nearest;

static void
gweather_location_finalize (GObject *gobject)
{
    GWeatherLocation *self = GWEATHER_LOCATION (gobject);

    /* Remove weak reference from DB object; but only if it points to us.
     * It may point elsewhere if we are an implicit nearest child. */
    if (self->db && g_ptr_array_index (self->db->locations, self->db_idx) == self)
        g_ptr_array_index (self->db->locations, self->db_idx) = NULL;

    g_free (self->_english_name);
    g_free (self->_local_name);
    g_free (self->_local_sort_name);
    g_free (self->_english_sort_name);
    g_free (self->_country_code);
    g_free (self->_station_code);

    g_clear_pointer (&self->_timezone, g_time_zone_unref);

    G_OBJECT_CLASS (gweather_location_parent_class)->finalize (gobject);
}

static void
gweather_location_dispose (GObject *gobject)
{
    GWeatherLocation *self = GWEATHER_LOCATION (gobject);

    if (self->_children) {
        for (int i = 0; self->_children[i]; i++) {
            g_object_unref (self->_children[i]);
        }
        g_free (self->_children);
        self->_children = NULL;
    }

    g_clear_object (&self->_parent);

    G_OBJECT_CLASS (gweather_location_parent_class)->dispose (gobject);
}

static void
gweather_location_class_init (GWeatherLocationClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = gweather_location_dispose;
    G_OBJECT_CLASS (klass)->finalize = gweather_location_finalize;

    env_no_nearest = !!g_getenv ("LIBGWEATHER_LOCATIONS_NO_NEAREST");
}

static void
gweather_location_init (GWeatherLocation *self)
{
    self->latitude = self->longitude = DBL_MAX;
    self->db_idx = INVALID_IDX;
    self->tz_hint_idx = INVALID_IDX;
}

static inline GWeatherLocation *
_iter_up (GWeatherLocation *loc)
{
    GWeatherLocation *tmp;

    tmp = gweather_location_get_parent (loc);
    g_object_unref (loc);

    return tmp;
}

#define ITER_UP(start, _p) for ((_p) = g_object_ref (start); (_p); (_p) = _iter_up (_p))

/*< private >
 * gweather_location_new_full:
 * @level: the level of the location
 * @nearest_station: (nullable) (transfer full): the nearest location
 * @name: (nullable): the name of the location
 * @latlon_valid: whether the @latitude and @longitude arguments are set
 * @latitude: the latitude of the location
 * @longitude: the longitude of the location
 *
 * Creates a new `GWeatherLocation` for the given level.
 *
 * If @name is set, it will be used to set the localised and non-localised
 * name; otherwise, if @nearest_station is set, it will be used to populate
 * the localised and non-localised name.
 *
 * The new location will use @nearest_station as its parent.
 *
 * If @latlon_valid is `TRUE`, the coordinates of the new location will
 * be taken from the @latitude and @longitude arguments; otherwise, the
 * coordinates of the @nearest_station will be used.
 *
 * Returns: (transfer full): the newly created location
 */
static GWeatherLocation *
gweather_location_new_full (GWeatherLocationLevel level,
                            GWeatherLocation *nearest_station,
                            const char *name,
                            gboolean latlon_valid,
                            double latitude,
                            double longitude)
{
    GWeatherLocation *self;
    char *normalized;

    self = g_object_new (GWEATHER_TYPE_LOCATION, NULL);

    self->level = level;

    if (name != NULL) {
        self->_english_name = g_strdup (name);
        self->_local_name = g_strdup (name);

        normalized = g_utf8_normalize (name, -1, G_NORMALIZE_ALL);
        self->_english_sort_name = g_utf8_casefold (normalized, -1);
        self->_local_sort_name = g_strdup (self->_english_sort_name);
        g_free (normalized);
    } else if (nearest_station != NULL) {
        self->_english_name = g_strdup (gweather_location_get_english_name (nearest_station));
        self->_local_name = g_strdup (gweather_location_get_name (nearest_station));
        self->_english_sort_name = g_strdup (gweather_location_get_english_sort_name (nearest_station));
        self->_local_sort_name = g_strdup (gweather_location_get_sort_name (nearest_station));
    }

    self->_parent = nearest_station; /* the new location owns the parent */
    self->_children = NULL;

    if (nearest_station != NULL)
        self->_station_code = g_strdup (gweather_location_get_code (nearest_station));

    if (latlon_valid) {
        self->latlon_valid = TRUE;
        self->latitude = latitude;
        self->longitude = longitude;
    } else if (nearest_station != NULL) {
        self->latlon_valid = nearest_station->latlon_valid;
        self->latitude = nearest_station->latitude;
        self->longitude = nearest_station->longitude;
    } else {
        self->latlon_valid = FALSE;
    }

    return self;
}

static void
add_timezones (GWeatherLocation *loc,
               GPtrArray *zones);

static GTimeZone *
timezone_ref_for_idx (GWeatherDb *db,
                      guint16 idx)
{
    DbWorldTimezonesEntryRef ref;
    const char *id;

    g_assert (db);
    g_assert (idx < db->timezones->len);

    GTimeZone *zone = g_ptr_array_index (db->timezones, idx);
    if (zone != NULL)
        return g_time_zone_ref (zone);

    ref = db_world_timezones_get_at (db->timezones_ref, idx);
    id = db_world_timezones_entry_get_key (ref);

    return g_time_zone_new_identifier (id);
}

static GWeatherLocation *
location_ref_for_idx (GWeatherDb *db,
                      guint16 idx,
                      GWeatherLocation *nearest_of)
{
    GWeatherLocation *loc;
    DbLocationRef ref;

    g_assert (db);
    g_assert (idx < db->locations->len);

    /* nearest copies are cached by the parent */
    if (!nearest_of) {
        loc = g_ptr_array_index (db->locations, idx);
        if (loc) {
            return g_object_ref (loc);
        }
    }

    ref = db_arrayof_location_get_at (db->locations_ref, idx);

    double latitude = db_coordinate_get_lat (db_location_get_coordinates (ref));
    double longitude = db_coordinate_get_lon (db_location_get_coordinates (ref));
    gboolean latlon_valid = isfinite (latitude) && isfinite (longitude);

    loc = gweather_location_new_full (db_location_get_level (ref),
                                      NULL,
                                      NULL,
                                      latlon_valid,
                                      latitude,
                                      longitude);
    loc->db = db;
    loc->db_idx = idx;
    loc->ref = ref;

    /* Override parent information for "nearest" copies. */
    if (nearest_of)
        loc->parent_idx = nearest_of->db_idx;
    else
        loc->parent_idx = db_location_get_parent (ref);

    loc->tz_hint_idx = db_location_get_tz_hint (ref);

    /* Note, we used to sort locations by distance (for cities) and name;
     * distance sorting is done in the variant already, name sorting however
     * needs translations and is not done anymore.
     */

    /* Store a weak reference in the cache.
     *
     * Implicit "nearest" copies do not have a weak reference, they simply
     * belong to the parent.
     */
    if (!nearest_of)
        g_ptr_array_index (db->locations, idx) = loc;

    return loc;
}

/**
 * gweather_location_get_world:
 *
 * Obtains the shared `GWeatherLocation` of type `GWEATHER_LOCATION_WORLD`,
 * representing a hierarchy containing all of the locations from the
 * location data.
 *
 * Return value: (nullable) (transfer full): a `GWEATHER_LOCATION_WORLD`
 *   location, or `NULL` if the locations data could not be found or could
 *   not be parsed.
 **/
GWeatherLocation *
gweather_location_get_world (void)
{
    gweather_location_ensure_world ();

    return location_ref_for_idx (gweather_get_db (), 0, NULL);
}

/**
 * gweather_location_get_name:
 * @loc: a #GWeatherLocation
 *
 * Gets the location's name, localized into the current language.
 *
 * Return value: (nullable) (transfer none): the location's name
 **/
const char *
gweather_location_get_name (GWeatherLocation *loc)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);

    if (loc->_local_name)
        return loc->_local_name;

    if (loc->db && IDX_VALID (loc->db_idx)) {
        const char *english_name;
        const char *msgctxt;
        english_name = EMPTY_TO_NULL (db_i18n_get_str (db_location_get_name (loc->ref)));
        msgctxt = EMPTY_TO_NULL (db_i18n_get_msgctxt (db_location_get_name (loc->ref)));

        if (msgctxt) {
            loc->_local_name = g_strdup (g_dpgettext2 (LOCATIONS_GETTEXT_PACKAGE,
                                                       msgctxt,
                                                       english_name));
        } else {
            loc->_local_name = g_strdup (g_dgettext (LOCATIONS_GETTEXT_PACKAGE,
                                                     english_name));
        }
        return loc->_local_name;
    } else {
        return NULL;
    }
}

/**
 * gweather_location_get_sort_name:
 * @loc: a #GWeatherLocation
 *
 * Gets the location's name, localized into the current language,
 * in a representation useful for comparisons.
 *
 * The "sort name" is the location's name after having g_utf8_normalize()
 * (with `G_NORMALIZE_ALL`) and g_utf8_casefold() called on it. You can
 * use this to sort locations, or to comparing user input against a
 * location name.
 *
 * Return value: (nullable) (transfer none): the sort name of the location
 **/
const char *
gweather_location_get_sort_name (GWeatherLocation *loc)
{
    const char *local_name;
    g_autofree char *normalized = NULL;

    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);

    if (loc->_local_sort_name)
        return loc->_local_sort_name;

    local_name = gweather_location_get_name (loc);
    if (!local_name)
        return NULL;

    normalized = g_utf8_normalize (local_name, -1, G_NORMALIZE_ALL);
    loc->_local_sort_name = g_utf8_casefold (normalized, -1);

    return loc->_local_sort_name;
}

/**
 * gweather_location_get_english_name:
 * @loc: a #GWeatherLocation
 *
 * Gets the location's name.
 *
 * Return value: (transfer none) (nullable): the location's name
 **/
const char *
gweather_location_get_english_name (GWeatherLocation *loc)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);

    if (loc->_english_name)
        return loc->_english_name;

    if (loc->db && IDX_VALID (loc->db_idx))
        return EMPTY_TO_NULL (db_i18n_get_str (db_location_get_name (loc->ref)));

    return NULL;
}

/**
 * gweather_location_get_english_sort_name:
 * @loc: a #GWeatherLocation
 *
 * Gets the location's name, in a representation useful for comparisons.
 *
 * The "sort name" is the location's name after having g_utf8_normalize()
 * (with `G_NORMALIZE_ALL`) and g_utf8_casefold() called on it. You can
 * use this to sort locations, or to comparing user input against a
 * location name.
 *
 * Return value: (nullable) (transfer none): the sort name of the location
 **/
const char *
gweather_location_get_english_sort_name (GWeatherLocation *loc)
{
    const char *english_name;
    g_autofree char *normalized = NULL;

    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);

    if (loc->_english_sort_name)
        return loc->_english_sort_name;

    english_name = gweather_location_get_english_name (loc);
    if (!english_name)
        return NULL;

    normalized = g_utf8_normalize (english_name, -1, G_NORMALIZE_ALL);
    loc->_english_sort_name = g_utf8_casefold (normalized, -1);

    return loc->_english_sort_name;
}

/**
 * gweather_location_get_level:
 * @loc: a #GWeatherLocation
 *
 * Gets @loc's level, from %GWEATHER_LOCATION_WORLD, to
 * %GWEATHER_LOCATION_WEATHER_STATION.
 *
 * Return value: @loc's level
 **/
GWeatherLocationLevel
gweather_location_get_level (GWeatherLocation *loc)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), GWEATHER_LOCATION_WORLD);

    return loc->level;
}

/**
 * gweather_location_level_to_string:
 * @level: a #GWeatherLocationLevel
 *
 * Returns the location level as a string, useful for debugging
 * purposes.
 *
 * Return value: a string
 **/
const char *
gweather_location_level_to_string (GWeatherLocationLevel level)
{
    switch (level) {
        case GWEATHER_LOCATION_WORLD:
            return "world";
        case GWEATHER_LOCATION_REGION:
            return "region";
        case GWEATHER_LOCATION_COUNTRY:
            return "country";
        case GWEATHER_LOCATION_ADM1:
            return "adm1";
        case GWEATHER_LOCATION_CITY:
            return "city";
        case GWEATHER_LOCATION_WEATHER_STATION:
            return "station";
        case GWEATHER_LOCATION_DETACHED:
            return "detached";
        case GWEATHER_LOCATION_NAMED_TIMEZONE:
            return "named-timezone";
        default:
            g_assert_not_reached ();
    }

    return NULL;
}

/**
 * gweather_location_get_parent:
 * @loc: a location
 *
 * Gets the location's parent.
 *
 * Return value: (transfer full) (nullable): the location's parent
 **/
GWeatherLocation *
gweather_location_get_parent (GWeatherLocation *loc)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);

    if (loc->_parent)
        return g_object_ref (loc->_parent);

    if (loc->level == GWEATHER_LOCATION_WORLD)
        return NULL;

    /* No database or root object */
    if (!loc->db || !IDX_VALID (loc->db_idx))
        return NULL;

    /* Note: We cannot use db_location_get_parent here in case this is an
     *       implicit nearest copy! */
    g_assert (IDX_VALID (loc->parent_idx) && loc->parent_idx != loc->db_idx);

    return location_ref_for_idx (loc->db, loc->parent_idx, NULL);
}

/**
 * gweather_location_next_child:
 * @loc: the location to iterate
 * @child: (transfer full) (nullable): the next child
 *
 * Allows iterating all children of a location.
 *
 * Pass `NULL` to get the first child, and any child to get the next one.
 *
 * Note that the reference to @child is taken, meaning iterating all
 * children is as simple as:
 *
 * ```c
 *   g_autoptr (GWeatherLocation) child = NULL;
 *   while ((child = gweather_location_next_child (location, child)))
 *     {
 *        // Do something with child
 *     }
 * ```
 *
 * Returns: (transfer full) (nullable): The next child, if one exists
 **/
GWeatherLocation *
gweather_location_next_child (GWeatherLocation *loc,
                              GWeatherLocation *_child)
{
    g_autoptr (GWeatherLocation) child = _child;
    DbArrayofuint16Ref children_ref;
    gsize length;
    gsize next_idx;

    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);

    /* Easy case, just look up the child and grab the next one. */
    if (loc->_children) {
        if (child == NULL) {
            if (loc->_children[0])
                return g_object_ref (loc->_children[0]);
            else
                return NULL;
        }

        for (gsize i = 0; loc->_children[i]; i++) {
            if (loc->_children[i] == child) {
                if (loc->_children[i + 1])
                    return g_object_ref (loc->_children[i + 1]);
                else
                    return NULL;
            }
        }

        goto invalid_child;
    }

    /* If we have a magic nearest child, iterate over that. */
    if (!env_no_nearest &&
        IDX_VALID (db_location_get_nearest (loc->ref))) {
        if (child && (!child->db || !IDX_VALID (child->db_idx) || child->parent_idx != loc->db_idx))
            goto invalid_child;

        if (child)
            return NULL;
        else
            return location_ref_for_idx (loc->db, db_location_get_nearest (loc->ref), loc);
    }

    if (!loc->db || !IDX_VALID (loc->db_idx)) {
        if (child)
            goto invalid_child;

        return NULL;
    }

    children_ref = db_location_get_children (loc->ref);
    length = db_arrayofuint16_get_length (children_ref);

    if (!child) {
        next_idx = 0;
    } else {
        gsize i;

        /* Find child index in DB. */
        for (i = 0; i < length; i++) {
            if (child->db_idx == db_arrayofuint16_get_at (children_ref, i))
                break;
        }

        if (i == length)
            goto invalid_child;

        next_idx = i + 1;
    }

    if (next_idx == length)
        return NULL;
    else
        return location_ref_for_idx (loc->db,
                                     db_arrayofuint16_get_at (children_ref, next_idx),
                                     NULL);

invalid_child:
    g_critical ("%s: Passed child %p is not a child of the given location %p", G_STRFUNC, loc, child);
    return NULL;
}

static void
foreach_city (GWeatherLocation *loc,
              GFunc callback,
              gpointer user_data,
              const char *country_code,
              GWeatherFilterFunc func,
              gpointer user_data_func)
{
    if (country_code) {
        const char *loc_country_code = gweather_location_get_country (loc);
        if (loc_country_code && (g_strcmp0 (loc_country_code, country_code) != 0))
            return;
    }

    if (func) {
        if (!func (loc, user_data_func))
            return;
    }

    if (loc->level == GWEATHER_LOCATION_CITY) {
        callback (loc, user_data);
    } else {
        g_autoptr (GWeatherLocation) child = NULL;

        while ((child = gweather_location_next_child (loc, child)))
            foreach_city (child, callback, user_data, country_code, func, user_data_func);
    }
}

struct FindNearestCityData
{
    double latitude;
    double longitude;
    GWeatherLocation *location;
    double distance;
};

struct ArgData
{
    double latitude;
    double longitude;
    GWeatherLocation *location;
    GTask *task;
};

typedef struct ArgData ArgData;

static double
location_distance (double lat1, double long1, double lat2, double long2)
{
    /* average radius of the earth in km */
    static const double radius = 6372.795;

    if (lat1 == lat2 && long1 == long2)
        return 0.0;

    return acos (cos (lat1) * cos (lat2) * cos (long1 - long2) +
                 sin (lat1) * sin (lat2)) *
           radius;
}

static void
find_nearest_city (GWeatherLocation *location,
                   gpointer user_data)
{
    struct FindNearestCityData *data = user_data;

    double distance = location_distance (location->latitude,
                                         location->longitude,
                                         data->latitude,
                                         data->longitude);

    if (data->location == NULL || data->distance > distance) {
        g_clear_object (&data->location);
        data->location = g_object_ref (location);
        data->distance = distance;
    }
}

/**
 * gweather_location_find_nearest_city:
 * @loc: The parent location, which will be searched recursively
 * @lat: Latitude, in degrees
 * @lon: Longitude, in degrees
 *
 * Finds the nearest city to the passed latitude and
 * longitude, among the descendants of @loc.
 *
 * The given location must be at most a %GWEATHER_LOCATION_ADM1 location.
 * This restriction may be lifted in a future version.
 *
 * Note that this function does not check if (@lat, @lon) fall inside
 * @loc, or are in the same region and time zone as the return value.
 *
 * Returns: (transfer full): the city closest to (@lat, @lon), in the
 *   region or administrative district of @loc.
 */
GWeatherLocation *
gweather_location_find_nearest_city (GWeatherLocation *loc,
                                     double lat,
                                     double lon)
{
    /* The data set really isn't too big. Don't concern ourselves
     * with a proper nearest neighbors search. Instead, just do
     * an O(n) search. */
    struct FindNearestCityData data;

    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);
    g_return_val_if_fail (loc->level < GWEATHER_LOCATION_CITY, NULL);

    lat = lat * M_PI / 180.0;
    lon = lon * M_PI / 180.0;

    data.latitude = lat;
    data.longitude = lon;
    data.location = NULL;
    data.distance = 0.0;

    foreach_city (loc, (GFunc) find_nearest_city, &data, NULL, NULL, NULL);

    return data.location;
}

/**
 * gweather_location_find_nearest_city_full:
 * @loc: the parent location, which will be searched recursively
 * @lat: Latitude, in degrees
 * @lon: Longitude, in degrees
 * @func: (scope notified) (nullable) (closure user_data): a function to iterate
 *   over the locations; the function must return `TRUE` to continue checking
 *   for the location, and `FALSE` to filter the location out
 * @user_data: for customization
 * @destroy: to destroy user_data
 *
 * Finds the nearest city to the passed latitude and
 * longitude, among the descendants of @loc.
 *
 * Supports the use of own filter function to filter out locations.
 *
 * Geocoding should be done on the application side if needed.
 *
 * @loc must be at most a %GWEATHER_LOCATION_ADM1 location.
 * This restriction may be lifted in a future version.
 *
 * Returns: (transfer full): the city closest to (@lat, @lon), in the
 *   region or administrative district of @loc with validation of
 *   filter function
 */
GWeatherLocation *
gweather_location_find_nearest_city_full (GWeatherLocation *loc,
                                          double lat,
                                          double lon,
                                          GWeatherFilterFunc func,
                                          gpointer user_data,
                                          GDestroyNotify destroy)
{
    /* The data set really isn't too big. Don't concern ourselves
     * with a proper nearest neighbors search. Instead, just do
     * an O(n) search. */
    struct FindNearestCityData data;

    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);
    g_return_val_if_fail ((loc->level < GWEATHER_LOCATION_CITY ||
                           loc->level == GWEATHER_LOCATION_NAMED_TIMEZONE),
                          NULL);

    lat = lat * M_PI / 180.0;
    lon = lon * M_PI / 180.0;

    data.latitude = lat;
    data.longitude = lon;
    data.location = NULL;
    data.distance = 0.0;

    foreach_city (loc, (GFunc) find_nearest_city, &data, NULL, func, user_data);

    destroy (user_data);

    return data.location;
}

static void
_got_place (GObject *source_object,
            GAsyncResult *result,
            gpointer user_data)
{
    ArgData *info = (user_data);
    GTask *task = info->task;
    GeocodePlace *place;
    GError *error = NULL;
    const char *country_code;
    struct FindNearestCityData data;

    place = geocode_reverse_resolve_finish (GEOCODE_REVERSE (source_object), result, &error);
    if (place == NULL) {
        g_task_return_error (task, error);
        g_slice_free (ArgData, info);
        g_object_unref (task);
        return;
    }
    country_code = geocode_place_get_country_code (place);

    data.latitude = info->latitude * M_PI / 180.0;
    data.longitude = info->longitude * M_PI / 180.0;
    data.location = NULL;
    data.distance = 0.0;

    foreach_city (info->location, (GFunc) find_nearest_city, &data, country_code, NULL, NULL);

    g_object_unref (info->location);
    g_slice_free (ArgData, info);

    if (data.location == NULL) {
        g_task_return_pointer (task, NULL, NULL);
    } else {
        GWeatherLocation *location =
            gweather_location_new_full (GWEATHER_LOCATION_DETACHED,
                                        data.location,
                                        geocode_place_get_town (place),
                                        TRUE,
                                        data.latitude,
                                        data.longitude);

        g_task_return_pointer (task, location, (GDestroyNotify) g_object_unref);
    }

    g_object_unref (task);
}

/**
 * gweather_location_detect_nearest_city: (finish-func detect_nearest_city_finish)
 * @loc: the parent location, which will be searched recursively
 * @lat: Latitude, in degrees
 * @lon: Longitude, in degrees
 * @cancellable: (nullable): a cancellable instance
 * @callback: callback function
 * @user_data: user data passed to @callback
 *
 * Initializes geocode reversing to find place for (@lat, @lon) coordinates.
 *
 * Calls the callback function passed by user when the result is ready.
 *
 * The given location must be at most a %GWEATHER_LOCATION_ADM1 location; this
 * restriction may be lifted in a future version.
 */
void
gweather_location_detect_nearest_city (GWeatherLocation *loc,
                                       double lat,
                                       double lon,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
    ArgData *data;
    GeocodeLocation *location;
    GeocodeReverse *reverse;
    GTask *task;

    g_return_if_fail (GWEATHER_IS_LOCATION (loc));
    g_return_if_fail (loc->level < GWEATHER_LOCATION_CITY || loc->level == GWEATHER_LOCATION_NAMED_TIMEZONE);

    location = geocode_location_new (lat, lon, GEOCODE_LOCATION_ACCURACY_CITY);
    reverse = geocode_reverse_new_for_location (location);

    task = g_task_new (NULL, cancellable, callback, user_data);

    data = g_slice_new0 (ArgData);
    data->latitude = lat;
    data->longitude = lon;
    data->location = g_object_ref (loc);
    data->task = task;

    geocode_reverse_resolve_async (reverse, cancellable, _got_place, data);
}

/**
 * gweather_location_detect_nearest_city_finish:
 * @result: the result of the asynchronous operation
 * @error: return location for an error
 *
 * Fetches the location from @result.
 *
 * Returns: (transfer full): Customized GWeatherLocation
 */
GWeatherLocation *
gweather_location_detect_nearest_city_finish (GAsyncResult *result,
                                              GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, NULL), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * gweather_location_has_coords:
 * @loc: a #GWeatherLocation
 *
 * Checks if @loc has valid latitude and longitude.
 *
 * Return value: %TRUE if @loc has valid latitude and longitude.
 **/
gboolean
gweather_location_has_coords (GWeatherLocation *loc)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), FALSE);

    return loc->latlon_valid;
}

/**
 * gweather_location_get_coords:
 * @loc: a #GWeatherLocation
 * @latitude: (out) (optional): the return location for the latitude
 * @longitude: (out) (optional): the return location for the longitude
 *
 * Gets @loc's coordinates.
 *
 * You must call [method@GWeather.Location.has_coords] before calling
 * this function.
 **/
void
gweather_location_get_coords (GWeatherLocation *loc,
                              double *latitude,
                              double *longitude)
{
    g_return_if_fail (GWEATHER_IS_LOCATION (loc));
    g_return_if_fail (loc->latlon_valid);

    if (latitude != NULL)
        *latitude = loc->latitude / M_PI * 180.0;
    if (longitude != NULL)
        *longitude = loc->longitude / M_PI * 180.0;
}

/**
 * gweather_location_get_distance:
 * @loc: a #GWeatherLocation
 * @loc2: a second #GWeatherLocation
 *
 * Determines the distance in kilometers between @loc and @loc2.
 *
 * Return value: the distance between @loc and @loc2.
 **/
double
gweather_location_get_distance (GWeatherLocation *loc,
                                GWeatherLocation *loc2)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), G_MAXDOUBLE);
    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), G_MAXDOUBLE);

    g_return_val_if_fail (loc->latlon_valid, G_MAXDOUBLE);
    g_return_val_if_fail (loc2->latlon_valid, G_MAXDOUBLE);

    return location_distance (loc->latitude,
                              loc->longitude,
                              loc2->latitude,
                              loc2->longitude);
}

/**
 * gweather_location_get_country:
 * @loc: a #GWeatherLocation
 *
 * Gets the ISO 3166 country code of the given location.
 *
 * For `GWEATHER_LOCATION_WORLD` and `GWEATHER_LOCATION_REGION`, this
 * function returns `NULL`.
 *
 * Return value: (nullable): the location's country code
 **/
const char *
gweather_location_get_country (GWeatherLocation *loc)
{
    g_autoptr (GWeatherLocation) s = NULL;

    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);

    ITER_UP (loc, s) {
        if (s->_country_code)
            return s->_country_code;

        if (s->db && IDX_VALID (s->db_idx)) {
            const char *country_code;
            country_code = EMPTY_TO_NULL (db_location_get_country_code (s->ref));
            if (country_code)
                return country_code;
        }
    }
    return NULL;
}

/**
 * gweather_location_get_timezone:
 * @loc: a #GWeatherLocation
 *
 * Gets the timezone associated with @loc, if known.
 *
 * Return value: (transfer none) (nullable): the location's timezone
 **/
GTimeZone *
gweather_location_get_timezone (GWeatherLocation *loc)
{
    g_autoptr (GWeatherLocation) s = NULL;

    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);

    if (loc->_timezone)
        return loc->_timezone;

    ITER_UP (loc, s) {
        if (!IDX_VALID (s->tz_hint_idx))
            continue;

        loc->_timezone = timezone_ref_for_idx (s->db, s->tz_hint_idx);

        return loc->_timezone;
    }

    return NULL;
}

/**
 * gweather_location_has_timezone:
 * @loc: the location
 *
 * Checks whether the location has a timezone.
 *
 * Returns: true if the location has a timezone; false otherwise
 */
gboolean
gweather_location_has_timezone (GWeatherLocation *loc)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), FALSE);

    if (loc->_timezone != NULL)
        return TRUE;

    g_autoptr (GWeatherLocation) s = NULL;

    ITER_UP (loc, s) {
        if (!IDX_VALID (s->tz_hint_idx))
            continue;

        loc->_timezone = timezone_ref_for_idx (s->db, s->tz_hint_idx);
        return TRUE;
    }

    return FALSE;
}

/**
 * gweather_location_get_timezone_str:
 * @loc: a #GWeatherLocation
 *
 * Gets the timezone associated with @loc, if known, as a string.
 *
 * Return value: (transfer none) (nullable): the location's timezone as
 *   a string
 **/
const char *
gweather_location_get_timezone_str (GWeatherLocation *loc)
{
    g_autoptr (GWeatherLocation) s = NULL;

    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);

    ITER_UP (loc, s) {
        if (s->_timezone)
            return g_time_zone_get_identifier (s->_timezone);

        if (s->db && IDX_VALID (s->tz_hint_idx)) {
            return db_world_timezones_entry_get_key (db_world_timezones_get_at (s->db->timezones_ref, s->tz_hint_idx));
        }
    }

    return NULL;
}

static void
add_timezones (GWeatherLocation *loc,
               GPtrArray *zones)
{
    /* NOTE: Only DB backed locations can have timezones */
    if (loc->db && IDX_VALID (loc->db_idx)) {
        DbArrayofuint16Ref ref;
        gsize len;

        ref = db_location_get_timezones (loc->ref);
        len = db_arrayofuint16_get_length (ref);
        for (gsize i = 0; i < len; i++) {
            guint16 tz_idx = db_arrayofuint16_get_at (ref, i);

            g_ptr_array_add (zones, timezone_ref_for_idx (loc->db, tz_idx));
        }
    }

    if (loc->level < GWEATHER_LOCATION_COUNTRY) {
        g_autoptr (GWeatherLocation) child = NULL;

        while ((child = gweather_location_next_child (loc, child)))
            add_timezones (child, zones);
    }
}

/**
 * gweather_location_get_timezones:
 * @loc: a #GWeatherLocation
 *
 * Gets an array of all timezones associated with any location under
 * @loc.
 *
 * Use gweather_location_free_timezones() to free this array.
 *
 * Return value: (transfer full) (array zero-terminated=1): the timezones
 *   associated with the location
 **/
GTimeZone **
gweather_location_get_timezones (GWeatherLocation *loc)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);

    GPtrArray *zones = g_ptr_array_new ();
    add_timezones (loc, zones);
    g_ptr_array_add (zones, NULL);

    return (GTimeZone **) g_ptr_array_free (zones, FALSE);
}

/**
 * gweather_location_free_timezones:
 * @loc: a #GWeatherLocation
 * @zones: (transfer full) (array zero-terminated=1): an array of timezones
 *   returned by [method@GWeather.Location.get_timezones]
 *
 * Frees the array of timezones returned by
 * gweather_location_get_timezones().
 */
void
gweather_location_free_timezones (GWeatherLocation *loc,
                                  GTimeZone **zones)
{
    g_return_if_fail (GWEATHER_IS_LOCATION (loc));
    g_return_if_fail (zones != NULL);

    for (int i = 0; zones[i]; i++)
        g_time_zone_unref (zones[i]);

    g_free (zones);
}

/**
 * gweather_location_get_code:
 * @loc: a #GWeatherLocation
 *
 * Gets the METAR station code associated with a
 * `GWEATHER_LOCATION_WEATHER_STATION` location.
 *
 * Return value: (nullable): the location's METAR station code
 */
const char *
gweather_location_get_code (GWeatherLocation *loc)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);

    if (loc->_station_code)
        return loc->_station_code;

    if (loc->db && IDX_VALID (loc->db_idx)) {
        return EMPTY_TO_NULL (db_location_get_metar_code (loc->ref));
    }

    return NULL;
}

/**
 * gweather_location_get_city_name:
 * @loc: a #GWeatherLocation
 *
 * Retrieves the city name for the given location.
 *
 * For a `GWEATHER_LOCATION_CITY` or `GWEATHER_LOCATION_DETACHED` location,
 * this method is equivalent to [method@GWeather.Location.get_name].
 *
 * For a `GWEATHER_LOCATION_WEATHER_STATION` location, this is equivalent to
 * calling [method@GWeather.Location.get_name] on the location's parent.
 *
 * For other locations this method will return `NULL`.
 *
 * Return value: (transfer full) (nullable): the city name of the location
 */
char *
gweather_location_get_city_name (GWeatherLocation *loc)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);

    if (loc->level == GWEATHER_LOCATION_CITY ||
        loc->level == GWEATHER_LOCATION_DETACHED) {
        return g_strdup (gweather_location_get_name (loc));
    } else {
        g_autoptr (GWeatherLocation) parent = NULL;
        parent = gweather_location_get_parent (loc);

        if (loc->level == GWEATHER_LOCATION_WEATHER_STATION &&
            parent &&
            parent->level == GWEATHER_LOCATION_CITY) {
            return g_strdup (gweather_location_get_name (parent));
        }
    }

    return NULL;
}

/**
 * gweather_location_get_country_name:
 * @loc: a #GWeatherLocation
 *
 * Retrieves the country name for the given location.
 *
 * For a `GWEATHER_LOCATION_COUNTRY` location, this is equivalent to
 * [method@GWeather.Location.get_name].
 *
 * For a `GWEATHER_LOCATION_REGION` and `GWEATHER_LOCATION_WORLD` location,
 * this method will return `NULL`.
 *
 * For other location levels, this method will find the parent
 * `GWEATHER_LOCATION_COUNTRY` and return its name.
 *
 * Return value: (transfer full) (nullable): the location's country name
 */
char *
gweather_location_get_country_name (GWeatherLocation *loc)
{
    g_autoptr (GWeatherLocation) country = NULL;

    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);

    ITER_UP (loc, country) {
        if (country->level == GWEATHER_LOCATION_COUNTRY)
            return g_strdup (gweather_location_get_name (country));
    }

    return NULL;
}

void
_gweather_location_update_weather_location (GWeatherLocation *gloc,
                                            WeatherLocation *loc)
{
    char *code = NULL, *zone = NULL, *radar = NULL, *tz_hint = NULL;
    gboolean latlon_valid = FALSE;
    gdouble lat = DBL_MAX, lon = DBL_MAX;
    g_autoptr (GWeatherLocation) start = NULL;
    g_autoptr (GWeatherLocation) l = NULL;

    if (gloc->level == GWEATHER_LOCATION_CITY) {
        GWeatherLocation *first_child;
        first_child = gweather_location_next_child (gloc, NULL);

        if (first_child)
            start = first_child;
    }
    if (!start)
        start = g_object_ref (gloc);

    ITER_UP (start, l) {
        if (!code)
            code = g_strdup (gweather_location_get_code (l));
        if (!zone && l->db && IDX_VALID (l->db_idx))
            zone = g_strdup (EMPTY_TO_NULL (db_location_get_forecast_zone (l->ref)));
        if (!radar && l->db && IDX_VALID (l->db_idx))
            radar = g_strdup (EMPTY_TO_NULL (db_location_get_radar (l->ref)));
        if (!tz_hint && l->db && IDX_VALID (l->tz_hint_idx))
            tz_hint = g_strdup (db_world_timezones_entry_get_key (db_world_timezones_get_at (l->db->timezones_ref, l->tz_hint_idx)));
        if (!latlon_valid) {
            lat = l->latitude;
            lon = l->longitude;
            latlon_valid = l->latlon_valid;
        }

        if (code && zone && radar && tz_hint && latlon_valid)
            break;
    }

    loc->name = g_strdup (gweather_location_get_name (gloc)),
    loc->code = code;
    loc->zone = zone;
    loc->radar = radar;
    /* This walks the hierarchy again ... */
    loc->country_code = g_strdup (gweather_location_get_country (start));
    loc->tz_hint = tz_hint;

    loc->latlon_valid = latlon_valid;
    loc->latitude = lat;
    loc->longitude = lon;
}

/**
 * gweather_location_find_by_station_code:
 * @world: a #GWeatherLocation at the world level
 * @station_code: a 4 letter METAR code
 *
 * Retrieves the weather station identifier by @station_code.
 *
 * Note that multiple instances of the same weather station can exist
 * in the database, and this function will return any of them, so this
 * not usually what you want.
 *
 * See [method@GWeather.Location.deserialize] to recover a stored location.
 *
 * Returns: (transfer full) (nullable): a weather station level location
 *   for the given station code, or `NULL` if none exists in the database
 */
GWeatherLocation *
gweather_location_find_by_station_code (GWeatherLocation *world,
                                        const gchar *station_code)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (world), NULL);

    if (!world->db)
        return NULL;

    DbWorldLocByMetarRef loc_by_metar = db_world_get_loc_by_metar (world->db->world);

    guint16 idx = 0;
    if (!db_world_loc_by_metar_lookup (loc_by_metar, station_code, NULL, &idx))
        return NULL;

    return location_ref_for_idx (world->db, idx, NULL);
}

/**
 * gweather_location_find_by_country_code:
 * @world: a #GWeatherLocation at the world
 * @country_code: a country code
 *
 * Retrieves the country identified by the specified ISO 3166 code,
 * if present in the database.
 *
 * Returns: (transfer full): a country level #GWeatherLocation, or %NULL.
 */
GWeatherLocation *
gweather_location_find_by_country_code (GWeatherLocation *world,
                                        const gchar *country_code)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (world), NULL);

    if (!world->db)
        return NULL;

    DbWorldLocByCountryRef loc_by_country = db_world_get_loc_by_country (world->db->world);

    guint16 idx = 0;
    if (!db_world_loc_by_country_lookup (loc_by_country, country_code, NULL, &idx))
        return NULL;

    return location_ref_for_idx (world->db, idx, NULL);
}

/**
 * gweather_location_equal:
 * @one: a #GWeatherLocation
 * @two: another #GWeatherLocation
 *
 * Compares two #GWeatherLocation and sees if they represent the same
 * place.
 * It is only legal to call this for cities, weather stations or
 * detached locations.
 * Note that this function only checks for geographical characteristics,
 * such as coordinates and METAR code. It is still possible that the two
 * locations belong to different worlds (in which case care must be
 * taken when passing them GWeatherLocationEntry and GWeatherInfo), or
 * if one is them is detached it could have a custom name.
 *
 * Returns: %TRUE if the two locations represent the same place as
 *          far as libgweather can tell, and %FALSE otherwise.
 */
gboolean
gweather_location_equal (GWeatherLocation *one,
                         GWeatherLocation *two)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (one), FALSE);
    g_return_val_if_fail (GWEATHER_IS_LOCATION (two), FALSE);

    g_autoptr (GWeatherLocation) p1 = NULL, p2 = NULL;
    int level;

    if (one == two)
        return TRUE;

    p1 = gweather_location_get_parent (one);
    p2 = gweather_location_get_parent (two);

    if (one->level != two->level &&
        one->level != GWEATHER_LOCATION_DETACHED &&
        two->level != GWEATHER_LOCATION_DETACHED)
        return FALSE;

    level = one->level;
    if (level == GWEATHER_LOCATION_DETACHED)
        level = two->level;

    if (level == GWEATHER_LOCATION_COUNTRY)
        return g_strcmp0 (gweather_location_get_country (one),
                          gweather_location_get_country (two));

    if (level == GWEATHER_LOCATION_ADM1) {
        if (g_strcmp0 (gweather_location_get_english_sort_name (one), gweather_location_get_english_sort_name (two)) != 0)
            return FALSE;

        return p1 && p2 &&
               gweather_location_equal (p1, p2);
    }

    if (g_strcmp0 (gweather_location_get_code (one),
                   gweather_location_get_code (two)) != 0)
        return FALSE;

    if (one->level != GWEATHER_LOCATION_DETACHED &&
        two->level != GWEATHER_LOCATION_DETACHED &&
        !gweather_location_equal (p1, p2))
        return FALSE;

    return ABS (one->latitude - two->latitude) < EPSILON &&
           ABS (one->longitude - two->longitude) < EPSILON;
}

/* ------------------- serialization ------------------------------- */

#define FORMAT 2

static GVariant *
gweather_location_format_two_serialize (GWeatherLocation *location)
{
    g_autoptr (GWeatherLocation) real_loc = NULL;
    g_autoptr (GWeatherLocation) parent = NULL;
    const char *name;
    const char *station_code;
    gboolean is_city;
    GVariantBuilder latlon_builder;
    GVariantBuilder parent_latlon_builder;

    name = gweather_location_get_english_name (location);

    /* Normalize location to be a weather station or detached */
    if (location->level == GWEATHER_LOCATION_CITY) {
        real_loc = gweather_location_next_child (location, NULL);
        is_city = TRUE;
    } else {
        is_city = FALSE;
    }
    if (!real_loc)
        real_loc = g_object_ref (location);

    parent = gweather_location_get_parent (real_loc);

    g_variant_builder_init (&latlon_builder, G_VARIANT_TYPE ("a(dd)"));
    if (real_loc->latlon_valid)
        g_variant_builder_add (&latlon_builder, "(dd)", real_loc->latitude, real_loc->longitude);

    g_variant_builder_init (&parent_latlon_builder, G_VARIANT_TYPE ("a(dd)"));
    if (parent && parent->latlon_valid)
        g_variant_builder_add (&parent_latlon_builder, "(dd)", parent->latitude, parent->longitude);

    station_code = gweather_location_get_code (real_loc);
    return g_variant_new ("(ssba(dd)a(dd))",
                          name,
                          station_code ? station_code : "",
                          is_city,
                          &latlon_builder,
                          &parent_latlon_builder);
}

static GWeatherLocation *
gweather_location_common_deserialize (GWeatherLocation *world,
                                      const char *name,
                                      const char *station_code,
                                      gboolean is_city,
                                      gboolean latlon_valid,
                                      gdouble latitude,
                                      gdouble longitude,
                                      gboolean parent_latlon_valid,
                                      gdouble parent_latitude,
                                      gdouble parent_longitude)
{
    g_autoptr (GWeatherLocation) by_station_code = NULL;
    DbWorldLocByMetarRef loc_by_metar;
    GWeatherLocation *found;
    gsize i;

    /* Since weather stations are no longer attached to cities, first try to
       find what claims to be a city by name and coordinates */
    if (is_city && latitude && longitude) {
        gssize idx = _gweather_find_nearest_city_index (latitude, longitude);

        if (idx >= 0)
            found = location_ref_for_idx (world->db, idx, NULL);
        else
            found = NULL;

        if (found && (g_strcmp0 (name, gweather_location_get_english_name (found)) == 0 ||
                      g_strcmp0 (name, gweather_location_get_name (found)) == 0))
            return g_steal_pointer (&found);
    }

    if (station_code[0] == '\0') {
        return gweather_location_new_full (GWEATHER_LOCATION_DETACHED,
                                           NULL,
                                           name,
                                           latlon_valid,
                                           latitude,
                                           longitude);
    }

    /* Lookup by station code, this may return NULL. */
    by_station_code = gweather_location_find_by_station_code (world, station_code);

    /* A station code beginning with @ indicates a named timezone entry, just
     * return it directly
     */
    if (station_code[0] == '@')
        return g_steal_pointer (&by_station_code);

    /* If we don't have coordinates, fallback immediately to making up
     * a location
     */
    if (!latlon_valid)
        return by_station_code
                 ? gweather_location_new_full (GWEATHER_LOCATION_DETACHED,
                                               g_steal_pointer (&by_station_code),
                                               name,
                                               FALSE,
                                               0,
                                               0)
                 : NULL;

    found = NULL;
    loc_by_metar = db_world_get_loc_by_metar (world->db->world);

    for (i = 0; i < db_world_loc_by_metar_get_length (loc_by_metar); i++) {
        g_autoptr (GWeatherLocation) ws = NULL, city = NULL;
        /* Skip if the metar code does not match */
        if (!g_str_equal (station_code, db_world_loc_by_metar_entry_get_key (db_world_loc_by_metar_get_at (loc_by_metar, i))))
            continue;

        /* Be lazy and allocate the location */
        ws = location_ref_for_idx (world->db, db_world_loc_by_metar_entry_get_value (db_world_loc_by_metar_get_at (loc_by_metar, i)), NULL);

        if (!ws->latlon_valid ||
            ABS (ws->latitude - latitude) >= EPSILON ||
            ABS (ws->longitude - longitude) >= EPSILON)
            /* Not what we're looking for... */
            continue;

        city = gweather_location_get_parent (ws);

        /* If we can't check for the latitude and longitude
	   of the parent, we just assume we found what we needed
	*/
        if ((!parent_latlon_valid || !city || !city->latlon_valid) ||
            (ABS (parent_latitude - city->latitude) < EPSILON &&
             ABS (parent_longitude - city->longitude) < EPSILON)) {

            /* Found! Now check which one we want (ws or city) and the name... */
            if (is_city)
                found = city;
            else
                found = ws;

            if (found == NULL) {
                /* Oops! There is no city for this weather station! */
                continue;
            }

            if (name == NULL ||
                g_str_equal (name, gweather_location_get_english_name (found)) ||
                g_str_equal (name, gweather_location_get_name (found)))
                found = g_object_ref (found);
            else
                found = gweather_location_new_full (GWEATHER_LOCATION_DETACHED,
                                                    g_object_ref (ws),
                                                    name,
                                                    TRUE,
                                                    latitude,
                                                    longitude);

            return found;
        }
    }

    /* No weather station matches the serialized data, let's pick
       one at random from the station code list */
    if (by_station_code)
        return gweather_location_new_full (GWEATHER_LOCATION_DETACHED,
                                           g_steal_pointer (&by_station_code),
                                           name,
                                           TRUE,
                                           latitude,
                                           longitude);
    else
        return NULL;
}

static GWeatherLocation *
gweather_location_format_one_deserialize (GWeatherLocation *world,
                                          GVariant *variant)
{
    const char *name;
    const char *station_code;
    gboolean is_city, latlon_valid, parent_latlon_valid;
    gdouble latitude, longitude, parent_latitude, parent_longitude;

    /* This one instead is a critical, because format is specified in
       the containing variant */
    g_return_val_if_fail (g_variant_is_of_type (variant,
                                                G_VARIANT_TYPE ("(ssbm(dd)m(dd))")),
                          NULL);

    g_variant_get (variant,
                   "(&s&sbm(dd)m(dd))",
                   &name,
                   &station_code,
                   &is_city,
                   &latlon_valid,
                   &latitude,
                   &longitude,
                   &parent_latlon_valid,
                   &parent_latitude,
                   &parent_longitude);

    return gweather_location_common_deserialize (world,
                                                 name,
                                                 station_code,
                                                 is_city,
                                                 latlon_valid,
                                                 latitude,
                                                 longitude,
                                                 parent_latlon_valid,
                                                 parent_latitude,
                                                 parent_longitude);
}

static GWeatherLocation *
gweather_location_format_two_deserialize (GWeatherLocation *world,
                                          GVariant *variant)
{
    const char *name;
    const char *station_code;
    GVariant *latlon_variant;
    GVariant *parent_latlon_variant;
    gboolean is_city, latlon_valid, parent_latlon_valid;
    gdouble latitude, longitude, parent_latitude, parent_longitude;

    /* This one instead is a critical, because format is specified in
       the containing variant */
    g_return_val_if_fail (g_variant_is_of_type (variant,
                                                G_VARIANT_TYPE ("(ssba(dd)a(dd))")),
                          NULL);

    g_variant_get (variant, "(&s&sb@a(dd)@a(dd))", &name, &station_code, &is_city, &latlon_variant, &parent_latlon_variant);

    if (g_variant_n_children (latlon_variant) > 0) {
        latlon_valid = TRUE;
        g_variant_get_child (latlon_variant, 0, "(dd)", &latitude, &longitude);
    } else {
        latlon_valid = FALSE;
        latitude = 0;
        longitude = 0;
    }

    if (g_variant_n_children (parent_latlon_variant) > 0) {
        parent_latlon_valid = TRUE;
        g_variant_get_child (parent_latlon_variant, 0, "(dd)", &parent_latitude, &parent_longitude);
    } else {
        parent_latlon_valid = FALSE;
        parent_latitude = 0;
        parent_longitude = 0;
    }

    g_variant_unref (latlon_variant);
    g_variant_unref (parent_latlon_variant);

    return gweather_location_common_deserialize (world,
                                                 name,
                                                 station_code,
                                                 is_city,
                                                 latlon_valid,
                                                 latitude,
                                                 longitude,
                                                 parent_latlon_valid,
                                                 parent_latitude,
                                                 parent_longitude);
}

/**
 * gweather_location_serialize:
 * @loc: a city, weather station or detached #GWeatherLocation
 *
 * Transforms a #GWeatherLocation into a #GVariant, in a way that
 * calling gweather_location_deserialize() will hold an equivalent
 * #GWeatherLocation.
 * The resulting variant can then be stored into GSettings or on disk.
 * This call is only valid for cities, weather stations and detached
 * locations.
 * The format of the resulting #GVariant is private to libgweather,
 * and it is subject to change. You should use the "v" format in GSettings,
 * to ensure maximum compatibility with future versions of the library.
 *
 * Returns: (transfer none): the serialization of @location.
 */
GVariant *
gweather_location_serialize (GWeatherLocation *loc)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION (loc), NULL);
    g_return_val_if_fail (loc->level >= GWEATHER_LOCATION_CITY, NULL);

    return g_variant_new ("(uv)", FORMAT, gweather_location_format_two_serialize (loc));
}

/**
 * gweather_location_deserialize:
 * @world: a world-level #GWeatherLocation
 * @serialized: the #GVariant representing the #GWeatherLocation
 *
 * This call undoes the effect of gweather_location_serialize(), that
 * is, it turns a #GVariant into a #GWeatherLocation. The conversion
 * happens in the context of @world (i.e, for a city or weather station,
 * the resulting location will be attached to a administrative division,
 * country and region as appropriate).
 *
 * Returns: (transfer full): the deserialized location.
 */
GWeatherLocation *
gweather_location_deserialize (GWeatherLocation *world,
                               GVariant *serialized)
{
    GVariant *v;
    GWeatherLocation *loc;
    int format;

    g_return_val_if_fail (GWEATHER_IS_LOCATION (world), NULL);
    g_return_val_if_fail (serialized != NULL, NULL);

    /* This is not a critical error, because the serialization format
       is not public, so apps can't check this before calling */
    if (!g_variant_is_of_type (serialized, G_VARIANT_TYPE ("(uv)")))
        return NULL;

    g_variant_get (serialized, "(uv)", &format, &v);

    if (format == 1)
        loc = gweather_location_format_one_deserialize (world, v);
    else if (format == 2)
        loc = gweather_location_format_two_deserialize (world, v);
    else
        loc = NULL;

    g_variant_unref (v);
    return loc;
}

/**
 * gweather_location_new_detached: (constructor)
 * @name: the user visible location name
 * @icao: (nullable): the ICAO code of the location
 * @latitude: the latitude of the location
 * @longitude: the longitude of the location
 *
 * Construct a new location from the given data, supplementing
 * any missing information from the static database.
 *
 * Returns: (transfer full): the newly created detached location
 */
GWeatherLocation *
gweather_location_new_detached (const char *name,
                                const char *icao,
                                double latitude,
                                double longitude)
{
    g_autoptr (GWeatherLocation) world = NULL;

    g_return_val_if_fail (name != NULL, NULL);

    if (*name == 0)
        name = NULL;

    world = gweather_location_get_world ();
    if (G_UNLIKELY (world == NULL)) {
        return NULL;
    }

    if (icao != NULL) {
        return gweather_location_common_deserialize (world, name, icao, FALSE, TRUE, latitude, longitude, FALSE, 0, 0);
    } else {
        g_autoptr (GWeatherLocation) city =
            gweather_location_find_nearest_city (world, latitude, longitude);

        latitude = DEGREES_TO_RADIANS (latitude);
        longitude = DEGREES_TO_RADIANS (longitude);

        return gweather_location_new_full (GWEATHER_LOCATION_DETACHED,
                                           g_steal_pointer (&city),
                                           name,
                                           TRUE,
                                           latitude,
                                           longitude);
    }
}
