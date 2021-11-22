/* timezones.c: Time zone tests
 *
 * SPDX-FileCopyrightText: 2017 Bastien Nocera <hadess@hadess.net>
 * SPDX-FileCopyrightText: 2021 Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "config.h"

#include "gweather-test-utils.h"

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

static void
test_named_timezones (void)
{
    g_autoptr (GWeatherLocation) world = NULL;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    g_autoptr (GWeatherLocation) child = NULL;
    while ((child = gweather_location_next_child (world, child)) != NULL) {
        GWeatherLocationLevel level;
        const char *code;

        level = gweather_location_get_level (child);
        if (level != GWEATHER_LOCATION_NAMED_TIMEZONE)
            continue;

        /* Verify that timezone codes start with a '@' */
        code = gweather_location_get_code (child);
        g_assert_nonnull (code);
        g_assert_true (code[0] == '@');
    }

    g_clear_object (&world);
    gweather_test_reset_world ();
}

#define CONFIGURATION                                                                                                                                                   \
    "[ "                                                                                                                                                                \
    "{'location': <(uint32 2, <('Rio de Janeiro', 'SBES', false, [(-0.39822596348113698, -0.73478361508961265)], [(-0.39822596348113698, -0.73478361508961265)])>)>}, " \
    "{'location': <(uint32 2, <('Coordinated Universal Time (UTC)', '@UTC', false, @a(dd) [], @a(dd) [])>)>}, "                                                         \
    "{'location': <(uint32 2, <('Perm', 'USPP', true, [(1.0122909661567112, 0.98174770424681035)], [(1.0122909661567112, 0.98174770424681035)])>)>} "                   \
    "]"

static void
test_timezones (void);

static void
test_named_timezones_deserialized (void)
{
    g_autoptr (GWeatherLocation) world = NULL;
    GList *list, *l;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    list = get_list_from_configuration (world, CONFIGURATION, 3);
    g_list_free_full (list, g_object_unref);

    list = get_list_from_configuration (world, CONFIGURATION, 3);
    for (l = list; l != NULL; l = l->next) {
        GWeatherLocation *loc = l->data;
        GTimeZone *tz;
        const char *tzid;

        tz = gweather_location_get_timezone (loc);
        g_assert_nonnull (tz);

        tzid = g_time_zone_get_identifier (tz);
        g_assert_nonnull (tzid);

        g_object_unref (loc);
    }
    g_list_free (list);

    g_clear_object (&world);

    /* test_timezones will clear the DB */
    test_timezones ();
}

static void
test_timezone (GWeatherLocation *location)
{
    g_autoptr (GTimeZone) gtz = NULL;
    const char *tz;

    tz = gweather_location_get_timezone_str (location);
    if (!tz) {
        GTimeZone **tzs;

        tzs = gweather_location_get_timezones (location);
        g_assert_nonnull (tzs);

        /* Only countries should have multiple timezones associated */
        if ((tzs[0] == NULL && gweather_location_get_level (location) < GWEATHER_LOCATION_WEATHER_STATION) &&
            gweather_location_get_level (location) >= GWEATHER_LOCATION_COUNTRY) {
            g_test_message ("Location '%s' does not have an associated timezone\n",
                            gweather_location_get_name (location));
            g_test_fail ();
        }
        gweather_location_free_timezones (location, tzs);
        return;
    }

    gtz = g_time_zone_new_identifier (tz);
    if (gtz == NULL) {
        g_test_message ("Location '%s' has invalid timezone '%s'\n",
                        gweather_location_get_name (location),
                        tz);
        g_test_fail ();
    }
}

static void
test_timezones_children (GWeatherLocation *location)
{
    g_autoptr (GWeatherLocation) child = NULL;
    while ((child = gweather_location_next_child (location, child)) != NULL) {
        if (gweather_location_get_level (child) >= GWEATHER_LOCATION_COUNTRY)
            test_timezone (child);

        test_timezones_children (child);
    }
}

static void
test_timezones (void)
{
    g_autoptr (GWeatherLocation) world = NULL;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    test_timezones_children (world);

    g_clear_object (&world);

    gweather_test_reset_world ();
}

int
main (int argc,
      char *argv[])
{
    setlocale (LC_ALL, "");

    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("http://gitlab.gnome.org/GNOME/libgweather/issues/");

    g_autofree char *schemas_dir = gweather_test_setup_gsettings ();

    g_test_add_func ("/weather/named-timezones", test_named_timezones);
    g_test_add_func ("/weather/named-timezones-deserialized", test_named_timezones_deserialized);
    g_test_add_func ("/weather/timezones", test_timezones);

    int res = g_test_run ();

    gweather_test_teardown_gsettings (schemas_dir);

    return res;
}
