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
#include <gtk/gtk.h>
#include <libxml/xmlreader.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include "gweather-location.h"
#include "gweather-timezone.h"
#include "parser.h"
#include "weather-priv.h"

/* This is the precision of coordinates in the database */
#define EPSILON 0.000001

/**
 * SECTION:gweather-location
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
 * @GWEATHER_LOCATION_ADM2: A location representing a subdivision of a
 * %GWEATHER_LOCATION_ADM1 location, or a direct subdivision of
 * a country that is not represented in a #GWeatherLocationEntry.
 * @GWEATHER_LOCATION_CITY: A location representing a city
 * @GWEATHER_LOCATION_WEATHER_STATION: A location representing a
 * weather station.
 * @GWEATHER_LOCATION_DETACHED: A location that is detached from the
 * database, for example because it was loaded from external storage
 * and could not be fully recovered. The parent of this location is
 * the nearest weather station.
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
 * Building a database with gweather_location_new_world() will never
 * create detached instances, but deserializing might.
 **/

static int
sort_locations_by_name (gconstpointer a, gconstpointer b)
{
    GWeatherLocation *loc_a = *(GWeatherLocation **)a;
    GWeatherLocation *loc_b = *(GWeatherLocation **)b;

    return g_utf8_collate (loc_a->sort_name, loc_b->sort_name);
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
location_new_from_xml (GWeatherParser *parser, GWeatherLocationLevel level,
		       GWeatherLocation *parent)
{
    GWeatherLocation *loc, *child;
    GPtrArray *children = NULL;
    const char *tagname;
    char *value, *normalized;
    int tagtype, i;

    loc = g_slice_new0 (GWeatherLocation);
    loc->latitude = loc->longitude = DBL_MAX;
    loc->parent = parent;
    loc->level = level;
    loc->ref_count = 1;
    if (level == GWEATHER_LOCATION_WORLD)
	loc->metar_code_cache = g_hash_table_ref (parser->metar_code_cache);
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
	if (!strcmp (tagname, "name") && !loc->name) {
	    value = gweather_parser_get_localized_value (parser);
	    if (!value)
		goto error_out;
	    loc->name = g_strdup (value);
	    xmlFree (value);
	    normalized = g_utf8_normalize (loc->name, -1, G_NORMALIZE_ALL);
	    loc->sort_name = g_utf8_casefold (normalized, -1);
	    g_free (normalized);

	} else if (!strcmp (tagname, "iso-code") && !loc->country_code) {
	    value = gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;
	    loc->country_code = g_strdup (value);
	    xmlFree (value);
	} else if (!strcmp (tagname, "tz-hint") && !loc->tz_hint) {
	    value = gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;
	    loc->tz_hint = g_strdup (value);
	    xmlFree (value);
	} else if (!strcmp (tagname, "code") && !loc->station_code) {
	    value = gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;
	    loc->station_code = g_strdup (value);
	    xmlFree (value);
	} else if (!strcmp (tagname, "coordinates") && !loc->latlon_valid) {
	    value = gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;
	    if (parse_coordinates (value, &loc->latitude, &loc->longitude))
		loc->latlon_valid = TRUE;
	    xmlFree (value);
	} else if (!strcmp (tagname, "zone") && !loc->forecast_zone) {
	    value = gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;
	    loc->forecast_zone = g_strdup (value);
	    xmlFree (value);
	} else if (!strcmp (tagname, "yahoo-woeid") && !loc->yahoo_id) {
	    value = gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;
	    loc->yahoo_id = g_strdup (value);
	    xmlFree (value);
	} else if (!strcmp (tagname, "radar") && !loc->radar) {
	    value = gweather_parser_get_value (parser);
	    if (!value)
		goto error_out;
	    loc->radar = g_strdup (value);
	    xmlFree (value);

	} else if (!strcmp (tagname, "region")) {
	    child = location_new_from_xml (parser, GWEATHER_LOCATION_REGION, loc);
	    if (!child)
		goto error_out;
	    if (parser->use_regions)
		g_ptr_array_add (children, child);
	    else {
		if (child->children) {
		    for (i = 0; child->children[i]; i++) {
			/* Correct back pointers */
			child->children[i]->parent = loc;
			g_ptr_array_add (children, child->children[i]);
		    }
		}
		child->children = NULL;
		gweather_location_unref (child);
	    }
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
	} else if (!strcmp (tagname, "province")) {
	    child = location_new_from_xml (parser, GWEATHER_LOCATION_ADM2, loc);
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

	} else if (!strcmp (tagname, "timezones")) {
	    loc->zones = gweather_timezones_parse_xml (parser);
	    if (!loc->zones)
		goto error_out;

	} else {
	    if (xmlTextReaderNext (parser->xml) != 1)
		goto error_out;
	}
    }
    if (xmlTextReaderRead (parser->xml) != 1 && parent)
	goto error_out;

    if (level == GWEATHER_LOCATION_WEATHER_STATION) {
	/* Cache weather stations by METAR code */
	GList *a, *b;

	a = g_hash_table_lookup (parser->metar_code_cache, loc->station_code);
	b = g_list_append (a, gweather_location_ref (loc));
	if (b != a)
	    g_hash_table_replace (parser->metar_code_cache, loc->station_code, b);
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

/**
 * gweather_location_new_world:
 * @use_regions: whether or not to divide the world into regions
 *
 * Creates a new #GWeatherLocation of type %GWEATHER_LOCATION_WORLD,
 * representing a hierarchy containing all of the locations from
 * Locations.xml.
 *
 * If @use_regions is %TRUE, the immediate children of the returned
 * location will be %GWEATHER_LOCATION_REGION nodes, representing the
 * top-level "regions" of Locations.xml (the continents and a few
 * other divisions), and the country-level nodes will be the children
 * of the regions. If @use_regions is %FALSE, the regions will be
 * skipped, and the children of the returned location will be the
 * %GWEATHER_LOCATION_COUNTRY nodes.
 *
 * Return value: (allow-none): a %GWEATHER_LOCATION_WORLD location, or
 * %NULL if Locations.xml could not be found or could not be parsed.
 **/
GWeatherLocation *
gweather_location_new_world (gboolean use_regions)
{
    GWeatherParser *parser;
    GWeatherLocation *world;

    parser = gweather_parser_new (use_regions);
    if (!parser)
	return NULL;

    world = location_new_from_xml (parser, GWEATHER_LOCATION_WORLD, NULL);

    gweather_parser_free (parser);
    return world;
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
    int i;

    g_return_if_fail (loc != NULL);

    if (--loc->ref_count)
	return;
    
    g_free (loc->name);
    g_free (loc->sort_name);
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
 * Note that %GWEATHER_LOCATION_WEATHER_STATION nodes are not
 * localized, and so the name returned for those nodes will always be
 * in English, and should therefore not be displayed to the user.
 * (FIXME: should we just not return a name?)
 *
 * Return value: @loc's name
 **/
const char *
gweather_location_get_name (GWeatherLocation *loc)
{
    g_return_val_if_fail (loc != NULL, NULL);
    return loc->name;
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
    return loc->sort_name;
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


/**
 * gweather_location_free_children:
 * @loc: a #GWeatherLocation
 * @children: an array of @loc's children
 *
 * This is a no-op. Do not use it.
 *
 * Deprecated: This is a no-op.
 **/
void
gweather_location_free_children (GWeatherLocation  *loc,
				 GWeatherLocation **children)
{
    ;
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
    /* average radius of the earth in km */
    static const double radius = 6372.795;

    g_return_val_if_fail (loc != NULL, 0);
    g_return_val_if_fail (loc2 != NULL, 0);

    //g_return_val_if_fail (loc->latlon_valid, 0.0);
    //g_return_val_if_fail (loc2->latlon_valid, 0.0);

    return acos (cos (loc->latitude) * cos (loc2->latitude) * cos (loc->longitude - loc2->longitude) +
		 sin (loc->latitude) * sin (loc2->latitude)) * radius;
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
 * Return value: (allow-none) @loc's city name, or %NULL
 **/
char *
gweather_location_get_city_name (GWeatherLocation *loc)
{
    g_return_val_if_fail (loc != NULL, NULL);

    if (loc->level == GWEATHER_LOCATION_CITY ||
	loc->level == GWEATHER_LOCATION_DETACHED)
	return g_strdup (loc->name);
    else if (loc->level == GWEATHER_LOCATION_WEATHER_STATION &&
	     loc->parent &&
	     loc->parent->level == GWEATHER_LOCATION_CITY)
	return g_strdup (loc->parent->name);
    else
	return NULL;
}

void
_gweather_location_update_weather_location (GWeatherLocation *gloc,
					    WeatherLocation  *loc)
{
    const char *code = NULL, *zone = NULL, *yahoo_id = NULL, *radar = NULL, *tz_hint = NULL, *country = NULL;
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
	if (!yahoo_id && l->yahoo_id)
	    yahoo_id = l->yahoo_id;
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

    loc->name = g_strdup (gweather_location_get_name (gloc)),
    loc->code = g_strdup (code);
    loc->zone = g_strdup (zone);
    loc->yahoo_id = g_strdup (yahoo_id);
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
 * Returns: a weather station level #GWeatherLocation for @station_code,
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

GWeatherLocation *
gweather_location_ref_world (GWeatherLocation *loc)
{
    while (loc &&
	   loc->level != GWEATHER_LOCATION_WORLD)
	loc = loc->parent;

    if (loc)
	gweather_location_ref (loc);
    return loc;
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

    if (level == GWEATHER_LOCATION_ADM1 ||
	level == GWEATHER_LOCATION_ADM2) {
	if (g_strcmp0 (one->sort_name, two->sort_name) != 0)
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

#define FORMAT 1

static GVariant *
gweather_location_format_one_serialize (GWeatherLocation *location)
{
    const char *name;
    gboolean is_city;

    name = location->name;

    /* Normalize location to be a weather station or detached */
    if (location->level == GWEATHER_LOCATION_CITY) {
	location = location->children[0];
	is_city = TRUE;
    } else {
	is_city = FALSE;
    }

    return g_variant_new ("(ssbm(dd)m(dd))",
			  name, location->station_code, is_city,
			  location->latlon_valid,
			  location->latitude,
			  location->longitude,
			  location->parent && location->parent->latlon_valid,
			  location->parent ? location->parent->latitude : 0.0d,
			  location->parent ? location->parent->longitude : 0.0d);
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
    self->level = GWEATHER_LOCATION_DETACHED;
    self->name = g_strdup (name);

    normalized = g_utf8_normalize (name, -1, G_NORMALIZE_ALL);
    self->sort_name = g_utf8_casefold (normalized, -1);
    g_free (normalized);

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
gweather_location_format_one_deserialize (GWeatherLocation *world,
					  GVariant         *variant)
{
    const char *name;
    const char *station_code;
    gboolean is_city, latlon_valid, parent_latlon_valid;
    gdouble latitude, longitude, parent_latitude, parent_longitude;
    GList *candidates, *l;
    GWeatherLocation *found;

    /* This one instead is a critical, because format is specified in
       the containing variant */
    g_return_val_if_fail (g_variant_is_of_type (variant,
						G_VARIANT_TYPE ("(ssbm(dd)m(dd))")), NULL);

    g_variant_get (variant, "(&s&sbm(dd)m(dd))", &name, &station_code, &is_city,
		   &latlon_valid, &latitude, &longitude,
		   &parent_latlon_valid, &parent_latitude, &parent_longitude);

    /* First find the list of candidate locations */
    candidates = g_hash_table_lookup (world->metar_code_cache, station_code);

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

	    if (g_strcmp0 (name, city->name) == 0)
		found = gweather_location_ref (city);
	    else
		found = _gweather_location_new_detached (city, name, FALSE, 0, 0);

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

/**
 * gweather_location_serialize:
 * @location: a #GWeatherLocation
 *
 * Returns: (transfer none):
 */
GVariant *
gweather_location_serialize (GWeatherLocation *location)
{
    g_return_val_if_fail (location != NULL, NULL);
    g_return_val_if_fail (location->level >= GWEATHER_LOCATION_CITY, NULL);

    return g_variant_new ("(uv)", FORMAT,
			  gweather_location_format_one_serialize (location));
}

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

    if (format == FORMAT)
	loc = gweather_location_format_one_deserialize (world, v);
    else
	loc = NULL;

    g_variant_unref (v);
    return loc;
}
