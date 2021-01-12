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
#include "gweather-weather.h"
#include "gweather-private.h"

/* We use/test gweather_location_get_children */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

extern void _gweather_location_reset_world (void);

/* For test_metar_weather_stations */
#define METAR_SOURCES "https://www.aviationweather.gov/docs/metar/stations.txt"

/* Maximum for test_airport_distance_sanity() */
#define TOO_FAR 100.0
static double max_distance = 0.0;

static void
test_named_timezones (void)
{
    g_autoptr(GWeatherLocation) world = NULL;
    GWeatherLocation **children;
    guint i;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

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

    g_clear_pointer (&world, gweather_location_unref);
    _gweather_location_reset_world ();
}

static GList *
get_list_from_configuration (GWeatherLocation *world,
                             const char *str,
                             gsize n_expected_items)
{
    GList *list;
    GVariant *v;
    guint i;

    /* The format of the CONFIGURATION is "aa{sv}" */
    v = g_variant_parse (NULL,
                         str,
                         NULL,
                         NULL,
                         NULL);
    g_assert_cmpint (g_variant_n_children (v), ==, n_expected_items);

    list = NULL;

    for (i = 0; i < g_variant_n_children (v); i++) {
        GVariantIter iteri;
        GVariant *child;
        char *key;
        GVariant *value;

        child = g_variant_get_child_value (v, i);
        g_variant_iter_init (&iteri, child);
        while (g_variant_iter_next (&iteri, "{sv}", &key, &value)) {
            GWeatherLocation *loc;

            if (g_strcmp0 (key, "location") != 0) {
                g_variant_unref (value);
                g_free (key);
                continue;
            }

            loc = gweather_location_deserialize (world, value);
            g_assert_nonnull (loc);
            list = g_list_prepend (list, loc);

            g_variant_unref (value);
            g_free (key);
        }

	g_variant_unref (child);
    }

    g_variant_unref (v);

    g_assert_cmpint (g_list_length (list), ==, n_expected_items);

    return list;
}

#define CONFIGURATION "[                                                                                                                                                                 \
                        {'location': <(uint32 2, <('Rio de Janeiro', 'SBES', false, [(-0.39822596348113698, -0.73478361508961265)], [(-0.39822596348113698, -0.73478361508961265)])>)>}, \
                        {'location': <(uint32 2, <('Coordinated Universal Time (UTC)', '@UTC', false, @a(dd) [], @a(dd) [])>)>},                                                         \
                        {'location': <(uint32 2, <('Perm', 'USPP', true, [(1.0122909661567112, 0.98174770424681035)], [(1.0122909661567112, 0.98174770424681035)])>)>}                   \
                       ]"

static void test_timezones (void);

static void
test_named_timezones_deserialized (void)
{
    g_autoptr(GWeatherLocation) world = NULL;
    GList *list, *l;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    list = get_list_from_configuration (world, CONFIGURATION, 3);
    for (l = list; l != NULL; l = l->next)
        gweather_location_unref (l->data);
    g_list_free (list);

    list = get_list_from_configuration (world, CONFIGURATION, 3);
    for (l = list; l != NULL; l = l->next) {
        GWeatherLocation *loc = l->data;
        GWeatherTimezone *tz;
        const char *tzid;

        tz = gweather_location_get_timezone (loc);
        g_assert_nonnull (tz);
        tzid = gweather_timezone_get_tzid (tz);
        g_assert_nonnull (tzid);
        gweather_location_get_level (loc);

        gweather_location_unref (loc);
    }
    g_list_free (list);

    g_clear_pointer (&world, gweather_location_unref);
    /* test_timezones will clear the DB */
    test_timezones ();
}

static void
test_no_code_serialize (void)
{
    GVariant *variant;
    g_autoptr(GWeatherLocation) world = NULL;
    g_autoptr(GWeatherLocation) loc = NULL;
    g_autoptr(GWeatherLocation) new_loc = NULL;
    GString *str;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    loc = gweather_location_find_nearest_city (world, 56.833333, 53.183333);
    g_assert_nonnull (loc);
    g_assert_cmpstr (gweather_location_get_name (loc), ==, "Izhevsk");
    g_assert_null (gweather_location_get_code (loc));

    variant = gweather_location_serialize (loc);
    g_assert_nonnull (variant);
    str = g_variant_print_string (variant, NULL, TRUE);
    g_message ("variant: %s", str->str);
    g_string_free (str, TRUE);

    new_loc = gweather_location_deserialize (world, variant);
    g_variant_unref (variant);
    g_assert_nonnull (new_loc);
    g_assert_cmpstr (gweather_location_get_name (loc), ==, gweather_location_get_name (new_loc));
    g_assert_true (gweather_location_equal (loc, new_loc));

    g_clear_pointer (&world, gweather_location_unref);
    g_clear_pointer (&loc, gweather_location_unref);
    g_clear_pointer (&new_loc, gweather_location_unref);
    _gweather_location_reset_world ();
}

static void
test_timezone (GWeatherLocation *location)
{
    g_autoptr(GWeatherTimezone) gtz = NULL;
    const char *tz;

    tz = gweather_location_get_timezone_str (location);
    if (!tz) {
        GWeatherTimezone **tzs;

        tzs = gweather_location_get_timezones (location);
        g_assert_nonnull (tzs);

        /* Only countries should have multiple timezones associated */
        if ((tzs[0] == NULL && gweather_location_get_level (location) < GWEATHER_LOCATION_WEATHER_STATION) &&
            gweather_location_get_level (location) >= GWEATHER_LOCATION_COUNTRY) {
            g_print ("Location '%s' does not have an associated timezone\n",
                     gweather_location_get_name (location));
            g_test_fail ();
        }
        gweather_location_free_timezones (location, tzs);
        return;
    }

    gtz = gweather_timezone_get_by_tzid (tz);
    if (!gtz) {
        g_print ("Location '%s' has invalid timezone '%s'\n",
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
    g_autoptr(GWeatherLocation) world = NULL;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    test_timezones_children (world);

    g_clear_pointer (&world, gweather_location_unref);
    _gweather_location_reset_world ();
}

static void
test_distance (GWeatherLocation *location)
{
    g_autoptr(GWeatherLocation) parent = NULL;
    double distance;

    parent = gweather_location_get_parent (location);
    if (gweather_location_get_level (parent) < GWEATHER_LOCATION_CITY)
      return;
    distance = gweather_location_get_distance (location, parent);

    if (distance > TOO_FAR) {
        g_print ("Airport '%s' is too far from city '%s' (%.1lf km)\n",
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
    g_autoptr(GWeatherLocation) world = NULL;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    test_airport_distance_children (world);

    if (g_test_failed ())
        g_warning ("Maximum city to airport distance is %.1f km", max_distance);

    g_clear_pointer (&world, gweather_location_unref);
    _gweather_location_reset_world ();
}

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

        if (g_hash_table_lookup (stations_ht, station)) {
            const char * const known_duplicates[] = {
                "VOGO",
                "KHQG",
                "KOEL",
                "KTQK",
                "KX26",
                NULL
            };
            if (g_strv_contains (known_duplicates, station)) {
                g_free (station);
                continue;
            }
            g_print ("Weather station '%s' already defined\n", station);
        }

        g_hash_table_insert (stations_ht, station, g_strdup (line));
        num_stations++;
    }

    g_strfreev (lines);

    /* Duplicates? */
    g_assert_cmpuint (num_stations, ==, g_hash_table_size (stations_ht));

    g_print ("Parsed %u weather stations\n", num_stations);

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
        g_print ("Could not find airport for '%s' in " METAR_SOURCES "\n", code);
        g_test_fail ();
    } else {
        char *has_metar;

        has_metar = g_strndup (line + 62, 1);
        if (*has_metar == 'Z') {
            g_print ("Airport weather station '%s' is obsolete\n", code);
            g_test_fail ();
        } else if (*has_metar == ' ') {
            g_print ("Could not find weather station for '%s' in " METAR_SOURCES "\n", code);
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
    g_autoptr(GWeatherLocation) world = NULL;
    SoupMessage *msg;
    SoupSession *session;
    GHashTable *stations_ht;
    char *contents;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    msg = soup_message_new ("GET", METAR_SOURCES);
    session = soup_session_new ();
    soup_session_send_message (session, msg);
    if (msg->status_code == SOUP_STATUS_SSL_FAILED) {
        g_test_message ("SSL/TLS failure, please check your glib-networking installation");
        g_test_failed ();
        return;
    }
    g_assert_cmpint (msg->status_code, >=, 200);
    g_assert_cmpint (msg->status_code, <, 300);
    g_object_unref (session);
    g_assert_nonnull (msg->response_body);

    contents = g_strndup (msg->response_body->data, msg->response_body->length);
    g_object_unref (msg);

    stations_ht = parse_metar_stations (contents);
    g_assert_nonnull (stations_ht);
    g_free (contents);

    test_metar_weather_stations_children (world, stations_ht);

    g_hash_table_unref (stations_ht);

    g_clear_pointer (&world, gweather_location_unref);
    _gweather_location_reset_world ();
}

static void
set_gsettings (void)
{
	char *tmpdir, *schema_text, *dest, *cmdline;
	int result;

	/* Create the installed schemas directory */
	tmpdir = g_dir_make_tmp ("libgweather-test-XXXXXX", NULL);
	g_assert_nonnull (tmpdir);

	/* Copy the schemas files */
	g_assert_true (g_file_get_contents (SCHEMAS_BUILDDIR "/org.gnome.GWeather.enums.xml", &schema_text, NULL, NULL));
	dest = g_strdup_printf ("%s/org.gnome.GWeather.enums.xml", tmpdir);
	g_assert_true (g_file_set_contents (dest, schema_text, -1, NULL));
	g_free (dest);
	g_free (schema_text);

	g_assert_true (g_file_get_contents (SCHEMASDIR "/org.gnome.GWeather.gschema.xml", &schema_text, NULL, NULL));
	dest = g_strdup_printf ("%s/org.gnome.GWeather.gschema.xml", tmpdir);
	g_assert_true (g_file_set_contents (dest, schema_text, -1, NULL));
	g_free (dest);
	g_free (schema_text);

	/* Compile the schemas */
	cmdline = g_strdup_printf ("glib-compile-schemas --targetdir=%s "
				   "--schema-file=%s/org.gnome.GWeather.enums.xml "
				   "--schema-file=%s/org.gnome.GWeather.gschema.xml",
				   tmpdir, SCHEMAS_BUILDDIR, SCHEMASDIR);
	g_assert_true (g_spawn_command_line_sync (cmdline, NULL, NULL, &result, NULL));
	g_assert_cmpint (result, ==, 0);
	g_free (cmdline);

	/* Set envvar */
	g_setenv ("GSETTINGS_SCHEMA_DIR", tmpdir, TRUE);
	g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

	g_free (tmpdir);
}

static void
test_utc_sunset (void)
{
	g_autoptr(GWeatherLocation) world = NULL;
	g_autoptr(GWeatherLocation) utc = NULL;
	GWeatherInfo *info;
	char *sunset;
	GWeatherMoonPhase phase;
	GWeatherMoonLatitude lat;
	gboolean ret;

	world = gweather_location_get_world ();
	g_assert_nonnull (world);
	utc = gweather_location_find_by_station_code (world, "@UTC");
	g_assert_nonnull (utc);

	info = gweather_info_new (utc);
	gweather_info_set_enabled_providers (info, GWEATHER_PROVIDER_NONE);
	gweather_info_update (info);

	sunset = gweather_info_get_sunset (info);
	g_assert_nonnull (sunset);
	g_free (sunset);

	ret = gweather_info_get_value_moonphase (info, &phase, &lat);
	g_assert_false (ret);

	g_object_unref (info);

	g_clear_pointer (&world, gweather_location_unref);
	g_clear_pointer (&utc, gweather_location_unref);
	_gweather_location_reset_world ();
}

static void
check_bad_duplicate_weather_stations (gpointer key,
                                      gpointer value,
                                      gpointer user_data)
{
    GPtrArray *stations = value;
    GHashTable *dedup;
    guint i;

    if (stations->len == 1)
        goto out;

    dedup = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    for (i = 0; i < stations->len; i++) {
        double latitude, longitude;

        gweather_location_get_coords (g_ptr_array_index (stations, i),
                                      &latitude, &longitude);
        g_hash_table_insert (dedup,
                             g_strdup_printf ("%.10lf %.10lf", latitude, longitude),
                             GUINT_TO_POINTER (1));
    }

    if (g_hash_table_size (dedup) > 1) {
        g_print ("Airport '%s' is defined %u times in different ways\n",
                 (const char *) key, stations->len);
        g_test_fail ();
    }

    g_hash_table_destroy (dedup);

out:
    g_ptr_array_free (stations, TRUE);
}

static void
test_bad_duplicate_weather_stations_children (GWeatherLocation *location,
                                              GHashTable       *stations_ht)
{
    GWeatherLocation **children;
    guint i;

    children = gweather_location_get_children (location);
    for (i = 0; children[i] != NULL; i++) {
        if (gweather_location_get_level (children[i]) == GWEATHER_LOCATION_WEATHER_STATION) {
            GPtrArray *stations;
            const char *code;

            code = gweather_location_get_code (children[i]);

            stations = g_hash_table_lookup (stations_ht, code);
            if (!stations)
                stations = g_ptr_array_new ();
            g_ptr_array_add (stations, children[i]);
            g_hash_table_insert (stations_ht, g_strdup (code), stations);
        } else {
            test_bad_duplicate_weather_stations_children (children[i], stations_ht);
        }
    }
}

static void
test_bad_duplicate_weather_stations (void)
{
    g_autoptr(GWeatherLocation) world = NULL;
    GHashTable *stations_ht;

    g_setenv ("LIBGWEATHER_LOCATIONS_NO_NEAREST", "1", TRUE);
    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    stations_ht = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, (GDestroyNotify) NULL);
    test_bad_duplicate_weather_stations_children (world, stations_ht);

    g_hash_table_foreach (stations_ht, check_bad_duplicate_weather_stations, NULL);

    g_hash_table_unref (stations_ht);

    g_unsetenv ("LIBGWEATHER_LOCATIONS_NO_NEAREST");
    g_clear_pointer (&world, gweather_location_unref);
    _gweather_location_reset_world ();
}

static void
test_duplicate_weather_stations_children (GWeatherLocation *location)
{
    GWeatherLocation **children;
    GHashTable *stations_ht = NULL;
    guint i;

    children = gweather_location_get_children (location);
    for (i = 0; children[i] != NULL; i++) {
        if (gweather_location_get_level (children[i]) == GWEATHER_LOCATION_WEATHER_STATION) {
            const char *code;

            code = gweather_location_get_code (children[i]);
            if (!stations_ht) {
                stations_ht = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     g_free, (GDestroyNotify) NULL);
            } else {
                gboolean exists;

                exists = GPOINTER_TO_INT (g_hash_table_lookup (stations_ht, code));
                if (exists) {
                    GWeatherLocationLevel parent_level;

                    parent_level = gweather_location_get_level (location);
                    g_print ("Duplicate weather station '%s' in %s (level '%s')\n",
                             code, gweather_location_get_name (location),
                             gweather_location_level_to_string (parent_level));
                    g_test_fail ();
                }
            }

            g_hash_table_insert (stations_ht, g_strdup (code), GINT_TO_POINTER (1));
        } else {
            test_duplicate_weather_stations_children (children[i]);
        }
    }

    if (stations_ht)
        g_hash_table_destroy (stations_ht);
}

static void
test_duplicate_weather_stations (void)
{
    g_autoptr(GWeatherLocation) world = NULL;

    g_setenv ("LIBGWEATHER_LOCATIONS_NO_NEAREST", "1", TRUE);
    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    test_duplicate_weather_stations_children (world);

    g_unsetenv ("LIBGWEATHER_LOCATIONS_NO_NEAREST");
    g_clear_pointer (&world, gweather_location_unref);
    _gweather_location_reset_world ();
}

static void
test_location_names (void)
{
    g_autoptr(GWeatherLocation) world = NULL;
    g_autoptr(GWeatherLocation) brussels = NULL;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    brussels = gweather_location_find_nearest_city (world, 50.833333, 4.333333);
    g_assert_nonnull (brussels);
    g_assert_cmpstr (gweather_location_get_name (brussels), ==, "Brussels");
    g_assert_cmpstr (gweather_location_get_sort_name (brussels), ==, "brussels");
    g_assert_cmpstr (gweather_location_get_english_name (brussels), ==, "Brussels");
    gweather_location_unref (brussels);

    setlocale (LC_ALL, "fr_FR.UTF-8");

    g_clear_pointer (&world, gweather_location_unref);
    g_clear_pointer (&brussels, gweather_location_unref);
    _gweather_location_reset_world ();

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    brussels = gweather_location_find_nearest_city (world, 50.833333, 4.333333);
    g_assert_nonnull (brussels);
    g_assert_cmpstr (gweather_location_get_name (brussels), ==, "Bruxelles");
    g_assert_cmpstr (gweather_location_get_sort_name (brussels), ==, "bruxelles");
    g_assert_cmpstr (gweather_location_get_english_name (brussels), ==, "Brussels");
    gweather_location_unref (brussels);

    setlocale (LC_ALL, "");
    g_clear_pointer (&world, gweather_location_unref);
    g_clear_pointer (&brussels, gweather_location_unref);
    _gweather_location_reset_world ();
}

static gboolean
find_loc_children (GWeatherLocation  *location,
		   const char        *search_str,
		   GWeatherLocation **ret)
{
    GWeatherLocation **children;
    guint i;

    children = gweather_location_get_children (location);
    for (i = 0; children[i] != NULL; i++) {
        if (gweather_location_get_level (children[i]) == GWEATHER_LOCATION_WEATHER_STATION) {
            const char *code;

            code = gweather_location_get_code (children[i]);
            if (g_strcmp0 (search_str, code) == 0) {
                *ret = gweather_location_ref (children[i]);
                return TRUE;
            }
        } else {
            if (find_loc_children (children[i], search_str, ret))
                return TRUE;
        }
    }

    return FALSE;
}

static GWeatherLocation *
find_loc (GWeatherLocation *world,
	  const char       *search_str)
{
    GWeatherLocation *loc = NULL;

    find_loc_children (world, search_str, &loc);
    return loc;
}

static void
weather_updated (GWeatherInfo *info,
                 GMainLoop    *loop)
{
    g_assert_not_reached ();
}

static gboolean
stop_loop_cb (gpointer user_data)
{
	g_main_loop_quit (user_data);
	return G_SOURCE_REMOVE;
}

static void
test_weather_loop_use_after_free (void)
{
    GMainLoop *loop;
    g_autoptr(GWeatherLocation) world = NULL;
    GWeatherLocation *loc;
    GWeatherInfo *info;
    const char *search_str = "LFLL";

    world = gweather_location_get_world ();
    loc = find_loc (world, search_str);

    if (!loc) {
        g_message ("Could not find station for %s", search_str);
        g_test_failed ();
        return;
    }

    g_message ("Found station %s for '%s'", gweather_location_get_name (loc), search_str);

    loop = g_main_loop_new (NULL, TRUE);
    info = gweather_info_new (NULL);
    gweather_info_set_application_id (info, "org.gnome.LibGWeather");
    gweather_info_set_contact_info (info, "https://gitlab.gnome.org/GNOME/libgweather/");
    gweather_info_set_enabled_providers (info,
					 GWEATHER_PROVIDER_METAR |
					 GWEATHER_PROVIDER_IWIN |
					 GWEATHER_PROVIDER_YAHOO |
					 GWEATHER_PROVIDER_MET_NO |
					 GWEATHER_PROVIDER_OWM);
    gweather_info_set_location (info, loc);
    g_signal_connect (G_OBJECT (info), "updated",
                      G_CALLBACK (weather_updated), loop);
    gweather_info_update (info);
    g_object_unref (info);

    gweather_location_unref (loc);

    g_timeout_add_seconds (5, stop_loop_cb, loop);
    g_main_loop_run (loop);
    g_main_loop_unref (loop);
}

static void
test_walk_world (void)
{
    g_autoptr(GWeatherLocation) cur = NULL, next = NULL;
    gint visited = 0;

    next = gweather_location_get_world ();
    while (next) {
	/* Update cur pointer. */
	g_clear_pointer (&cur, gweather_location_unref);
	cur = g_steal_pointer (&next);
	visited += 1;
	g_assert_cmpint (cur->ref_count, ==, 1);

	/* Select next item, which is in this order:
	 *  1. The first child
	 *  2. Walk up the parent tree and try to find a sibbling
	 * Note that cur remains valid after the loop and points to the world
	 * again.
	 */
	if ((next = gweather_location_next_child (cur, NULL)))
	    continue;

	while (TRUE) {
	    g_autoptr(GWeatherLocation) child = NULL;
	    /* Move cur to the parent, keeping the child as reference. */
	    child = g_steal_pointer (&cur);
	    cur = gweather_location_get_parent (child);
	    if (!cur)
		break;
	    g_assert_cmpint (cur->ref_count, ==, 1);
	    g_assert_cmpint (child->ref_count, ==, 1);

	    if ((next = gweather_location_next_child (cur, gweather_location_ref (child))))
		break;
	}
    }

    /* cur must be NULL at this point */
    g_assert_null (cur);

    /* Check that we visited a reasonable number of nodes.
     * Due to implicit nearest nodes, this needs to be more than the number
     * of DB entries. */
    cur = gweather_location_get_world ();
    g_assert_cmpint (visited, >, cur->db->locations->len);
    g_clear_pointer (&cur, gweather_location_unref);

    /* noop, but asserts we did not leak */
    _gweather_location_reset_world ();
}

static void
test_radians_to_degrees_str (void)
{
    char long_version[G_ASCII_DTOSTR_BUF_SIZE];
    g_autofree char *short_version = NULL;
    double coord = 1.260765526077;

    g_ascii_dtostr (long_version, G_ASCII_DTOSTR_BUF_SIZE, RADIANS_TO_DEGREES (coord));
    short_version = _radians_to_degrees_str (coord);

    g_assert_cmpint (strlen (long_version), >, strlen (short_version));
    g_assert_cmpstr (short_version, ==, "72.2365");
}

static void
log_handler (const char *log_domain, GLogLevelFlags log_level, const char *message, gpointer user_data)
{
	g_print ("%s\n", message);
}

int
main (int argc, char *argv[])
{
	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("http://gitlab.gnome.org/GNOME/libgweather/issues/");

	/* We need to handle log messages produced by g_message so they're interpreted correctly by the GTester framework */
	g_log_set_handler (NULL, G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG, log_handler, NULL);

	g_setenv ("LIBGWEATHER_LOCATIONS_PATH",
		  TEST_LOCATIONS,
		  FALSE);
	set_gsettings ();

	g_test_add_func ("/weather/radians-to-degrees_str", test_radians_to_degrees_str);
	g_test_add_func ("/weather/named-timezones", test_named_timezones);
	g_test_add_func ("/weather/named-timezones-deserialized", test_named_timezones_deserialized);
	g_test_add_func ("/weather/no-code-serialize", test_no_code_serialize);
	g_test_add_func ("/weather/timezones", test_timezones);
	g_test_add_func ("/weather/airport_distance_sanity", test_airport_distance_sanity);
	g_test_add_func ("/weather/metar_weather_stations", test_metar_weather_stations);
	g_test_add_func ("/weather/utc_sunset", test_utc_sunset);
	g_test_add_func ("/weather/weather-loop-use-after-free", test_weather_loop_use_after_free);
	/* Modifies environment, so needs to run last */
	g_test_add_func ("/weather/bad_duplicate_weather_stations", test_bad_duplicate_weather_stations);
	g_test_add_func ("/weather/duplicate_weather_stations", test_duplicate_weather_stations);
	g_test_add_func ("/weather/location-names", test_location_names);
	g_test_add_func ("/weather/walk_world", test_walk_world);

	return g_test_run ();
}
