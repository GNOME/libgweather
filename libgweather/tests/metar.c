/* metar.c: METAR tests
 *
 * SPDX-FileCopyrightText: 2021  Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gweather-test-utils.h"

#include <libsoup/soup.h>

/* For test_metar_weather_stations */
#define METAR_SOURCES "https://www.aviationweather.gov/docs/metar/stations.txt"

static GHashTable *
parse_metar_stations (const char *contents)
{
    GHashTable *stations_ht;
    char **lines;
    guint i, num_stations;

    stations_ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    num_stations = 0;
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

        /* If it is a duplicate discard it */
        if (g_hash_table_lookup (stations_ht, station)) {
            g_test_message ("Weather station '%s' already defined\n", station);
            g_free (station);
            continue;
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
                            GHashTable *stations_ht)
{
    const char *code, *line;

    code = gweather_location_get_code (location);
    g_assert_nonnull (code);

    line = g_hash_table_lookup (stations_ht, code);
    if (!line) {
        g_test_message ("Could not find airport for '%s' in " METAR_SOURCES "\n", code);
        g_test_fail ();
    } else {
        char *has_metar;

        has_metar = g_strndup (line + 62, 1);
        if (*has_metar == 'Z') {
            g_test_message ("Airport weather station '%s' is obsolete\n", code);
            g_test_fail ();
        } else if (*has_metar == ' ') {
            g_test_message ("Could not find weather station for '%s' in " METAR_SOURCES "\n", code);
            g_test_fail ();
        }
        g_free (has_metar);
    }
}

static void
test_metar_weather_stations_children (GWeatherLocation *location,
                                      GHashTable *stations_ht)
{
    g_autoptr (GWeatherLocation) child = NULL;

    while ((child = gweather_location_next_child (location, child)) != NULL) {
        if (gweather_location_get_level (child) == GWEATHER_LOCATION_WEATHER_STATION)
            test_metar_weather_station (child, stations_ht);
        else
            test_metar_weather_stations_children (child, stations_ht);
    }
}

static void
test_metar_weather_stations (void)
{
    g_autoptr (GWeatherLocation) world = NULL;
    SoupMessage *msg;
    SoupSession *session;
    GHashTable *stations_ht;
    char *contents;
#if SOUP_CHECK_VERSION(2, 99, 2)
    GBytes *body;
    GError *error = NULL;
    gsize bsize;
#endif

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    msg = soup_message_new ("GET", METAR_SOURCES);
    session = soup_session_new ();
#if SOUP_CHECK_VERSION(2, 99, 2)
    body = soup_session_send_and_read (session, msg, NULL, &error);
    if (error && error->domain == G_TLS_ERROR) {
#else
    soup_session_send_message (session, msg);
    if (msg->status_code == SOUP_STATUS_SSL_FAILED) {
#endif
        g_test_message ("SSL/TLS failure, please check your glib-networking installation");
        g_test_failed ();
        return;
    }
#if SOUP_CHECK_VERSION(2, 99, 2)
    g_assert_no_error (error);
    g_assert_cmpint (soup_message_get_status (msg), >=, 200);
    g_assert_cmpint (soup_message_get_status (msg), <, 300);
    g_assert_nonnull (body);
    contents = g_bytes_unref_to_data (body, &bsize);
#else
    g_assert_cmpint (msg->status_code, >=, 200);
    g_assert_cmpint (msg->status_code, <, 300);
    g_assert_nonnull (msg->response_body);
    contents = g_strndup (msg->response_body->data, msg->response_body->length);
#endif
    g_object_unref (session);
    g_object_unref (msg);

    stations_ht = parse_metar_stations (contents);
    g_assert_nonnull (stations_ht);
    g_free (contents);

    test_metar_weather_stations_children (world, stations_ht);

    g_hash_table_unref (stations_ht);

    g_clear_object (&world);

    gweather_test_reset_world ();
}

int
main (int argc, char *argv[])
{
    setlocale (LC_ALL, "");

    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("http://gitlab.gnome.org/GNOME/libgweather/issues/");

    g_autofree char *schemas_dir = gweather_test_setup_gsettings ();

    g_test_add_func ("/weather/metar_weather_stations", test_metar_weather_stations);

    int res = g_test_run ();

    gweather_test_teardown_gsettings (schemas_dir);

    return res;
}
