/* duplicates.c: Check for duplicates in the locations database
 *
 * SPDX-FileCopyrightText: 2017 Bastien Nocera <hadess@hadess.net>
 * SPDX-FileCopyrightText: 2021 Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <locale.h>
#include <string.h>

#include <libgweather/gweather-version.h>

/* We use internal API */
#include "gweather-private.h"

extern void
_gweather_location_reset_world (void);

/* Set up the temporary directory with the GSettings schemas */
static char *
setup_gsettings (void)
{
    char *tmpdir, *schema_text, *dest, *cmdline;
    int result;

    /* Create the installed schemas directory */
    GError *error = NULL;
    tmpdir = g_dir_make_tmp ("libgweather-test-XXXXXX", &error);
    g_assert_no_error (error);

    g_test_message ("Using temporary directory: %s", tmpdir);

    /* Copy the schemas files */
    g_assert_true (g_file_get_contents (SCHEMAS_BUILDDIR "/org.gnome.GWeather4.enums.xml", &schema_text, NULL, NULL));
    dest = g_build_filename (tmpdir, "org.gnome.GWeather4.enums.xml", NULL);
    g_assert_true (g_file_set_contents (dest, schema_text, -1, NULL));
    g_free (dest);
    g_free (schema_text);

    g_assert_true (g_file_get_contents (SCHEMASDIR "/org.gnome.GWeather4.gschema.xml", &schema_text, NULL, NULL));
    dest = g_build_filename (tmpdir, "org.gnome.GWeather4.gschema.xml", NULL);
    g_assert_true (g_file_set_contents (dest, schema_text, -1, NULL));
    g_free (dest);
    g_free (schema_text);

    /* Compile the schemas */
    cmdline = g_strdup_printf ("glib-compile-schemas --targetdir=%s "
                               "--schema-file=%s/org.gnome.GWeather4.enums.xml "
                               "--schema-file=%s/org.gnome.GWeather4.gschema.xml",
                               tmpdir,
                               SCHEMAS_BUILDDIR,
                               SCHEMASDIR);
    g_assert_true (g_spawn_command_line_sync (cmdline, NULL, NULL, &result, NULL));
    g_assert_cmpint (result, ==, 0);
    g_free (cmdline);

    /* Set envvar */
    g_setenv ("GSETTINGS_SCHEMA_DIR", tmpdir, TRUE);

    return tmpdir;
}

/* Tear down the temporary directory with the GSettings schemas */
static void
teardown_gsettings (const char *schemas_dir)
{
    char *dest = NULL;

    dest = g_build_filename (schemas_dir, "org.gnome.GWeather4.enums.xml", NULL);
    g_assert_no_errno (g_unlink (dest));
    g_free (dest);

    dest = g_build_filename (schemas_dir, "org.gnome.GWeather4.gschema.xml", NULL);
    g_assert_no_errno (g_unlink (dest));
    g_free (dest);

    dest = g_build_filename (schemas_dir, "gschemas.compiled", NULL);
    g_assert_no_errno (g_unlink (dest));
    g_free (dest);

    g_assert_no_errno (g_rmdir (schemas_dir));
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
        GWeatherLocation *location = g_ptr_array_index (stations, i);

        double latitude, longitude;
        gweather_location_get_coords (location, &latitude, &longitude);

        char *coords = g_strdup_printf ("%.10lf %.10lf", latitude, longitude);
        g_hash_table_insert (dedup, coords, GUINT_TO_POINTER (1));
    }

    if (g_hash_table_size (dedup) > 1) {
        g_test_message ("Airport '%s' is defined %u times in different ways\n",
                        (const char *) key,
                        stations->len);
        g_test_fail ();
    }

    g_hash_table_destroy (dedup);

out:
    g_ptr_array_free (stations, TRUE);
}

static void
test_bad_duplicate_weather_stations_children (GWeatherLocation *location,
                                              GHashTable *stations_ht)
{
    g_autoptr (GWeatherLocation) child = NULL;
    while ((child = gweather_location_next_child (location, child)) != NULL) {
        if (gweather_location_get_level (child) == GWEATHER_LOCATION_WEATHER_STATION) {
            GPtrArray *stations;
            const char *code;

            code = gweather_location_get_code (child);

            stations = g_hash_table_lookup (stations_ht, code);
            if (!stations) {
                stations = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
                g_hash_table_insert (stations_ht, g_strdup (code), stations);
            }
            g_ptr_array_add (stations, g_object_ref (child));
        } else {
            test_bad_duplicate_weather_stations_children (child, stations_ht);
        }
    }
}

static void
test_bad_duplicate_weather_stations (void)
{
    g_autoptr (GWeatherLocation) world = NULL;
    GHashTable *stations_ht;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    stations_ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) NULL);
    test_bad_duplicate_weather_stations_children (world, stations_ht);

    g_hash_table_foreach (stations_ht, check_bad_duplicate_weather_stations, NULL);

    g_hash_table_unref (stations_ht);

    g_clear_object (&world);
    _gweather_location_reset_world ();
}

static void
test_duplicate_weather_stations_children (GWeatherLocation *location)
{
    g_autoptr (GHashTable) stations_ht = NULL;

    g_autoptr (GWeatherLocation) child = NULL;
    while ((child = gweather_location_next_child (location, child)) != NULL) {
        if (gweather_location_get_level (child) == GWEATHER_LOCATION_WEATHER_STATION) {
            const char *code;

            code = gweather_location_get_code (child);
            if (stations_ht == NULL) {
                stations_ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) NULL);
            } else {
                gboolean exists;

                exists = GPOINTER_TO_INT (g_hash_table_lookup (stations_ht, code));
                if (exists) {
                    GWeatherLocationLevel parent_level;

                    parent_level = gweather_location_get_level (location);
                    g_test_message ("Duplicate weather station '%s' in %s (level '%s')\n",
                                    code,
                                    gweather_location_get_name (location),
                                    gweather_location_level_to_string (parent_level));
                    g_test_fail ();
                    return;
                }
            }

            g_hash_table_insert (stations_ht, g_strdup (code), GINT_TO_POINTER (1));
        } else {
            test_duplicate_weather_stations_children (child);
        }
    }
}

static void
test_duplicate_weather_stations (void)
{
    g_autoptr (GWeatherLocation) world = NULL;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    test_duplicate_weather_stations_children (world);

    g_clear_object (&world);
    _gweather_location_reset_world ();
}

int
main (int argc,
      char *argv[])
{
    setlocale (LC_ALL, "");

    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("http://gitlab.gnome.org/GNOME/libgweather/issues/");

    char *schemas_dir = setup_gsettings ();

    /* Modifies environment, so needs to run last */
    g_test_add_func ("/weather/bad_duplicate_weather_stations", test_bad_duplicate_weather_stations);
    g_test_add_func ("/weather/duplicate_weather_stations", test_duplicate_weather_stations);

    int res = g_test_run ();

    teardown_gsettings (schemas_dir);

    g_free (schemas_dir);

    return res;
}
