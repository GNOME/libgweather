/* test_libgweather.c: Test suite for libgweather
 *
 * SPDX-FileCopyrightText: 2017 Bastien Nocera <hadess@hadess.net>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "config.h"

#include "gweather-test-utils.h"

#include "gweather-private.h"

/* Maximum for test_airport_distance_sanity() */
#define TOO_FAR 100.0
static double max_distance = 0.0;

static void
test_no_code_serialize (void)
{
    GVariant *variant;
    g_autoptr (GWeatherLocation) world = NULL;
    g_autoptr (GWeatherLocation) loc = NULL;
    g_autoptr (GWeatherLocation) new_loc = NULL;
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
    g_test_message ("variant: %s", str->str);
    g_string_free (str, TRUE);

    new_loc = gweather_location_deserialize (world, variant);
    g_variant_unref (variant);
    g_assert_nonnull (new_loc);
    g_assert_cmpstr (gweather_location_get_name (loc), ==, gweather_location_get_name (new_loc));
    g_assert_true (gweather_location_equal (loc, new_loc));

    g_clear_object (&world);
    g_clear_object (&loc);
    g_clear_pointer (&new_loc, g_object_unref);

    gweather_test_reset_world ();
}

static void
test_distance (GWeatherLocation *location)
{
    g_autoptr (GWeatherLocation) parent = NULL;
    double distance;

    parent = gweather_location_get_parent (location);
    if (gweather_location_get_level (parent) < GWEATHER_LOCATION_CITY)
        return;
    distance = gweather_location_get_distance (location, parent);

    if (distance > TOO_FAR) {
        g_test_message ("Airport '%s' is too far from city '%s' (%.1lf km)\n",
                        gweather_location_get_name (location),
                        gweather_location_get_name (parent),
                        distance);
        max_distance = MAX (max_distance, distance);
        g_test_fail ();
    }
}

static void
test_airport_distance_children (GWeatherLocation *location)
{
    g_autoptr (GWeatherLocation) child = NULL;

    while ((child = gweather_location_next_child (location, child)) != NULL) {
        if (gweather_location_get_level (child) == GWEATHER_LOCATION_WEATHER_STATION)
            test_distance (child);
        else
            test_airport_distance_children (child);
    }
}

static void
test_airport_distance_sanity (void)
{
    g_autoptr (GWeatherLocation) world = NULL;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    test_airport_distance_children (world);

    g_test_message ("Maximum airport distance: %g", max_distance);

    g_clear_object (&world);
    gweather_test_reset_world ();
}

static void
test_utc_sunset (void)
{
    g_autoptr (GWeatherLocation) world = NULL;
    g_autoptr (GWeatherLocation) utc = NULL;
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

    g_clear_object (&world);
    g_clear_object (&utc);

    gweather_test_reset_world ();
}

static void
test_location_names (void)
{
    g_autoptr (GWeatherLocation) world = NULL;
    g_autoptr (GWeatherLocation) brussels = NULL;
    char *old_locale;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    brussels = gweather_location_find_nearest_city (world, 50.833333, 4.333333);
    g_assert_nonnull (brussels);
    g_assert_cmpstr (gweather_location_get_name (brussels), ==, "Brussels");
    g_assert_cmpstr (gweather_location_get_sort_name (brussels), ==, "brussels");
    g_assert_cmpstr (gweather_location_get_english_name (brussels), ==, "Brussels");

    old_locale = setlocale (LC_ALL, "fr_FR.UTF-8");
    if (old_locale == NULL) {
        g_test_skip ("Locale fr_FR.UTF-8 is not available");
        return;
    }

    g_clear_object (&world);
    g_clear_object (&brussels);
    gweather_test_reset_world ();

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    brussels = gweather_location_find_nearest_city (world, 50.833333, 4.333333);
    g_assert_nonnull (brussels);
    g_assert_cmpstr (gweather_location_get_name (brussels), ==, "Bruxelles");
    g_assert_cmpstr (gweather_location_get_sort_name (brussels), ==, "bruxelles");
    g_assert_cmpstr (gweather_location_get_english_name (brussels), ==, "Brussels");
    g_clear_object (&brussels);

    g_clear_object (&world);

    setlocale (LC_ALL, old_locale);
    gweather_test_reset_world ();
}

static gboolean
find_loc_children (GWeatherLocation *location,
                   const char *search_str,
                   GWeatherLocation **ret)
{
    g_autoptr (GWeatherLocation) child = NULL;
    while ((child = gweather_location_next_child (location, child)) != NULL) {
        if (gweather_location_get_level (child) == GWEATHER_LOCATION_WEATHER_STATION) {
            const char *code;

            code = gweather_location_get_code (child);
            if (g_strcmp0 (search_str, code) == 0) {
                *ret = g_object_ref (child);
                return TRUE;
            }
        } else {
            if (find_loc_children (child, search_str, ret))
                return TRUE;
        }
    }

    return FALSE;
}

static GWeatherLocation *
find_loc (GWeatherLocation *world,
          const char *search_str)
{
    GWeatherLocation *loc = NULL;

    find_loc_children (world, search_str, &loc);
    return loc;
}

static void
weather_updated (GWeatherInfo *info,
                 GMainLoop *loop)
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
    g_autoptr (GWeatherLocation) world = NULL;
    GWeatherLocation *loc;
    GWeatherInfo *info;
    const char *search_str = "LFLL";

    world = gweather_location_get_world ();
    loc = find_loc (world, search_str);

    if (!loc) {
        g_test_message ("Could not find station for %s", search_str);
        g_test_failed ();
        return;
    }

    g_test_message ("Found station %s for '%s'", gweather_location_get_name (loc), search_str);

    loop = g_main_loop_new (NULL, TRUE);
    info = gweather_info_new (NULL);
    gweather_info_set_application_id (info, "org.gnome.LibGWeather");
    gweather_info_set_contact_info (info, "https://gitlab.gnome.org/GNOME/libgweather/");
    gweather_info_set_enabled_providers (info,
                                         GWEATHER_PROVIDER_METAR |
                                             GWEATHER_PROVIDER_IWIN |
                                             GWEATHER_PROVIDER_MET_NO |
                                             GWEATHER_PROVIDER_OWM);
    gweather_info_set_location (info, loc);
    g_signal_connect (G_OBJECT (info), "updated", G_CALLBACK (weather_updated), loop);
    gweather_info_update (info);
    g_object_unref (info);

    g_object_unref (loc);

    g_timeout_add_seconds (5, stop_loop_cb, loop);
    g_main_loop_run (loop);
    g_main_loop_unref (loop);
}

static void
test_walk_world (void)
{
    g_autoptr (GWeatherLocation) cur = NULL, next = NULL;
    gint visited = 0;

    next = gweather_location_get_world ();
    while (next) {
        /* Update cur pointer. */
        g_clear_object (&cur);

        cur = g_steal_pointer (&next);
        visited += 1;

        /* Select next item, which is in this order:
	     *  1. The first child
	     *  2. Walk up the parent tree and try to find a sibbling
	     * Note that cur remains valid after the loop and points to the world
	     * again.
	     */
        if ((next = gweather_location_next_child (cur, NULL)))
            continue;

        while (TRUE) {
            g_autoptr (GWeatherLocation) child = NULL;

            /* Move cur to the parent, keeping the child as reference. */
            child = g_steal_pointer (&cur);
            cur = gweather_location_get_parent (child);
            if (!cur)
                break;

            if ((next = gweather_location_next_child (cur, g_object_ref (child))))
                break;
        }
    }

    /* cur must be NULL at this point */
    g_assert_null (cur);

    /* Check that we visited a reasonable number of nodes.
     * Due to implicit nearest nodes, this needs to be more than the number
     * of DB entries. */
    GWeatherDb *db = gweather_get_db ();
    g_assert_cmpint (visited, >, db->locations->len);

    /* noop, but asserts we did not leak */
    gweather_test_reset_world ();
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

int
main (int argc, char *argv[])
{
    setlocale (LC_ALL, "");

    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("http://gitlab.gnome.org/GNOME/libgweather/issues/");

    g_autofree char *schemas_dir = gweather_test_setup_gsettings ();

    g_test_add_func ("/weather/radians-to-degrees_str", test_radians_to_degrees_str);
    g_test_add_func ("/weather/no-code-serialize", test_no_code_serialize);
    g_test_add_func ("/weather/airport_distance_sanity", test_airport_distance_sanity);
    g_test_add_func ("/weather/utc_sunset", test_utc_sunset);
    g_test_add_func ("/weather/weather-loop-use-after-free", test_weather_loop_use_after_free);
    g_test_add_func ("/weather/location-names", test_location_names);
    g_test_add_func ("/weather/walk_world", test_walk_world);

    int res = g_test_run ();

    gweather_test_teardown_gsettings (schemas_dir);

    return res;
}
