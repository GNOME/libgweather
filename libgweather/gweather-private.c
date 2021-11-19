/* gweather-private.c - Overall weather server functions
 *
 * SPDX-FileCopyrightText: The GWeather authors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "gweather-private.h"

/* sign, 3 digits, separator, 4 decimals, nul-char */
#define DEGREES_STR_SIZE (1 + 3 + 1 + 4 + 1)

static GWeatherDb *world_db;

void
_gweather_location_reset_world (void)
{
    gsize i;

    g_return_if_fail (world_db != NULL);

    /* At this point, we had a leak if the caches are not completely empty. */
    for (i = 0; i < world_db->locations->len; i++) {
        if (G_UNLIKELY (g_ptr_array_index (world_db->locations, i) != NULL)) {
            g_warning ("Location with index %li and name %s is still referenced!",
                       i,
                       gweather_location_get_name (g_ptr_array_index (world_db->locations, i)));
            g_assert_not_reached ();
        }
    }
    for (i = 0; i < world_db->timezones->len; i++) {
        if (G_UNLIKELY (g_ptr_array_index (world_db->timezones, i) != NULL)) {
            g_warning ("Timezone with index %li and tzid %s is still referenced!",
                       i,
                       gweather_timezone_get_tzid (g_ptr_array_index (world_db->timezones, i)));
            g_assert_not_reached ();
        }
    }
}

static gpointer
ensure_world (gpointer dummy G_GNUC_UNUSED)
{
    g_autoptr (GError) error = NULL;
    g_autofree char *filename = NULL;
    g_autoptr (GMappedFile) map;
    const char *locations_path;
    time_t now;
    struct tm tm;
    GWeatherDb *db = NULL;

    locations_path = g_getenv ("LIBGWEATHER_LOCATIONS_PATH");
    if (locations_path != NULL) {
        filename = g_strdup (locations_path);
        if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
            g_warning ("User specified database %s does not exist", filename);
            g_clear_pointer (&filename, g_free);
        }
    }

    if (filename == NULL) {
        filename = g_build_filename (GWEATHER_BIN_LOCATION_DIR, "Locations.bin", NULL);
    }

    map = g_mapped_file_new (filename, FALSE, &error);
    if (map == NULL) {
        g_critical ("Failed to open database %s: %s", filename, error->message);
        return NULL;
    }

    db = g_new0 (GWeatherDb, 1);
    db->world = db_world_from_data (g_mapped_file_get_contents (map), g_mapped_file_get_length (map));
    /* This is GWthDB01 */
    if (db_world_get_magic (db->world) != 0x5747687442443130) {
        g_free (db);
        return NULL;
    }

    db->map = g_steal_pointer (&map);

    db->locations_ref = db_world_get_locations (db->world);
    db->timezones_ref = db_world_get_timezones (db->world);

    db->locations = g_ptr_array_new ();
    db->timezones = g_ptr_array_new ();

    g_ptr_array_set_size (db->locations, db_arrayof_location_get_length (db->locations_ref));
    g_ptr_array_set_size (db->timezones, db_world_timezones_get_length (db->timezones_ref));

    /* Get timestamps for the start and end of this year.
     * This is used to parse timezone information. */
    now = time (NULL);
    tm = *gmtime (&now);
    tm.tm_mon = 0;
    tm.tm_mday = 1;
    tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    db->year_start = mktime (&tm);
    tm.tm_year++;
    db->year_end = mktime (&tm);

    world_db = db;

    return db;
}

/*< private >
 * gweather_location_ensure_world:
 *
 * Ensures that the locations database is available.
 */
void
gweather_location_ensure_world (void)
{
    static GOnce ensure_world_once = G_ONCE_INIT;

    g_once (&ensure_world_once, ensure_world, NULL);
}

/*< private >
 * gweather_get_db:
 *
 * Retrieves a pointer to the locations database.
 *
 * Returns: (transfer none): the root of the locations database
 */
GWeatherDb *
gweather_get_db (void)
{
    gweather_location_ensure_world ();

    return world_db;
}

char *
_radians_to_degrees_str (gdouble radians)
{
    char *str;
    double degrees;

    str = g_malloc0 (DEGREES_STR_SIZE);
    /* Max 4 decimals */
    degrees = (double) ((int) (RADIANS_TO_DEGREES (radians) * 10000)) / 10000;
    /* Too many digits */
    g_return_val_if_fail (degrees <= 1000 || degrees >= -1000, NULL);
    return g_ascii_formatd (str, G_ASCII_DTOSTR_BUF_SIZE, "%g", degrees);
}
