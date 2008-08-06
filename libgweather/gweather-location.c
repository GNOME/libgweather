/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gweather-locations.c - Location-handling code
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

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

struct _GWeatherLocation {
    char *name, *sort_name;
    GWeatherLocation *parent, **children;
    GWeatherLocationLevel level;
    char *country_code, *tz_hint;
    char *station_code, *forecast_zone, *radar;
    double latitude, longitude;
    gboolean latlon_valid;
    GWeatherTimezone **zones;

    int ref_count;
};

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

static char *
unparse_coordinates (double latitude, double longitude)
{
    int lat_d, lat_m, lat_s, lon_d, lon_m, lon_s;
    char lat_dir, lon_dir;

    latitude = latitude * 180.0 / M_PI;
    longitude = longitude * 180.0 / M_PI;

    if (latitude < 0.0) {
	lat_dir = 'S';
	latitude = -latitude;
    } else
	lat_dir = 'N';
    if (longitude < 0.0) {
	lon_dir = 'W';
	longitude = -longitude;
    } else
	lon_dir = 'E';

    lat_d = (int)latitude;
    lat_m = (int)(latitude * 60.0) - lat_d * 60;
    lat_s = (int)(latitude * 3600.0) - lat_d * 3600 - lat_m * 60;
    lon_d = (int)longitude;
    lon_m = (int)(longitude * 60.0) - lon_d * 60;
    lon_s = (int)(longitude * 3600.0) - lon_d * 3600 - lon_m * 60;

    return g_strdup_printf ("%02d-%02d-%02d%c %03d-%02d-%02d%c",
			    lat_d, lat_m, lat_s, lat_dir,
			    lon_d, lon_m, lon_s, lon_dir);
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
    loc->parent = parent;
    loc->level = level;
    loc->ref_count = 1;
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
		    for (i = 0; child->children[i]; i++)
			g_ptr_array_add (children, gweather_location_ref (child->children[i]));
		}
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

GWeatherLocation *
gweather_location_ref (GWeatherLocation *loc)
{
    loc->ref_count++;
    return loc;
}

void
gweather_location_unref (GWeatherLocation *loc)
{
    int i;

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

    g_slice_free (GWeatherLocation, loc);
}

const char *
gweather_location_get_name (GWeatherLocation *loc)
{
    return loc->name;
}

const char *
gweather_location_get_sort_name (GWeatherLocation *loc)
{
    return loc->sort_name;
}

GWeatherLocationLevel
gweather_location_get_level (GWeatherLocation *loc)
{
    return loc->level;
}

GWeatherLocation *
gweather_location_get_parent (GWeatherLocation *loc)
{
    return loc->parent;
}

GWeatherLocation **
gweather_location_get_children (GWeatherLocation *loc)
{
    gweather_location_ref (loc);
    if (loc->children)
	return loc->children;
    else
	return g_new0 (GWeatherLocation *, 1);
}

void
gweather_location_free_children (GWeatherLocation  *loc,
				 GWeatherLocation **children)
{
    if (!loc->children)
	g_free (children);
    gweather_location_unref (loc);
}

gboolean
gweather_location_has_coords (GWeatherLocation *loc)
{
    return loc->latlon_valid;
}

void
gweather_location_get_coords (GWeatherLocation *loc,
			      double *latitude, double *longitude)
{
    //g_return_if_fail (loc->latlon_valid);

    *latitude = loc->latitude / M_PI * 180.0;
    *longitude = loc->longitude / M_PI * 180.0;
}

double
gweather_location_get_distance (GWeatherLocation *loc, GWeatherLocation *loc2)
{
    /* average radius of the earth in km */
    static const double radius = 6372.795;

    //g_return_val_if_fail (loc->latlon_valid, 0.0);
    //g_return_val_if_fail (loc2->latlon_valid, 0.0);

    return acos (cos (loc->latitude) * cos (loc2->latitude) * cos (loc->longitude - loc2->longitude) +
		 sin (loc->latitude) * sin (loc2->latitude)) * radius;
}

const char *
gweather_location_get_country (GWeatherLocation *loc)
{
    while (loc->parent && !loc->country_code)
	loc = loc->parent;
    return loc->country_code;
}

GWeatherTimezone *
gweather_location_get_timezone (GWeatherLocation *loc)
{
    const char *tz_hint;
    int i;

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

GWeatherTimezone **
gweather_location_get_timezones (GWeatherLocation *loc)
{
    GPtrArray *zones;

    zones = g_ptr_array_new ();
    add_timezones (loc, zones);
    g_ptr_array_add (zones, NULL);
    return (GWeatherTimezone **)g_ptr_array_free (zones, FALSE);
}

void
gweather_location_free_timezones (GWeatherLocation  *loc,
				  GWeatherTimezone **zones)
{
    int i;

    for (i = 0; zones[i]; i++)
	gweather_timezone_unref (zones[i]);
    g_free (zones);
}

const char *
gweather_location_get_code (GWeatherLocation *loc)
{
    return loc->station_code;
}

char *
gweather_location_get_city_name (GWeatherLocation *loc)
{
    if (loc->level == GWEATHER_LOCATION_CITY)
	return g_strdup (loc->name);
    else if (loc->level == GWEATHER_LOCATION_WEATHER_STATION &&
	     loc->parent &&
	     loc->parent->level == GWEATHER_LOCATION_CITY)
	return g_strdup (loc->parent->name);
    else
	return NULL;
}

WeatherLocation *
gweather_location_to_weather_location (GWeatherLocation *gloc,
				       const char *name)
{
    const char *code = NULL, *zone = NULL, *radar = NULL, *tz_hint = NULL;
    GWeatherLocation *l;
    WeatherLocation *wloc;
    char *coords;

    if (!name)
	name = gweather_location_get_name (gloc);

    if (gloc->level == GWEATHER_LOCATION_CITY && gloc->children)
	l = gloc->children[0];
    else
	l = gloc;

    if (l->latlon_valid)
	coords = unparse_coordinates (l->latitude, l->longitude);
    else
	coords = NULL;

    while (l && (!code || !zone || !radar || !tz_hint)) {
	if (!code && l->station_code)
	    code = l->station_code;
	if (!zone && l->forecast_zone)
	    zone = l->forecast_zone;
	if (!radar && l->radar)
	    radar = l->radar;
	if (!tz_hint && l->tz_hint)
	    tz_hint = l->tz_hint;
	l = l->parent;
    }

    wloc = weather_location_new (name, code, zone, radar, coords,
				 gweather_location_get_country (gloc),
				 tz_hint);
    g_free (coords);
    return wloc;
}

WeatherInfo *
gweather_location_get_weather (GWeatherLocation *loc)
{
    WeatherLocation *wloc;
    WeatherInfo *info;

    wloc = gweather_location_to_weather_location (loc, NULL);
    info = weather_info_new (wloc, NULL, NULL, NULL);
    weather_location_free (wloc);
    return info;
}
