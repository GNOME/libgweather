/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gweather-location.c - Location-handling code
 *
 * Copyright 2008, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <math.h>
#include <locale.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libxml/xmlreader.h>
#include <geocode-glib/geocode-glib.h>

#include "gweather-timezone.h"
#include "gweather-private.h"

/* This is the precision of coordinates in the database */
#define EPSILON 0.000001

/* This is the maximum distance for which we will attach an
 * airport to a city, see also test_distance() */
#define AIRPORT_MAX_DISTANCE 100.0

static inline GWeatherLocation*
_iter_up(GWeatherLocation *loc)
{
    GWeatherLocation *tmp;

    tmp = gweather_location_get_parent (loc);
    gweather_location_unref (loc);
    return tmp;
}
#define ITER_UP(start, _p) for ((_p) = gweather_location_ref (start); (_p); (_p) = _iter_up(_p))

/**
 * SECTION:gweatherlocation
 * @Title: GWeatherLocation
 *
 * A #GWeatherLocation represents a "location" of some type known to
 * libgweather; anything from a single weather station to the entire
 * world. See #GWeatherLocationLevel for information about how the
 * hierarchy of locations works.
 */

/**
 * GWeatherLocationLevel:
 * @GWEATHER_LOCATION_WORLD: A location representing the entire world.
 * @GWEATHER_LOCATION_REGION: A location representing a continent or
 * other top-level region.
 * @GWEATHER_LOCATION_COUNTRY: A location representing a "country" (or
 * other geographic unit that has an ISO-3166 country code)
 * @GWEATHER_LOCATION_ADM1: A location representing a "first-level
 * administrative division"; ie, a state, province, or similar
 * division.
 * @GWEATHER_LOCATION_CITY: A location representing a city
 * @GWEATHER_LOCATION_WEATHER_STATION: A location representing a
 * weather station.
 * @GWEATHER_LOCATION_DETACHED: A location that is detached from the
 * database, for example because it was loaded from external storage
 * and could not be fully recovered. The parent of this location is
 * the nearest weather station.
 * @GWEATHER_LOCATION_NAMED_TIMEZONE: A location representing a named
 * or special timezone in the world, such as UTC
 *
 * The size/scope of a particular #GWeatherLocation.
 *
 * Locations form a hierarchy, with a %GWEATHER_LOCATION_WORLD
 * location at the top, divided into regions or countries, and so on.
 * Countries may or may not be divided into "adm1"s, and "adm1"s may
 * or may not be divided into "adm2"s. A city will have at least one,
 * and possibly several, weather stations inside it. Weather stations
 * will never appear outside of cities.
 *
 * Building a database with gweather_location_get_world() will never
 * create detached instances, but deserializing might.
 **/

static GWeatherLocation *
location_new (GWeatherLocationLevel level)
{
    GWeatherLocation *loc;

    loc = g_slice_new0 (GWeatherLocation);
    loc->latitude = loc->longitude = DBL_MAX;
    loc->level = level;
    loc->ref_count = 1;
    loc->db_idx = INVALID_IDX;
    loc->tz_hint_idx = INVALID_IDX;

    return loc;
}

static void add_timezones (GWeatherLocation *loc, GPtrArray *zones);

static GWeatherLocation *
location_ref_for_idx (GWeatherDb       *db,
		      guint16           idx,
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
	    return gweather_location_ref (loc);
	}
    }

    ref = db_arrayof_location_get_at (db->locations_ref, idx);
    loc = location_new (db_location_get_level (ref));
    loc->db = db;
    loc->db_idx = idx;
    loc->ref = ref;

    /* Override parent information for "nearest" copies. */
    if (nearest_of)
	loc->parent_idx = nearest_of->db_idx;
    else
	loc->parent_idx = db_location_get_parent (ref);

    loc->tz_hint_idx = db_location_get_tz_hint (ref);

    loc->latitude = db_coordinate_get_lat (db_location_get_coordinates (ref));
    loc->longitude = db_coordinate_get_lon (db_location_get_coordinates (ref));
    loc->latlon_valid = isfinite(loc->latitude) && isfinite(loc->longitude);

    /* Note, we used to sort locations by distance (for cities) and name;
     * Distance sorting is done in the variant already,
     * name sorting however needs translations and is not done anymore. */

    /* Store a weak reference in the cache.
     * Implicit "nearest" copies do not have a weak reference, they simply
     * belong to the parent. */
    if (!nearest_of)
	g_ptr_array_index (db->locations, idx) = loc;

    return loc;
}

static GWeatherDb *world_db;

void
_gweather_location_reset_world (void)
{
	gsize i;
	g_return_if_fail (world_db);

	/* At this point, we had a leak if the caches are not completely empty. */
	for (i = 0; i < world_db->locations->len; i++) {
		if (G_UNLIKELY (g_ptr_array_index (world_db->locations, i) != NULL)) {
			g_warning ("Location with index %li and name %s is still referenced!",
				   i, gweather_location_get_name (g_ptr_array_index (world_db->locations, i)));
			g_assert_not_reached ();
		}
	}
	for (i = 0; i < world_db->timezones->len; i++) {
		if (G_UNLIKELY (g_ptr_array_index (world_db->timezones, i) != NULL)) {
			g_warning ("Timezone with index %li and tzid %s is still referenced!",
				   i, gweather_timezone_get_tzid (g_ptr_array_index (world_db->timezones, i)));
			g_assert_not_reached ();
		}
	}
}

/**
 * gweather_location_get_world:
 *
 * Obtains the shared #GWeatherLocation of type %GWEATHER_LOCATION_WORLD,
 * representing a hierarchy containing all of the locations from
 * Locations.xml.
 *
 * Prior to version 40 no reference was returned.
 *
 * Return value: (allow-none) (transfer full): a %GWEATHER_LOCATION_WORLD
 * location, or %NULL if Locations.xml could not be found or could not be parsed.
 **/
GWeatherLocation *
gweather_location_get_world ()
{
    g_autoptr(GError) error = NULL;
    GMappedFile *map;

    if (!world_db) {
        const char *locations_path;
        g_autofree char *filename = NULL;
        time_t now;
        struct tm tm;

        locations_path = g_getenv ("LIBGWEATHER_LOCATIONS_PATH");
        if (locations_path) {
            filename = g_strdup (locations_path);
            if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
		g_warning ("User specified database %s does not exist", filename);
		g_clear_pointer (&filename, g_free);
	    }
        }

        if (!filename)
	    filename = g_build_filename (GWEATHER_BIN_LOCATION_DIR, "Locations.bin", NULL);

	map = g_mapped_file_new (filename, FALSE, &error);
	if (!map) {
	    g_warning ("Faile to open database %s: %s", filename, error->message);
	    return NULL;
	}

	world_db = g_new0 (GWeatherDb, 1);
	world_db->map = map;
	world_db->world = db_world_from_data (g_mapped_file_get_contents (map), g_mapped_file_get_length (map));
	/* This is GWthDB01 */
	if (db_world_get_magic (world_db->world) != 0x5747687442443130) {
	    g_mapped_file_unref (world_db->map);
	    g_free (world_db);
	    return NULL;
	}

	world_db->locations_ref = db_world_get_locations (world_db->world);
	world_db->timezones_ref = db_world_get_timezones (world_db->world);

	world_db->locations = g_ptr_array_new ();
	world_db->timezones = g_ptr_array_new ();

	g_ptr_array_set_size (world_db->locations, db_arrayof_location_get_length (world_db->locations_ref));
	g_ptr_array_set_size (world_db->timezones, db_world_timezones_get_length (world_db->timezones_ref));

	/* Get timestamps for the start and end of this year.
	 * This is used to parse timezone information. */
	now = time (NULL);
	tm = *gmtime (&now);
	tm.tm_mon = 0;
	tm.tm_mday = 1;
	tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
	world_db->year_start = mktime (&tm);
	tm.tm_year++;
	world_db->year_end = mktime (&tm);
    }

    return location_ref_for_idx (world_db, 0, NULL);
}

/**
 * gweather_location_ref:
 * @loc: a #GWeatherLocation
 *
 * Adds 1 to @loc's reference count.
 *
 * Return value: @loc
 **/
GWeatherLocation *
gweather_location_ref (GWeatherLocation *loc)
{
    g_return_val_if_fail (loc != NULL, NULL);

    loc->ref_count++;
    return loc;
}

/**
 * gweather_location_unref:
 * @loc: a #GWeatherLocation
 *
 * Subtracts 1 from @loc's reference count, and frees it if the
 * reference count reaches 0.
 **/
void
gweather_location_unref (GWeatherLocation *loc)
{
    g_return_if_fail (loc != NULL);

    int i;

    if (--loc->ref_count)
	return;

    /* Remove weak reference from DB object; but only if it points to us.
     * It may point elsewhere if we are an implicit nearest child. */
    if (loc->db && g_ptr_array_index (loc->db->locations, loc->db_idx) == loc)
        g_ptr_array_index (loc->db->locations, loc->db_idx) = NULL;

    g_free (loc->_english_name);
    g_free (loc->_local_name);
    g_free (loc->_local_sort_name);
    g_free (loc->_english_sort_name);
    g_free (loc->_country_code);
    g_free (loc->_station_code);

    if (loc->_children) {
	for (i = 0; loc->_children[i]; i++) {
	    gweather_location_unref (loc->_children[i]);
	}
	g_free (loc->_children);
    }

    g_clear_pointer (&loc->_parent, gweather_location_unref);
    g_clear_pointer (&loc->_timezone, gweather_timezone_unref);

    g_slice_free (GWeatherLocation, loc);
}

GType
gweather_location_get_type (void)
{
    static volatile gsize type_volatile = 0;

    if (g_once_init_enter (&type_volatile)) {
	GType type = g_boxed_type_register_static (
	    g_intern_static_string ("GWeatherLocation"),
	    (GBoxedCopyFunc) gweather_location_ref,
	    (GBoxedFreeFunc) gweather_location_unref);
	g_once_init_leave (&type_volatile, type);
    }
    return type_volatile;
}

/**
 * gweather_location_get_name:
 * @loc: a #GWeatherLocation
 *
 * Gets @loc's name, localized into the current language.
 *
 * Return value: @loc's name
 **/
const char *
gweather_location_get_name (GWeatherLocation *loc)
{
    g_return_val_if_fail (loc != NULL, NULL);

    if (loc->_local_name)
	return loc->_local_name;

    if (loc->db && IDX_VALID (loc->db_idx)) {
	const char *english_name;
	const char *msgctxt;
	english_name = EMPTY_TO_NULL (db_i18n_get_str (db_location_get_name (loc->ref)));
	msgctxt = EMPTY_TO_NULL (db_i18n_get_msgctxt (db_location_get_name (loc->ref)));

	if (msgctxt) {
	    loc->_local_name = g_strdup (g_dpgettext2 ("libgweather-locations",
						       msgctxt, english_name));
	} else {
	    loc->_local_name = g_strdup (g_dgettext ("libgweather-locations", english_name));
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
 * Gets @loc's "sort name", which is the name after having
 * g_utf8_normalize() (with %G_NORMALIZE_ALL) and g_utf8_casefold()
 * called on it. You can use this to sort locations, or to comparing
 * user input against a location name.
 *
 * Return value: @loc's sort name
 **/
const char *
gweather_location_get_sort_name (GWeatherLocation *loc)
{
    const char *local_name;
    g_autofree char *normalized = NULL;
    g_return_val_if_fail (loc != NULL, NULL);

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
 * Gets @loc's English name.
 *
 * Return value: @loc's English name
 * Since: 3.36
 **/
const char *
gweather_location_get_english_name (GWeatherLocation *loc)
{
    g_return_val_if_fail (loc != NULL, NULL);

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
 * Gets @loc's english "sort name", which is the english name after having
 * g_utf8_normalize() (with %G_NORMALIZE_ALL) and g_utf8_casefold()
 * called on it. You can use this to sort locations, or to comparing
 * user input against a location name.
 *
 * Return value: @loc's English name for sorting
 * Since: 3.38
 **/
const char *
gweather_location_get_english_sort_name (GWeatherLocation *loc)
{
    const char *english_name;
    g_autofree char *normalized = NULL;
    g_return_val_if_fail (loc != NULL, NULL);

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
    g_return_val_if_fail (loc != NULL, GWEATHER_LOCATION_WORLD);
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
        g_assert_not_reached();
    }

    return NULL;
}

/**
 * gweather_location_get_parent:
 * @loc: a #GWeatherLocation
 *
 * Gets @loc's parent location.
 *
 * Prior to version 40 no reference was returned.
 *
 * Return value: (transfer full) (allow-none): @loc's parent, or %NULL
 * if @loc is a %GWEATHER_LOCATION_WORLD node.
 **/
GWeatherLocation *
gweather_location_get_parent (GWeatherLocation *loc)
{
    g_return_val_if_fail (loc != NULL, NULL);

    if (loc->_parent)
	return gweather_location_ref (loc->_parent);

    if (loc->level == GWEATHER_LOCATION_WORLD)
	return NULL;

    /* No database or root object */
    if (!loc->db || !IDX_VALID(loc->db_idx))
	return NULL;

    /* Note: We cannot use db_location_get_parent here in case this is an
     *       implicit nearest copy! */
    g_assert (IDX_VALID(loc->parent_idx) && loc->parent_idx != loc->db_idx);
    return location_ref_for_idx (loc->db, loc->parent_idx, NULL);
}

/**
 * gweather_location_next_child:
 * @loc: a #GWeatherLocation
 * @child: (transfer full) (nullable): The child
 *
 * Allows iterating all children. Pass %NULL to get the first child,
 * and any child to get the next one. Note that the reference to @child
 * is taken, meaning iterating all children is as simple as:
 *
 * |[<!-- language="C" -->
 *   g_autoptr(GWeatherLocation) child = NULL;
 *   while ((child = gweather_location_next_child (location, child)))
 *     {
 *        // Do something with child
 *     }
 * ]|
 *
 * Returns: (transfer full) (nullable): The next child, or %NULL
 *
 * Since: 40
 **/
GWeatherLocation*
gweather_location_next_child  (GWeatherLocation  *loc, GWeatherLocation  *_child)
{
    g_autoptr(GWeatherLocation) child = _child;
    DbArrayofuint16Ref children_ref;
    gsize length;
    gsize next_idx;
    gsize i;

    g_return_val_if_fail (loc != NULL, NULL);

    /* Easy case, just look up the child and grab the next one. */
    if (loc->_children) {
	if (child == NULL) {
	    if (loc->_children[0])
		return gweather_location_ref (loc->_children[0]);
	    else
		return NULL;
	}

	for (i = 0; loc->_children[i]; i++) {
	    if (loc->_children[i] == child) {
		if (loc->_children[i + 1])
		    return gweather_location_ref (loc->_children[i + 1]);
		else
		    return NULL;
	    }
	}

	goto invalid_child;
    }

    /* If we have a magic nearest child, iterate over that. */
    if (!g_getenv ("LIBGWEATHER_LOCATIONS_NO_NEAREST") &&
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

/**
 * gweather_location_get_children:
 * @loc: a #GWeatherLocation
 *
 * Gets an array of @loc's children; this is owned by @loc and will
 * not remain valid if @loc is freed.
 *
 * Return value: (transfer none) (array zero-terminated=1): @loc's
 * children. (May be empty, but will not be %NULL.)
 *
 * Deprecated: 40. Use gweather_location_next_child() instead to avoid high
 * memory consumption
 **/
GWeatherLocation **
gweather_location_get_children (GWeatherLocation *loc)
{
    static GWeatherLocation *no_children = NULL;
    DbArrayofuint16Ref children_ref;
    gsize length;
    gsize i;

    g_return_val_if_fail (loc != NULL, &no_children);

    if (loc->_children)
	return loc->_children;

    if (!loc->db)
	return &no_children;

    /* Fill in the magic nearest child if we need to and should. */
    if (!g_getenv ("LIBGWEATHER_LOCATIONS_NO_NEAREST") &&
        IDX_VALID (db_location_get_nearest (loc->ref))) {
	loc->_children = g_new0 (GWeatherLocation*, 2);
	loc->_children[0] = location_ref_for_idx (loc->db, db_location_get_nearest (loc->ref), loc);

	return loc->_children;
    }

    /* Get the actual children. */
    children_ref = db_location_get_children (loc->ref);
    length = db_arrayofuint16_get_length (children_ref);
    if (length == 0)
	return &no_children;

    loc->_children = g_new0 (GWeatherLocation*, length + 1);
    for (i = 0; i < length; i++)
	loc->_children[i] = location_ref_for_idx (loc->db,
						 db_arrayofuint16_get_at (children_ref, i),
						 NULL);

    return loc->_children;
}

static void
foreach_city (GWeatherLocation  *loc,
              GFunc              callback,
              gpointer           user_data,
	      const char        *country_code,
              GWeatherFilterFunc func,
              gpointer           user_data_func)
{
     if (country_code) {
         const char *loc_country_code = gweather_location_get_country(loc);
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
	g_autoptr(GWeatherLocation) child = NULL;

	while ((child = gweather_location_next_child (loc, child)))
	    foreach_city (child, callback, user_data, country_code, func, user_data_func);
    }
}

struct FindNearestCityData {
    double latitude;
    double longitude;
    GWeatherLocation *location;
    double distance;
};

struct ArgData {
    double latitude;
    double longitude;
    GWeatherLocation *location;
    GTask *task;
};

typedef struct ArgData ArgData;

static double
location_distance (double lat1, double long1,
		   double lat2, double long2)
{
    /* average radius of the earth in km */
    static const double radius = 6372.795;

    if (lat1 == lat2 && long1 == long2)
        return 0.0;

    return acos (cos (lat1) * cos (lat2) * cos (long1 - long2) +
		 sin (lat1) * sin (lat2)) * radius;
}

static void
find_nearest_city (GWeatherLocation *location,
		   gpointer          user_data) {
    struct FindNearestCityData *data = user_data;

    double distance = location_distance (location->latitude, location->longitude,
					 data->latitude, data->longitude);

    if (data->location == NULL || data->distance > distance) {
	g_clear_pointer (&data->location, gweather_location_unref);
	data->location = gweather_location_ref (location);
	data->distance = distance;
    }
}

/**
 * gweather_location_find_nearest_city:
 * @loc: (allow-none): The parent location, which will be searched recursively
 * @lat: Latitude, in degrees
 * @lon: Longitude, in degrees
 *
 * Finds the nearest city to the passed latitude and
 * longitude, among the descendants of @loc.
 *
 * @loc must be at most a %GWEATHER_LOCATION_ADM1 location.
 * This restriction may be lifted in a future version.
 *
 * Note that this function does not check if (@lat, @lon) fall inside
 * @loc, or are in the same region and timezone as the return value.
 *
 * Returns: (transfer full): the city closest to (@lat, @lon), in the
 *          region or administrative district of @loc.
 *
 * Since: 3.12
 */
GWeatherLocation *
gweather_location_find_nearest_city (GWeatherLocation *loc,
				     double            lat,
				     double            lon)
{
    g_autoptr(GWeatherLocation) world = NULL;
    /* The data set really isn't too big. Don't concern ourselves
     * with a proper nearest neighbors search. Instead, just do
     * an O(n) search. */
    struct FindNearestCityData data;

    g_return_val_if_fail (loc == NULL || loc->level < GWEATHER_LOCATION_CITY, NULL);

    if (loc == NULL)
	loc = world = gweather_location_get_world ();

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
 * @loc: (allow-none): The parent location, which will be searched recursively
 * @lat: Latitude, in degrees
 * @lon: Longitude, in degrees
 * @func: (scope notified) (allow-none): returns true to continue check for
 *                                       the location and false to filter the location out
 * @user_data: for customization
 * @destroy: to destroy user_data
 *
 * Finds the nearest city to the passed latitude and
 * longitude, among the descendants of @loc.
 *
 * Supports the use of own filter function to filter out locations.
 * Geocoding should be done on the application side if needed.
 *
 * @loc must be at most a %GWEATHER_LOCATION_ADM1 location.
 * This restriction may be lifted in a future version.
 *
 * Returns: (transfer full): the city closest to (@lat, @lon), in the
 *          region or administrative district of @loc with validation of filter function.
 *
 * Since: 3.12
 */
GWeatherLocation *
gweather_location_find_nearest_city_full (GWeatherLocation  *loc,
					  double             lat,
					  double             lon,
					  GWeatherFilterFunc func,
					  gpointer           user_data,
					  GDestroyNotify     destroy)
{
    g_autoptr(GWeatherLocation) world = NULL;
    /* The data set really isn't too big. Don't concern ourselves
     * with a proper nearest neighbors search. Instead, just do
     * an O(n) search. */
    struct FindNearestCityData data;

    g_return_val_if_fail (loc == NULL || loc->level < GWEATHER_LOCATION_CITY ||
			  loc->level == GWEATHER_LOCATION_NAMED_TIMEZONE, NULL);

    if (loc == NULL)
        loc = world = gweather_location_get_world ();

    lat = lat * M_PI / 180.0;
    lon = lon * M_PI / 180.0;

    data.latitude = lat;
    data.longitude = lon;
    data.location = NULL;
    data.distance = 0.0;

    foreach_city (loc, (GFunc) find_nearest_city, &data, NULL, func, user_data);

    destroy (user_data);

    return gweather_location_ref (data.location);
}

static void
_got_place (GObject      *source_object,
	    GAsyncResult *result,
	    gpointer      user_data)
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

    g_slice_free (ArgData, info);

    if (data.location == NULL) {
	g_task_return_pointer (task, NULL, NULL);
    } else {
        GWeatherLocation *location;
        location = _gweather_location_new_detached(data.location, geocode_place_get_town (place), TRUE, data.latitude, data.longitude);

	g_task_return_pointer (task, location, (GDestroyNotify)gweather_location_unref);
    }

    g_object_unref (task);
}

/**
 * gweather_location_detect_nearest_city:
 * @loc: (allow-none): The parent location, which will be searched recursively
 * @lat: Latitude, in degrees
 * @lon: Longitude, in degrees
 * @cancellable: optional, NULL to ignore
 * @callback: callback function for GAsyncReadyCallback argument for GAsyncResult
 * @user_data: user data passed to @callback
 *
 * Initializes geocode reversing to find place for (@lat, @lon) coordinates. Calls the callback
 * function passed by user when the result is ready.
 *
 * @loc must be at most a %GWEATHER_LOCATION_ADM1 location.
 * This restriction may be lifted in a future version.
 *
 * Since: 3.12
 */
void
gweather_location_detect_nearest_city (GWeatherLocation    *loc,
					double              lat,
					double              lon,
					GCancellable       *cancellable,
					GAsyncReadyCallback callback,
					gpointer            user_data)
{
    g_autoptr(GWeatherLocation) world = NULL;
    ArgData *data;
    GeocodeLocation *location;
    GeocodeReverse *reverse;
    GTask *task;

    g_return_if_fail (loc == NULL || loc->level < GWEATHER_LOCATION_CITY ||
		      loc->level == GWEATHER_LOCATION_NAMED_TIMEZONE);

    if (loc == NULL)
        loc = world = gweather_location_get_world ();

    location = geocode_location_new (lat, lon, GEOCODE_LOCATION_ACCURACY_CITY);
    reverse = geocode_reverse_new_for_location (location);

    task = g_task_new (NULL, cancellable, callback, user_data);

    data = g_slice_new0 (ArgData);
    data->latitude = lat;
    data->longitude = lon;
    data->location = loc;
    data->task = task;

    geocode_reverse_resolve_async (reverse, cancellable, _got_place, data);
}

/**
 * gweather_location_detect_nearest_location_finish:
 * @result: 
 * @error: Stores error if any occurs in retrieving the result
 *
 * Fetches the location from @result.
 *
 * Returns: (transfer full): Customized GWeatherLocation
 *
 * Since: 3.12
 */

GWeatherLocation *
gweather_location_detect_nearest_city_finish (GAsyncResult *result, GError **error)
{
    GTask *task;

    g_return_val_if_fail (g_task_is_valid (result,
					   NULL),
			  NULL);

    task = G_TASK (result);
    return g_task_propagate_pointer (task, error);
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
    g_return_val_if_fail (loc != NULL, FALSE);
    return loc->latlon_valid;
}

/**
 * gweather_location_get_coords:
 * @loc: a #GWeatherLocation
 * @latitude: (out): on return will contain @loc's latitude
 * @longitude: (out): on return will contain @loc's longitude
 *
 * Gets @loc's coordinates; you must check
 * gweather_location_has_coords() before calling this.
 **/
void
gweather_location_get_coords (GWeatherLocation *loc,
			      double *latitude, double *longitude)
{
    //g_return_if_fail (loc->latlon_valid);
    g_return_if_fail (loc != NULL);
    g_return_if_fail (latitude != NULL);
    g_return_if_fail (longitude != NULL);

    *latitude = loc->latitude / M_PI * 180.0;
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
gweather_location_get_distance (GWeatherLocation *loc, GWeatherLocation *loc2)
{
    g_return_val_if_fail (loc != NULL, G_MAXDOUBLE);
    g_return_val_if_fail (loc2 != NULL, G_MAXDOUBLE);

    g_return_val_if_fail (loc->latlon_valid, G_MAXDOUBLE);
    g_return_val_if_fail (loc2->latlon_valid, G_MAXDOUBLE);

    return location_distance (loc->latitude, loc->longitude,
			      loc2->latitude, loc2->longitude);
}

/**
 * gweather_location_get_country:
 * @loc: a #GWeatherLocation
 *
 * Gets the ISO 3166 country code of @loc (or %NULL if @loc is a
 * region- or world-level location)
 *
 * Return value: (allow-none): @loc's country code (or %NULL if @loc
 * is a region- or world-level location)
 **/
const char *
gweather_location_get_country (GWeatherLocation *loc)
{
    g_autoptr(GWeatherLocation) s = NULL;
    g_return_val_if_fail (loc != NULL, NULL);

    ITER_UP(loc, s) {
	if (s->_country_code)
	    return s->_country_code;

	if (s->db && IDX_VALID(s->db_idx)) {
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
 * The timezone is owned either by @loc or by one of its parents.
 * FIXME.
 *
 * Return value: (transfer none) (allow-none): @loc's timezone, or
 * %NULL
 **/
GWeatherTimezone *
gweather_location_get_timezone (GWeatherLocation *loc)
{
    g_autoptr(GWeatherLocation) s = NULL;

    g_return_val_if_fail (loc != NULL, NULL);

    if (loc->_timezone)
	return loc->_timezone;

    ITER_UP(loc, s) {
	if (!IDX_VALID (s->tz_hint_idx))
	    continue;

	loc->_timezone = _gweather_timezone_ref_for_idx (s->db, s->tz_hint_idx);
	return loc->_timezone;
    }
    return NULL;
}

/**
 * gweather_location_get_timezone_str:
 * @loc: a #GWeatherLocation
 *
 * Gets the timezone associated with @loc, if known, as a string.
 *
 * The timezone string is owned either by @loc or by one of its
 * parents, do not free it.
 *
 * Return value: (transfer none) (allow-none): @loc's timezone as
 * a string, or %NULL
 **/
const char *
gweather_location_get_timezone_str (GWeatherLocation *loc)
{
    g_autoptr(GWeatherLocation) s = NULL;

    g_return_val_if_fail (loc != NULL, NULL);

    ITER_UP(loc, s) {
	if (s->_timezone)
	    return gweather_timezone_get_tzid (s->_timezone);

	if (s->db && IDX_VALID(s->tz_hint_idx)) {
	    return db_world_timezones_entry_get_key (db_world_timezones_get_at (s->db->timezones_ref, s->tz_hint_idx));
	}
    }

    return NULL;
}

static void
add_timezones (GWeatherLocation *loc, GPtrArray *zones)
{
    gsize i;

    /* NOTE: Only DB backed locations can have timezones */
    if (loc->db && IDX_VALID (loc->db_idx)) {
	DbArrayofuint16Ref ref;
	gsize len;

	ref = db_location_get_timezones (loc->ref);
	len = db_arrayofuint16_get_length (ref);
	for (i = 0; i < len; i++)
	    g_ptr_array_add (zones,
			     _gweather_timezone_ref_for_idx (loc->db,
							      db_arrayofuint16_get_at (ref, i)));
    }
    if (loc->level < GWEATHER_LOCATION_COUNTRY) {
	g_autoptr(GWeatherLocation) child = NULL;

	while ((child = gweather_location_next_child (loc, child)))
	    add_timezones (child, zones);
    }
}

/**
 * gweather_location_get_timezones:
 * @loc: a #GWeatherLocation
 *
 * Gets an array of all timezones associated with any location under
 * @loc. You can use gweather_location_free_timezones() to free this
 * array.
 *
 * Return value: (transfer full) (array zero-terminated=1): an array
 * of timezones. May be empty but will not be %NULL.
 **/
GWeatherTimezone **
gweather_location_get_timezones (GWeatherLocation *loc)
{
    GPtrArray *zones;

    g_return_val_if_fail (loc != NULL, NULL);

    zones = g_ptr_array_new ();
    add_timezones (loc, zones);
    g_ptr_array_add (zones, NULL);
    return (GWeatherTimezone **)g_ptr_array_free (zones, FALSE);
}

/**
 * gweather_location_free_timezones:
 * @loc: a #GWeatherLocation
 * @zones: an array returned from gweather_location_get_timezones()
 *
 * Frees the array of timezones returned by
 * gweather_location_get_timezones().
 **/
void
gweather_location_free_timezones (GWeatherLocation  *loc,
				  GWeatherTimezone **zones)
{
    int i;

    g_return_if_fail (loc != NULL);
    g_return_if_fail (zones != NULL);

    for (i = 0; zones[i]; i++)
	gweather_timezone_unref (zones[i]);
    g_free (zones);
}

/**
 * gweather_location_get_code:
 * @loc: a #GWeatherLocation
 *
 * Gets the METAR station code associated with a
 * %GWEATHER_LOCATION_WEATHER_STATION location.
 *
 * Return value: (allow-none): @loc's METAR station code, or %NULL
 **/
const char *
gweather_location_get_code (GWeatherLocation *loc)
{
    g_return_val_if_fail (loc != NULL, NULL);
    if (loc->_station_code)
	return loc->_station_code;

    if (loc->db && IDX_VALID(loc->db_idx)) {
	return EMPTY_TO_NULL (db_location_get_metar_code (loc->ref));
    }

    return NULL;
}

/**
 * gweather_location_get_city_name:
 * @loc: a #GWeatherLocation
 *
 * For a %GWEATHER_LOCATION_CITY or %GWEATHER_LOCATION_DETACHED location,
 * this is equivalent to gweather_location_get_name().
 * For a %GWEATHER_LOCATION_WEATHER_STATION location, it is equivalent to
 * calling gweather_location_get_name() on the location's parent. For
 * other locations it will return %NULL.
 *
 * Return value: (allow-none): @loc's city name, or %NULL
 **/
char *
gweather_location_get_city_name (GWeatherLocation *loc)
{
    g_return_val_if_fail (loc != NULL, NULL);

    if (loc->level == GWEATHER_LOCATION_CITY ||
        loc->level == GWEATHER_LOCATION_DETACHED) {
        return g_strdup (gweather_location_get_name (loc));
    } else {
        g_autoptr(GWeatherLocation) parent = NULL;
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
 * Gets the country name of loc.
 * For a %GWEATHER_LOCATION_COUNTRY location, this is equivalent to
 * gweather_location_get_name().
 * For a %GWEATHER_LOCATION_REGION and GWEATHER_LOCATION_WORLD location it
 * will return %NULL.
 * For other locations it will find the parent %GWEATHER_LOCATION_COUNTRY
 * and return its name.
 *
 * Return value: (allow-none): @loc's country name, or %NULL
 **/
char *
gweather_location_get_country_name (GWeatherLocation *loc)
{
    g_autoptr(GWeatherLocation) country = NULL;

    g_return_val_if_fail (loc != NULL, NULL);

    ITER_UP(loc, country) {
	if (country->level == GWEATHER_LOCATION_COUNTRY)
	    return g_strdup (gweather_location_get_name (country));
    }

    return NULL;
}

void
_gweather_location_update_weather_location (GWeatherLocation *gloc,
					    WeatherLocation  *loc)
{
    char *code = NULL, *zone = NULL, *radar = NULL, *tz_hint = NULL;
    gboolean latlon_valid = FALSE;
    gdouble lat = DBL_MAX, lon = DBL_MAX;
    g_autoptr(GWeatherLocation) start = NULL;
    g_autoptr(GWeatherLocation) l = NULL;

    if (gloc->level == GWEATHER_LOCATION_CITY) {
	GWeatherLocation *first_child;
	first_child = gweather_location_next_child (gloc, NULL);

	if (first_child)
		start = first_child;
    }
    if (!start)
	start = gweather_location_ref (gloc);

    ITER_UP(start, l) {
	if (!code)
	    code = g_strdup (gweather_location_get_code (l));
	if (!zone && l->db && IDX_VALID(l->db_idx))
	    zone = g_strdup (EMPTY_TO_NULL (db_location_get_forecast_zone (l->ref)));
	if (!radar && l->db && IDX_VALID(l->db_idx))
	    radar = g_strdup (EMPTY_TO_NULL (db_location_get_radar (l->ref)));
	if (!tz_hint && l->db && IDX_VALID(l->tz_hint_idx))
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
 * Note that multiple instances of the same weather station can exist
 * in the database, and this function will return any of them, so this
 * not usually what you want.
 *
 * See gweather_location_deserialize() to recover a stored #GWeatherLocation.
 *
 * Prior to version 40 no reference was returned.
 *
 * Returns: (transfer full): a weather station level #GWeatherLocation for @station_code,
 *          or %NULL if none exists in the database.
 */
GWeatherLocation *
gweather_location_find_by_station_code (GWeatherLocation *world,
					 const gchar      *station_code)
{
    DbWorldLocByMetarRef loc_by_metar;
    guint16 idx;

    if (!world->db)
	return NULL;

    loc_by_metar = db_world_get_loc_by_metar (world->db->world);
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
 * Prior to version 40 no reference was returned.
 *
 * Returns: (transfer full): a country level #GWeatherLocation, or %NULL.
 */
GWeatherLocation *
gweather_location_find_by_country_code (GWeatherLocation *world,
                                        const gchar      *country_code)
{
    DbWorldLocByCountryRef loc_by_country;
    guint16 idx;

    if (!world->db)
	return NULL;

    loc_by_country = db_world_get_loc_by_country (world->db->world);
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
    g_autoptr(GWeatherLocation) p1 = NULL, p2 = NULL;
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

    return ABS(one->latitude - two->latitude) < EPSILON &&
	ABS(one->longitude - two->longitude) < EPSILON;
}

/* ------------------- serialization ------------------------------- */

#define FORMAT 2

static GVariant *
gweather_location_format_two_serialize (GWeatherLocation *location)
{
    g_autoptr(GWeatherLocation) real_loc = NULL;
    g_autoptr(GWeatherLocation) parent = NULL;
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
	real_loc = gweather_location_ref (location);

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
			  &latlon_builder, &parent_latlon_builder);
}

GWeatherLocation *
_gweather_location_new_detached (GWeatherLocation *nearest_station,
				 const char       *name,
				 gboolean          latlon_valid,
				 gdouble           latitude,
				 gdouble           longitude)
{
    GWeatherLocation *self;
    char *normalized;

    self = location_new (GWEATHER_LOCATION_DETACHED);
    if (name != NULL) {
	self->_english_name = g_strdup (name);
	self->_local_name = g_strdup (name);

	normalized = g_utf8_normalize (name, -1, G_NORMALIZE_ALL);
	self->_english_sort_name = g_utf8_casefold (normalized, -1);
	self->_local_sort_name = g_strdup (self->_english_sort_name);
	g_free (normalized);
    } else if (nearest_station) {
	self->_english_name = g_strdup (gweather_location_get_english_name (nearest_station));
	self->_local_name = g_strdup (gweather_location_get_name (nearest_station));
	self->_english_sort_name = g_strdup (gweather_location_get_english_sort_name (nearest_station));
	self->_local_sort_name = g_strdup (gweather_location_get_sort_name (nearest_station));
    }

    self->_parent = nearest_station; /* a reference is passed */
    self->_children = NULL;

    if (nearest_station)
	self->_station_code = g_strdup (gweather_location_get_code (nearest_station));

    g_assert (nearest_station || latlon_valid);

    if (latlon_valid) {
	self->latlon_valid = TRUE;
	self->latitude = latitude;
	self->longitude = longitude;
    } else {
	self->latlon_valid = nearest_station->latlon_valid;
	self->latitude = nearest_station->latitude;
	self->longitude = nearest_station->longitude;
    }

    return self;
}

static GWeatherLocation *
gweather_location_common_deserialize (GWeatherLocation *world,
				      const char       *name,
				      const char       *station_code,
				      gboolean          is_city,
				      gboolean          latlon_valid,
				      gdouble           latitude,
				      gdouble           longitude,
				      gboolean          parent_latlon_valid,
				      gdouble           parent_latitude,
				      gdouble           parent_longitude)
{
    g_autoptr(GWeatherLocation) by_station_code = NULL;
    DbWorldLocByMetarRef loc_by_metar;
    GWeatherLocation *found;
    gsize i;

    /* Since weather stations are no longer attached to cities, first try to
       find what claims to be a city by name and coordinates */
    if (is_city && latitude && longitude) {
        found = gweather_location_find_nearest_city (world,
                                                     latitude / M_PI * 180.0,
                                                     longitude / M_PI * 180.0);

        if (found && (g_strcmp0 (name, gweather_location_get_english_name (found)) == 0 ||
                      g_strcmp0 (name, gweather_location_get_name (found)) == 0))
	    return g_steal_pointer (&found);
	g_clear_pointer (&found, gweather_location_unref);
    }

    if (station_code[0] == '\0')
        return _gweather_location_new_detached (NULL, name, latlon_valid, latitude, longitude);

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
	       ? _gweather_location_new_detached (g_steal_pointer (&by_station_code),
	                                          name, FALSE, 0, 0)
	       : NULL;

    found = NULL;
    loc_by_metar = db_world_get_loc_by_metar (world->db->world);

    for (i = 0; i < db_world_loc_by_metar_get_length (loc_by_metar); i++) {
	g_autoptr(GWeatherLocation) ws = NULL, city = NULL;
	/* Skip if the metar code does not match */
	if (!g_str_equal (station_code, db_world_loc_by_metar_entry_get_key (db_world_loc_by_metar_get_at (loc_by_metar, i))))
	    continue;

	/* Be lazy and allocate the location */
	ws = location_ref_for_idx (world->db, db_world_loc_by_metar_entry_get_value (db_world_loc_by_metar_get_at (loc_by_metar, i)), NULL);

	if (!ws->latlon_valid ||
	    ABS(ws->latitude - latitude) >= EPSILON ||
	    ABS(ws->longitude - longitude) >= EPSILON)
	    /* Not what we're looking for... */
	    continue;

	city = gweather_location_get_parent (ws);

	/* If we can't check for the latitude and longitude
	   of the parent, we just assume we found what we needed
	*/
	if ((!parent_latlon_valid || !city || !city->latlon_valid) ||
	    (ABS(parent_latitude - city->latitude) < EPSILON &&
	     ABS(parent_longitude - city->longitude) < EPSILON)) {

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
		found = gweather_location_ref (found);
	    else
		found = _gweather_location_new_detached (gweather_location_ref (ws), name, TRUE, latitude, longitude);

	    return found;
	}
    }

    /* No weather station matches the serialized data, let's pick
       one at random from the station code list */
    if (by_station_code)
       return _gweather_location_new_detached (g_steal_pointer (&by_station_code),
                                               name, TRUE, latitude, longitude);
    else
       return NULL;
}

static GWeatherLocation *
gweather_location_format_one_deserialize (GWeatherLocation *world,
					  GVariant         *variant)
{
    const char *name;
    const char *station_code;
    gboolean is_city, latlon_valid, parent_latlon_valid;
    gdouble latitude, longitude, parent_latitude, parent_longitude;

    /* This one instead is a critical, because format is specified in
       the containing variant */
    g_return_val_if_fail (g_variant_is_of_type (variant,
						G_VARIANT_TYPE ("(ssbm(dd)m(dd))")), NULL);

    g_variant_get (variant, "(&s&sbm(dd)m(dd))", &name, &station_code, &is_city,
		   &latlon_valid, &latitude, &longitude,
		   &parent_latlon_valid, &parent_latitude, &parent_longitude);

    return gweather_location_common_deserialize(world, name, station_code, is_city,
						latlon_valid, latitude, longitude,
						parent_latlon_valid, parent_latitude, parent_longitude);
}

static GWeatherLocation *
gweather_location_format_two_deserialize (GWeatherLocation *world,
					  GVariant         *variant)
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
						G_VARIANT_TYPE ("(ssba(dd)a(dd))")), NULL);

    g_variant_get (variant, "(&s&sb@a(dd)@a(dd))", &name, &station_code, &is_city,
		   &latlon_variant, &parent_latlon_variant);

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

    return gweather_location_common_deserialize(world, name, station_code, is_city,
						latlon_valid, latitude, longitude,
						parent_latlon_valid, parent_latitude, parent_longitude);
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
    g_return_val_if_fail (loc != NULL, NULL);
    g_return_val_if_fail (loc->level >= GWEATHER_LOCATION_CITY, NULL);

    return g_variant_new ("(uv)", FORMAT,
			  gweather_location_format_two_serialize (loc));
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
			       GVariant         *serialized)
{
    GVariant *v;
    GWeatherLocation *loc;
    int format;

    g_return_val_if_fail (world != NULL, NULL);
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
 * gweather_location_new_detached:
 * @name: the user visible location name
 * @icao: (nullable): the ICAO code of the location
 * @latitude: the latitude of the location
 * @longitude: the longitude of the location
 *
 * Construct a new location from the given data, supplementing
 * any missing information from the static database.
 */
GWeatherLocation *
gweather_location_new_detached (const char *name,
				const char *icao,
				gdouble     latitude,
				gdouble     longitude)
{
    g_autoptr(GWeatherLocation) world = NULL;
    g_autoptr(GWeatherLocation) city = NULL;

    g_return_val_if_fail (name != NULL, NULL);

    if (*name == 0)
	name = NULL;

    world = gweather_location_get_world ();

    if (icao != NULL) {
	return gweather_location_common_deserialize (world, name,
						     icao, FALSE,
						     TRUE, latitude, longitude,
						     FALSE, 0, 0);
    } else {
	city = gweather_location_find_nearest_city (world, latitude, longitude);

	latitude = DEGREES_TO_RADIANS (latitude);
	longitude = DEGREES_TO_RADIANS (longitude);
	return _gweather_location_new_detached (g_steal_pointer (&city), name,
						TRUE, latitude, longitude);
    }
}
