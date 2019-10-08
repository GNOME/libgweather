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
 * <http://www.gnu.org/licenses/>.
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

#include "gweather-location.h"
#include "gweather-timezone.h"
#include "gweather-parser.h"
#include "gweather-private.h"

/* This is the precision of coordinates in the database */
#define EPSILON 0.000001

/* This is the maximum distance for which we will attach an
 * airport to a city, see also test_distance() */
#define AIRPORT_MAX_DISTANCE 100.0

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

static int
sort_locations_by_name (gconstpointer a, gconstpointer b)
{
    GWeatherLocation *loc_a = *(GWeatherLocation **)a;
    GWeatherLocation *loc_b = *(GWeatherLocation **)b;

    return g_utf8_collate (loc_a->local_sort_name, loc_b->local_sort_name);
}
 
static int
sort_locations_by_distance (gconstpointer a, gconstpointer b, gpointer user_data)
{
    GWeatherLocation *loc_a = *(GWeatherLocation **)a;
    GWeatherLocation *loc_b = *(GWeatherLocation **)b;
    GWeatherLocation *city = (GWeatherLocation *)user_data;
    double dist_a, dist_b;

    dist_a = gweather_location_get_distance (loc_a, city);
    dist_b = gweather_location_get_distance (loc_b, city);
    if (dist_a < dist_b)
	return -1;
    else if (dist_a > dist_b)
	return 1;
    else
	return 0;
}

static gboolean
parse_coordinates (const char *coordinates,
		   double *latitude, double *longitude)
{
    char *p;

    *latitude = g_ascii_strtod (coordinates, &p) * M_PI / 180.0;
    if (p == (char *)coordinates)
	return FALSE;
    if (*p++ != ' ')
	return FALSE;
    *longitude = g_ascii_strtod (p, &p) * M_PI / 180.0;
    return !*p;
}

static GWeatherLocation *
location_new (GWeatherLocationLevel level)
{
    GWeatherLocation *loc;

    loc = g_slice_new0 (GWeatherLocation);
    loc->latitude = loc->longitude = DBL_MAX;
    loc->level = level;
    loc->ref_count = 1;

    return loc;
}

static void add_timezones (GWeatherLocation *loc, GPtrArray *zones);

static void
add_nearest_weather_station (GWeatherLocation *location)
{
    GWeatherLocation **siblings, *station;
    GWeatherLocation *closest = NULL;
    double min_distance = G_MAXDOUBLE;
    GPtrArray *zones;
    guint i;

    g_assert (location->parent);
    g_assert (gweather_location_get_level (location) == GWEATHER_LOCATION_CITY);

    if (location->children != NULL)
        return;

    siblings = location->parent->children;
    for (i = 0; siblings[i] != NULL; i++) {
        double distance;

        if (siblings[i] == location)
            continue;

        if (siblings[i]->level != GWEATHER_LOCATION_WEATHER_STATION)
            continue;

        /* Skip siblings without valid coordinates */
        if (!siblings[i]->latlon_valid)
            continue;

        distance = gweather_location_get_distance (location, siblings[i]);
        if (distance < min_distance) {
            closest = siblings[i];
            min_distance = distance;
        }
    }

    /* This should not happen */
    if (!closest) {
        g_critical ("Location '%s' has no valid airports attached", location->english_name);
        return;
    }

    /* This could however */
    if (min_distance > AIRPORT_MAX_DISTANCE) {
        g_debug ("Not adding airport '%s' as it's too far from '%s' (%.1lf km)\n",
                 gweather_location_get_name (closest),
                 gweather_location_get_name (location),
                 min_distance);
        return;
    }

    location->children = g_new0 (GWeatherLocation *, 2);
    location->children[0] = g_slice_new0 (GWeatherLocation);
    station = location->children[0];
    station->english_name = g_strdup (closest->english_name);
    station->local_name = g_strdup (closest->local_name);
    station->msgctxt = g_strdup (closest->msgctxt);
    station->local_sort_name = g_strdup (closest->local_sort_name);
    station->english_sort_name = g_strdup (closest->english_sort_name);
    station->parent = location;
    station->level = GWEATHER_LOCATION_WEATHER_STATION;
    station->country_code = g_strdup (closest->country_code);
    station->tz_hint = g_strdup (closest->tz_hint);
    station->station_code = g_strdup (closest->station_code);
    station->forecast_zone = g_strdup (closest->forecast_zone);
    station->radar = g_strdup (closest->radar);
    station->latitude = closest->latitude;
    station->longitude = closest->longitude;
    station->latlon_valid = closest->latlon_valid;

    zones = g_ptr_array_new ();
    add_timezones (station, zones);
    g_ptr_array_add (zones, NULL);
    station->zones = (GWeatherTimezone **)g_ptr_array_free (zones, FALSE);

    station->ref_count = 1;
}

static void
add_nearest_weather_stations (GWeatherLocation *location)
{
    GWeatherLocation **children;
    guint i;

    /* For each city without a <location>, add the nearest airport in the
     * same country or state to it */
    children = gweather_location_get_children (location);
    for (i = 0; children[i] != NULL; i++) {
        if (gweather_location_get_level (children[i]) == GWEATHER_LOCATION_CITY)
            add_nearest_weather_station (children[i]);
        else
            add_nearest_weather_stations (children[i]);
    }
}

static GWeatherLocation *
location_new_from_xml (GWeatherParser *parser, GWeatherLocationLevel level,
		       GWeatherLocation *parent)
{
    GWeatherLocation *loc, *child;
    GPtrArray *children = NULL;
    const char *tagname;
    char *value, *normalized;
    int tagtype;
    unsigned int i;

    loc = location_new (level);
    loc->parent = parent;
    if (level == GWEATHER_LOCATION_WORLD) {
	loc->metar_code_cache = g_hash_table_ref (parser->metar_code_cache);
	loc->country_code_cache = g_hash_table_ref (parser->country_code_cache);
	loc->timezone_cache = g_hash_table_ref (parser->timezone_cache);
    }
    children = g_ptr_array_new ();

    if (xmlTextReaderRead (parser->xml) != 1)
	goto error_out;
    while ((tagtype = xmlTextReaderNodeType (parser->xml)) !=
	   XML_READER_TYPE_END_ELEMENT) {
	if (tagtype != XML_READER_TYPE_ELEMENT) {
	    if (xmlTextReaderRead (parser->xml) != 1)
		goto error_out;
	    continue;
	}

	tagname = (const char *) xmlTextReaderConstName (parser->xml);
	if ((!strcmp (tagname, "name") || !strcmp (tagname, "_name")) && !loc->english_name) {
            loc->msgctxt = _gweather_parser_get_msgctxt_value (parser);
	    value = _gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;

	    loc->english_name = g_strdup (value);

	    if (loc->msgctxt) {
		loc->local_name = g_strdup (g_dpgettext2 ("libgweather-locations",
							  (char*) loc->msgctxt, value));
	    } else {
		loc->local_name = g_strdup (g_dgettext ("libgweather-locations", value));
	    }

	    normalized = g_utf8_normalize (loc->local_name, -1, G_NORMALIZE_ALL);
	    loc->local_sort_name = g_utf8_casefold (normalized, -1);
	    g_free (normalized);

	    normalized = g_utf8_normalize (loc->english_name, -1, G_NORMALIZE_ALL);
	    loc->english_sort_name = g_utf8_casefold (normalized, -1);
	    g_free (normalized);
	    xmlFree (value);
	} else if (!strcmp (tagname, "iso-code") && !loc->country_code) {
	    value = _gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;
	    loc->country_code = g_strdup (value);
	    xmlFree (value);
	} else if (!strcmp (tagname, "tz-hint") && !loc->tz_hint) {
	    value = _gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;
	    loc->tz_hint = g_strdup (value);
	    xmlFree (value);
	} else if (!strcmp (tagname, "code") && !loc->station_code) {
	    value = _gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;
	    loc->station_code = g_strdup (value);
	    xmlFree (value);
	} else if (!strcmp (tagname, "coordinates") && !loc->latlon_valid) {
	    value = _gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;
	    if (parse_coordinates (value, &loc->latitude, &loc->longitude))
		loc->latlon_valid = TRUE;
	    else
	        g_warning ("Coordinates could not be parsed: '%s'", value);
	    xmlFree (value);
	} else if (!strcmp (tagname, "zone") && !loc->forecast_zone) {
	    value = _gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;
	    loc->forecast_zone = g_strdup (value);
	    xmlFree (value);
	} else if (!strcmp (tagname, "radar") && !loc->radar) {
	    value = _gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;
	    loc->radar = g_strdup (value);
	    xmlFree (value);

	} else if (!strcmp (tagname, "region")) {
	    child = location_new_from_xml (parser, GWEATHER_LOCATION_REGION, loc);
	    if (!child)
		goto error_out;
	    g_ptr_array_add (children, child);
	} else if (!strcmp (tagname, "country")) {
	    child = location_new_from_xml (parser, GWEATHER_LOCATION_COUNTRY, loc);
	    if (!child)
		goto error_out;
	    g_ptr_array_add (children, child);
	} else if (!strcmp (tagname, "state")) {
	    child = location_new_from_xml (parser, GWEATHER_LOCATION_ADM1, loc);
	    if (!child)
		goto error_out;
	    g_ptr_array_add (children, child);
	} else if (!strcmp (tagname, "city")) {
	    child = location_new_from_xml (parser, GWEATHER_LOCATION_CITY, loc);
	    if (!child)
		goto error_out;
	    g_ptr_array_add (children, child);
	} else if (!strcmp (tagname, "location")) {
	    child = location_new_from_xml (parser, GWEATHER_LOCATION_WEATHER_STATION, loc);
	    if (!child)
		goto error_out;
	    g_ptr_array_add (children, child);

	} else if (!strcmp (tagname, "named-timezone")) {
	    child = location_new_from_xml (parser, GWEATHER_LOCATION_NAMED_TIMEZONE, loc);
	    if (!child)
		goto error_out;
	    g_ptr_array_add (children, child);

	} else if (!strcmp (tagname, "timezones")) {
	    loc->zones = _gweather_timezones_parse_xml (parser);
	    if (!loc->zones)
		goto error_out;

	} else {
	    if (xmlTextReaderNext (parser->xml) != 1)
		goto error_out;
	}
    }
    if (xmlTextReaderRead (parser->xml) != 1 && parent)
	goto error_out;

    if (level == GWEATHER_LOCATION_WEATHER_STATION ||
	level == GWEATHER_LOCATION_NAMED_TIMEZONE) {
	/* Cache weather stations by METAR code */
	GList *a, *b;

	a = g_hash_table_lookup (parser->metar_code_cache, loc->station_code);
	b = g_list_append (a, gweather_location_ref (loc));
	if (b != a)
	    g_hash_table_replace (parser->metar_code_cache, loc->station_code, b);
    }

    if (level == GWEATHER_LOCATION_COUNTRY) {
	if (loc->country_code) {
	    GWeatherLocation *existing;

	    existing = g_hash_table_lookup (parser->country_code_cache, loc->country_code);
	    if (existing)
	        g_warning ("A country with country code '%s' is already in the database",
	                   loc->country_code);
	    g_hash_table_replace (parser->country_code_cache, loc->country_code,
				  gweather_location_ref (loc));
	}
    }

    if (children->len) {
	if (level == GWEATHER_LOCATION_CITY)
	    g_ptr_array_sort_with_data (children, sort_locations_by_distance, loc);
	else
	    g_ptr_array_sort (children, sort_locations_by_name);

	g_ptr_array_add (children, NULL);
	loc->children = (GWeatherLocation **)g_ptr_array_free (children, FALSE);
    } else
	g_ptr_array_free (children, TRUE);

    return loc;

error_out:
    gweather_location_unref (loc);
    for (i = 0; i < children->len; i++)
	gweather_location_unref (children->pdata[i]);
    g_ptr_array_free (children, TRUE);

    return NULL;
}

static GWeatherLocation *global_world = NULL;

static void _gweather_location_unref_no_check (GWeatherLocation *loc);

GWEATHER_EXTERN void
_gweather_location_reset_world (void)
{
	g_clear_pointer (&global_world, _gweather_location_unref_no_check);
}

/**
 * gweather_location_get_world:
 *
 * Obtains the shared #GWeatherLocation of type %GWEATHER_LOCATION_WORLD,
 * representing a hierarchy containing all of the locations from
 * Locations.xml.
 *
 * Return value: (allow-none) (transfer none): a %GWEATHER_LOCATION_WORLD
 * location, or %NULL if Locations.xml could not be found or could not be parsed.
 * The return value is owned by libgweather and should not be modified or freed.
 **/
GWeatherLocation *
gweather_location_get_world (void)
{
    GWeatherParser *parser;

    if (!global_world) {
        const char *locations_path;

        locations_path = g_getenv ("LIBGWEATHER_LOCATIONS_PATH");
        if (locations_path) {
            parser = _gweather_parser_new_for_path (locations_path);
            if (!parser) {
                g_warning ("Failed to open '%s' as LIBGWEATHER_LOCATIONS_PATH",
                           locations_path);
                parser = _gweather_parser_new ();
            }
        } else {
            parser = _gweather_parser_new ();
        }
	if (!parser)
	    return NULL;

	global_world = location_new_from_xml (parser, GWEATHER_LOCATION_WORLD, NULL);
	if (!g_getenv ("LIBGWEATHER_LOCATIONS_NO_NEAREST"))
	    add_nearest_weather_stations (global_world);
	_gweather_parser_free (parser);
    }

    return global_world;
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

static void
_gweather_location_unref_no_check (GWeatherLocation *loc)
{
    int i;

    if (--loc->ref_count)
	return;

    g_free (loc->english_name);
    g_free (loc->local_name);
    g_free (loc->msgctxt);
    g_free (loc->local_sort_name);
    g_free (loc->english_sort_name);
    g_free (loc->country_code);
    g_free (loc->tz_hint);
    g_free (loc->station_code);
    g_free (loc->forecast_zone);
    g_free (loc->radar);

    if (loc->children) {
	for (i = 0; loc->children[i]; i++) {
	    loc->children[i]->parent = NULL;
	    gweather_location_unref (loc->children[i]);
	}
	g_free (loc->children);
    }

    if (loc->zones) {
	for (i = 0; loc->zones[i]; i++)
	    gweather_timezone_unref (loc->zones[i]);
	g_free (loc->zones);
    }

    if (loc->metar_code_cache)
	g_hash_table_unref (loc->metar_code_cache);
    if (loc->timezone_cache)
	g_hash_table_unref (loc->timezone_cache);
    if (loc->country_code_cache)
	g_hash_table_unref (loc->country_code_cache);

    g_slice_free (GWeatherLocation, loc);
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
    g_return_if_fail (loc->level != GWEATHER_LOCATION_WORLD || loc->ref_count > 1);

    _gweather_location_unref_no_check (loc);
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

    return loc->local_name;
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
    g_return_val_if_fail (loc != NULL, NULL);
    return loc->local_sort_name;
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

    return loc->english_name;
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
 * Return value: (transfer none) (allow-none): @loc's parent, or %NULL
 * if @loc is a %GWEATHER_LOCATION_WORLD node.
 **/
GWeatherLocation *
gweather_location_get_parent (GWeatherLocation *loc)
{
    g_return_val_if_fail (loc != NULL, NULL);
    return loc->parent;
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
 **/
GWeatherLocation **
gweather_location_get_children (GWeatherLocation *loc)
{
    static GWeatherLocation *no_children = NULL;

    g_return_val_if_fail (loc != NULL, NULL);

    if (loc->children)
	return loc->children;
    else
	return &no_children;
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
    } else if (loc->children) {
        int i;
        for (i = 0; loc->children[i]; i++)
            foreach_city (loc->children[i], callback, user_data, country_code, func, user_data_func);
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
	data->location = location;
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
   /* The data set really isn't too big. Don't concern ourselves
     * with a proper nearest neighbors search. Instead, just do
     * an O(n) search. */
    struct FindNearestCityData data;

    g_return_val_if_fail (loc == NULL || loc->level < GWEATHER_LOCATION_CITY, NULL);

    if (loc == NULL)
	loc = gweather_location_get_world ();

    lat = lat * M_PI / 180.0;
    lon = lon * M_PI / 180.0;

    data.latitude = lat;
    data.longitude = lon;
    data.location = NULL;
    data.distance = 0.0;

    foreach_city (loc, (GFunc) find_nearest_city, &data, NULL, NULL, NULL);

    return gweather_location_ref (data.location);
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
   /* The data set really isn't too big. Don't concern ourselves
     * with a proper nearest neighbors search. Instead, just do
     * an O(n) search. */
    struct FindNearestCityData data;

    g_return_val_if_fail (loc == NULL || loc->level < GWEATHER_LOCATION_CITY ||
			  loc->level == GWEATHER_LOCATION_NAMED_TIMEZONE, NULL);

    if (loc == NULL)
        loc = gweather_location_get_world ();

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
    ArgData *data;
    GeocodeLocation *location;
    GeocodeReverse *reverse;
    GTask *task;

    g_return_if_fail (loc == NULL || loc->level < GWEATHER_LOCATION_CITY ||
		      loc->level == GWEATHER_LOCATION_NAMED_TIMEZONE);

    if (loc == NULL)
        loc = gweather_location_get_world ();

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
    g_return_val_if_fail (loc != NULL, NULL);

    while (loc->parent && !loc->country_code)
	loc = loc->parent;
    return loc->country_code;
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
    const char *tz_hint;
    int i;

    g_return_val_if_fail (loc != NULL, NULL);

    while (loc && !loc->tz_hint)
	loc = loc->parent;
    if (!loc)
	return NULL;
    tz_hint = loc->tz_hint;

    while (loc) {
	while (loc && !loc->zones)
	    loc = loc->parent;
	if (!loc)
	    return NULL;
	for (i = 0; loc->zones[i]; i++) {
	    if (!strcmp (tz_hint, gweather_timezone_get_tzid (loc->zones[i])))
		return loc->zones[i];
	}
	loc = loc->parent;
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
    const char *tz_hint;

    g_return_val_if_fail (loc != NULL, NULL);

    while (loc && !loc->tz_hint)
	loc = loc->parent;
    if (!loc)
	return NULL;
    tz_hint = loc->tz_hint;

    return tz_hint;
}

static void
add_timezones (GWeatherLocation *loc, GPtrArray *zones)
{
    int i;

    if (loc->zones) {
	for (i = 0; loc->zones[i]; i++)
	    g_ptr_array_add (zones, gweather_timezone_ref (loc->zones[i]));
    }
    if (loc->level < GWEATHER_LOCATION_COUNTRY && loc->children) {
	for (i = 0; loc->children[i]; i++)
	    add_timezones (loc->children[i], zones);
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
    return loc->station_code;
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
        return g_strdup (loc->local_name);
    } else if (loc->level == GWEATHER_LOCATION_WEATHER_STATION &&
               loc->parent &&
               loc->parent->level == GWEATHER_LOCATION_CITY) {
        return g_strdup (loc->parent->local_name);
    } else
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
    GWeatherLocation *country;

    g_return_val_if_fail (loc != NULL, NULL);

    country = loc;
    while (country != NULL && country->level != GWEATHER_LOCATION_COUNTRY) {
        country = country->parent;
    }

    return country != NULL ? g_strdup (country->local_name) : NULL;
}

void
_gweather_location_update_weather_location (GWeatherLocation *gloc,
					    WeatherLocation  *loc)
{
    const char *code = NULL, *zone = NULL, *radar = NULL, *tz_hint = NULL, *country = NULL;
    gboolean latlon_valid = FALSE;
    gdouble lat = DBL_MAX, lon = DBL_MAX;
    GWeatherLocation *l;

    if (gloc->level == GWEATHER_LOCATION_CITY && gloc->children)
	l = gloc->children[0];
    else
	l = gloc;

    while (l && (!code || !zone || !radar || !tz_hint || !latlon_valid || !country)) {
	if (!code && l->station_code)
	    code = l->station_code;
	if (!zone && l->forecast_zone)
	    zone = l->forecast_zone;
	if (!radar && l->radar)
	    radar = l->radar;
	if (!tz_hint && l->tz_hint)
	    tz_hint = l->tz_hint;
	if (!country && l->country_code)
	    country = l->country_code;
	if (!latlon_valid && l->latlon_valid) {
	    lat = l->latitude;
	    lon = l->longitude;
	    latlon_valid = TRUE;
	}
	l = l->parent;
    }

    loc->name = g_strdup (gloc->local_name),
    loc->code = g_strdup (code);
    loc->zone = g_strdup (zone);
    loc->radar = g_strdup (radar);
    loc->country_code = g_strdup (country);
    loc->tz_hint = g_strdup (tz_hint);

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
 * Returns: (transfer none): a weather station level #GWeatherLocation for @station_code,
 *          or %NULL if none exists in the database.
 */
GWeatherLocation *
gweather_location_find_by_station_code (GWeatherLocation *world,
					const gchar      *station_code)
{
    GList *l;

    l = g_hash_table_lookup (world->metar_code_cache, station_code);
    return l ? l->data : NULL;
}

/**
 * gweather_location_find_by_country_code:
 * @world: a #GWeatherLocation at the world
 * @country_code: a country code
 *
 * Retrieves the country identified by the specified ISO 3166 code,
 * if present in the database.
 *
 * Returns: (transfer none): a country level #GWeatherLocation, or %NULL.
 */
GWeatherLocation *
gweather_location_find_by_country_code (GWeatherLocation *world,
                                        const gchar      *country_code)
{
	return g_hash_table_lookup (world->country_code_cache, country_code);
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
    int level;

    if (one == two)
	return TRUE;

    if (one->level != two->level &&
	one->level != GWEATHER_LOCATION_DETACHED &&
	two->level != GWEATHER_LOCATION_DETACHED)
	return FALSE;

    level = one->level;
    if (level == GWEATHER_LOCATION_DETACHED)
	level = two->level;

    if (level == GWEATHER_LOCATION_COUNTRY)
	return g_strcmp0 (one->country_code, two->country_code);

    if (level == GWEATHER_LOCATION_ADM1) {
	if (g_strcmp0 (one->english_sort_name, two->english_sort_name) != 0)
	    return FALSE;

	return one->parent && two->parent &&
	    gweather_location_equal (one->parent, two->parent);
    }

    if (g_strcmp0 (one->station_code, two->station_code) != 0)
	return FALSE;

    if (one->level != GWEATHER_LOCATION_DETACHED &&
	two->level != GWEATHER_LOCATION_DETACHED &&
	!gweather_location_equal (one->parent, two->parent))
	return FALSE;

    return ABS(one->latitude - two->latitude) < EPSILON &&
	ABS(one->longitude - two->longitude) < EPSILON;
}

/* ------------------- serialization ------------------------------- */

#define FORMAT 2

static GVariant *
gweather_location_format_two_serialize (GWeatherLocation *location)
{
    const char *name;
    gboolean is_city;
    GVariantBuilder latlon_builder;
    GVariantBuilder parent_latlon_builder;

    name = location->english_name;

    /* Normalize location to be a weather station or detached */
    if (location->level == GWEATHER_LOCATION_CITY) {
        if (location->children != NULL)
            location = location->children[0];
        is_city = TRUE;
    } else {
	is_city = FALSE;
    }

    g_variant_builder_init (&latlon_builder, G_VARIANT_TYPE ("a(dd)"));
    if (location->latlon_valid)
	g_variant_builder_add (&latlon_builder, "(dd)", location->latitude, location->longitude);

    g_variant_builder_init (&parent_latlon_builder, G_VARIANT_TYPE ("a(dd)"));
    if (location->parent && location->parent->latlon_valid)
	g_variant_builder_add (&parent_latlon_builder, "(dd)", location->parent->latitude, location->parent->longitude);

    return g_variant_new ("(ssba(dd)a(dd))",
			  name, location->station_code ? location->station_code : "", is_city,
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

    self = g_slice_new0 (GWeatherLocation);
    self->ref_count = 1;
    self->level = GWEATHER_LOCATION_DETACHED;
    if (name != NULL) {
	self->english_name = g_strdup (name);
	self->local_name = g_strdup (name);

	normalized = g_utf8_normalize (name, -1, G_NORMALIZE_ALL);
	self->english_sort_name = g_utf8_casefold (normalized, -1);
	self->local_sort_name = g_strdup (self->english_sort_name);
	g_free (normalized);
    } else if (nearest_station) {
	self->english_name = g_strdup (nearest_station->english_name);
	self->local_name = g_strdup (nearest_station->local_name);
	self->english_sort_name = g_strdup (nearest_station->english_sort_name);
	self->local_sort_name = g_strdup (nearest_station->local_sort_name);
    }

    self->parent = nearest_station;
    self->children = NULL;

    if (nearest_station)
	self->station_code = g_strdup (nearest_station->station_code);

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
    GList *candidates, *l;
    GWeatherLocation *found;

    /* Since weather stations are no longer attached to cities, first try to
       find what claims to be a city by name and coordinates */
    if (is_city && latitude && longitude) {
        found = gweather_location_find_nearest_city (world,
                                                     latitude / M_PI * 180.0,
                                                     longitude / M_PI * 180.0);
        if (found && (g_strcmp0 (name, found->english_name) == 0 ||
                      g_strcmp0 (name, found->local_name) == 0))
	    return gweather_location_ref (found);
    }

    if (station_code[0] == '\0')
        return _gweather_location_new_detached (NULL, name, latlon_valid, latitude, longitude);

    /* First find the list of candidate locations */
    candidates = g_hash_table_lookup (world->metar_code_cache, station_code);

    /* A station code beginning with @ indicates a named timezone entry, just
     * return it directly
     */
    if (station_code[0] == '@')
       return gweather_location_ref (candidates->data);

    /* If we don't have coordinates, fallback immediately to making up
     * a location
     */
    if (!latlon_valid)
	return candidates ? _gweather_location_new_detached (candidates->data, name, FALSE, 0, 0) : NULL;

    found = NULL;

    /* First try weather stations directly. */
    for (l = candidates; l; l = l->next) {
	GWeatherLocation *ws, *city;

	ws = l->data;

	if (!ws->latlon_valid ||
	    ABS(ws->latitude - latitude) >= EPSILON ||
	    ABS(ws->longitude - longitude) >= EPSILON)
	    /* Not what we're looking for... */
	    continue;

	/* If we can't check for the latitude and longitude
	   of the parent, we just assume we found what we needed
	*/
	if ((!parent_latlon_valid || !ws->parent || !ws->parent->latlon_valid) ||
	    (ABS(parent_latitude - ws->parent->latitude) < EPSILON &&
	     ABS(parent_longitude - ws->parent->longitude) < EPSILON)) {

	    /* Found! Now check which one we want (ws or city) and the name... */
	    if (is_city)
		city = ws->parent;
	    else
		city = ws;

	    if (city == NULL) {
		/* Oops! There is no city for this weather station! */
		continue;
	    }

	    if (name == NULL ||
		g_strcmp0 (name, city->english_name) == 0 ||
		g_strcmp0 (name, city->local_name) == 0)
		found = gweather_location_ref (city);
	    else
		found = _gweather_location_new_detached (ws, name, TRUE, latitude, longitude);

	    break;
	}
    }

    if (found)
	return found;

    /* No weather station matches the serialized data, let's pick
       one at random from the station code list */
    if (candidates)
	return _gweather_location_new_detached (candidates->data,
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
    GWeatherLocation *world, *city;

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
	return _gweather_location_new_detached (city, name,
						TRUE, latitude, longitude);
    }
}
