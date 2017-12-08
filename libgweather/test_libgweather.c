/*
 * (c) 2017 Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301  USA.
 */

#include "config.h"

#include <locale.h>
#include <string.h>
#include <libsoup/soup.h>

#include <gweather-version.h>
#include "gweather-location.h"

/* For test_metar_weather_stations */
#define METAR_SOURCES "https://www.aviationweather.gov/docs/metar/stations.txt"

/* Maximum for test_airport_distance_sanity() */
#define TOO_FAR 100.0
static double max_distance = 0.0;

static void
test_named_timezones (void)
{
    GWeatherLocation *world, **children;
    guint i;

    world = gweather_location_get_world ();
    g_assert (world);

    children = gweather_location_get_children (world);
    for (i = 0; children[i] != NULL; i++) {
        GWeatherLocationLevel level;
        const char *code;

        level = gweather_location_get_level (children[i]);
        if (level != GWEATHER_LOCATION_NAMED_TIMEZONE)
            continue;

        /* Verify that timezone codes start with a '@' */
        code = gweather_location_get_code (children[i]);
        g_assert_nonnull (code);
        g_assert_true (code[0] == '@');
    }
}

static void
test_timezone (GWeatherLocation *location)
{
    GWeatherTimezone *gtz;
    const char *tz;

    tz = gweather_location_get_timezone_str (location);
    if (!tz) {
        GWeatherTimezone **tzs;

        tzs = gweather_location_get_timezones (location);
        g_assert (tzs);

        /* Only countries should have multiple timezones associated */
        if (tzs[0] == NULL ||
            gweather_location_get_level (location) > GWEATHER_LOCATION_COUNTRY) {
            g_test_message ("Location '%s' does not have an associated timezone",
                            gweather_location_get_name (location));
            g_test_fail ();
        }
        gweather_location_free_timezones (location, tzs);
        return;
    }

    gtz = gweather_timezone_get_by_tzid (tz);
    if (!gtz) {
        g_test_message ("Location '%s' has invalid timezone '%s'",
                        gweather_location_get_name (location),
                        tz);
        g_test_fail ();
    }
}

static void
test_timezones_children (GWeatherLocation *location)
{
    GWeatherLocation **children;
    guint i;

    children = gweather_location_get_children (location);
    for (i = 0; children[i] != NULL; i++) {
        if (gweather_location_get_level (children[i]) >= GWEATHER_LOCATION_COUNTRY)
            test_timezone (children[i]);

        test_timezones_children (children[i]);
    }
}

static void
test_timezones (void)
{
    GWeatherLocation *world;

    world = gweather_location_get_world ();
    g_assert (world);

    test_timezones_children (world);
}

static void
test_distance (GWeatherLocation *location)
{
    GWeatherLocation *parent;
    double distance;

    parent = gweather_location_get_parent (location);
    distance = gweather_location_get_distance (location, parent);

    if (distance > TOO_FAR) {
        g_test_message ("Airport '%s' is too far from city '%s' (%.1lf km)",
                        gweather_location_get_name (location),
                        gweather_location_get_name (parent),
                        distance);
        max_distance = MAX(max_distance, distance);
        g_test_fail ();
    }
}

static void
test_airport_distance_children (GWeatherLocation *location)
{
    GWeatherLocation **children;
    guint i;

    children = gweather_location_get_children (location);
    for (i = 0; children[i] != NULL; i++) {
        if (gweather_location_get_level (children[i]) == GWEATHER_LOCATION_WEATHER_STATION)
            test_distance (children[i]);
        else
            test_airport_distance_children (children[i]);
    }
}

static void
test_airport_distance_sanity (void)
{
    GWeatherLocation *world;

    world = gweather_location_get_world ();
    g_assert (world);

    test_airport_distance_children (world);

    if (g_test_failed ())
        g_warning ("Maximum city to airport distance is %.1f km", max_distance);
}

static GHashTable *
parse_metar_stations (const char *contents)
{
    GHashTable *stations_ht;
    char **lines;
    guint i, num_stations;

    stations_ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    lines = g_strsplit (contents, "\n", -1);

    for (i = 0; lines[i] != NULL; i++) {
        char *line = lines[i];
        char *station;

        if (line[0] == '!')
            continue;

        if (strlen (line) != 83)
            continue;

        station = g_strndup (line + 20, 4);
        /* Skip stations with no ICAO code */
        if (g_str_equal (station, "    ")) {
            g_free (station);
            continue;
        }

        if (g_hash_table_lookup (stations_ht, station)) {
            if (g_str_equal (station, "VOGO") ||
                g_str_equal (station, "KHQG")) {
                g_free (station);
                continue;
            }
            g_test_message ("Weather station '%s' already defined", station);
        }

        g_hash_table_insert (stations_ht, station, g_strdup (line));
        num_stations++;
    }

    g_strfreev (lines);

    /* Duplicates? */
    g_assert_cmpuint (num_stations, ==, g_hash_table_size (stations_ht));

    g_test_message ("Parsed %u weather stations", num_stations);

    return stations_ht;
}

static void
test_metar_weather_station (GWeatherLocation *location,
                            GHashTable       *stations_ht)
{
    const char *code, *line;

    code = gweather_location_get_code (location);
    g_assert_nonnull (code);

    line = g_hash_table_lookup (stations_ht, code);
    if (!line) {
        g_test_message ("Could not find airport for '%s'", code);
        g_test_fail ();
    } else {
        char *has_metar;

        has_metar = g_strndup (line + 62, 1);
        if (*has_metar == 'Z') {
            g_test_message ("Airport weather station '%s' is obsolete", code);
            g_test_fail ();
        } else if (*has_metar == ' ') {
            g_test_message ("Could not find weather station for '%s'", code);
            g_test_fail ();
        }
        g_free (has_metar);
    }
}

static void
test_metar_weather_stations_children (GWeatherLocation *location,
                                      GHashTable       *stations_ht)
{
    GWeatherLocation **children;
    guint i;

    children = gweather_location_get_children (location);
    for (i = 0; children[i] != NULL; i++) {
        if (gweather_location_get_level (children[i]) == GWEATHER_LOCATION_WEATHER_STATION)
            test_metar_weather_station (children[i], stations_ht);
        else
            test_metar_weather_stations_children (children[i], stations_ht);
    }
}

static void
test_metar_weather_stations (void)
{
    GWeatherLocation *world;
    SoupMessage *msg;
    SoupSession *session;
    GHashTable *stations_ht;
    char *contents;

    world = gweather_location_get_world ();
    g_assert (world);

    msg = soup_message_new ("GET", METAR_SOURCES);
    session = soup_session_new ();
    soup_session_send_message (session, msg);
    g_assert (SOUP_STATUS_IS_SUCCESSFUL (msg->status_code));
    g_object_unref (session);
    g_assert_nonnull (msg->response_body);

    contents = g_strndup (msg->response_body->data, msg->response_body->length);
    g_object_unref (msg);

    stations_ht = parse_metar_stations (contents);
    g_assert_nonnull (stations_ht);
    g_free (contents);

    test_metar_weather_stations_children (world, stations_ht);
}

static void
log_handler (const char *log_domain, GLogLevelFlags log_level, const char *message, gpointer user_data)
{
	g_test_message ("%s", message);
}

int
main (int argc, char *argv[])
{
	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

	/* We need to handle log messages produced by g_message so they're interpreted correctly by the GTester framework */
	g_log_set_handler (NULL, G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG, log_handler, NULL);

	g_setenv ("LIBGWEATHER_LOCATIONS_PATH",
		  TEST_SRCDIR "../data/Locations.xml",
		  FALSE);

	g_test_add_func ("/weather/named-timezones", test_named_timezones);
	g_test_add_func ("/weather/timezones", test_timezones);
	g_test_add_func ("/weather/airport_distance_sanity", test_airport_distance_sanity);
	g_test_add_func ("/weather/metar_weather_stations", test_metar_weather_stations);

	return g_test_run ();
}
