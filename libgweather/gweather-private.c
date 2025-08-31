/* gweather-private.c - Overall weather server functions
 *
 * SPDX-FileCopyrightText: The GWeather authors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <math.h>

#include "gweather-private.h"

/* sign, 3 digits, separator, 4 decimals, nul-char */
#define DEGREES_STR_SIZE (1 + 3 + 1 + 4 + 1)
#define R                6372.795

static GWeatherDb *world_db;

void
_gweather_location_reset_world (void)
{
    guint i;

    g_return_if_fail (world_db != NULL);

    /* At this point, we had a leak if the caches are not completely empty. */
    for (i = 0; i < world_db->locations->len; i++) {
        if (G_UNLIKELY (g_ptr_array_index (world_db->locations, i) != NULL)) {
            g_warning ("Location with index %u and name %s is still referenced!",
                       i,
                       gweather_location_get_name (g_ptr_array_index (world_db->locations, i)));
            g_assert_not_reached ();
        }
    }
    for (i = 0; i < world_db->timezones->len; i++) {
        if (G_UNLIKELY (g_ptr_array_index (world_db->timezones, i) != NULL)) {
            g_warning ("Timezone with index %u and tzid %s is still referenced!",
                       i,
                       g_time_zone_get_identifier (g_ptr_array_index (world_db->timezones, i)));
            g_assert_not_reached ();
        }
    }
}

static struct kdtree *
index_lat_lon (DbArrayofLocationRef locations_ref)
{
    struct kdtree *tree;
    gsize size;

    tree = kd_create (3);
    size = db_arrayof_location_get_length (locations_ref);

    for (gsize i = 0; i < size; i++) {
        DbLocationRef loc_ref = db_arrayof_location_get_at (locations_ref, i);

        if (db_location_get_level (loc_ref) == GWEATHER_LOCATION_CITY) {
            DbCoordinateRef coord = db_location_get_coordinates (loc_ref);
            double lat = db_coordinate_get_lat (coord);
            double lon = db_coordinate_get_lon (coord);

            if (isfinite (lat) && isfinite (lon)) {
                double lat_r = lat * (M_PI / 180.0);
                double lon_r = lon * (M_PI / 180.0);
                double x = R * cos (lat_r) * cos (lon_r);
                double y = R * cos (lat_r) * sin (lon_r);
                double z = R * sin (lat_r);

                kd_insert3 (tree, x, y, z, GSIZE_TO_POINTER (i));
            }
        }
    }

    return tree;
}

static gpointer
ensure_world (gpointer dummy G_GNUC_UNUSED)
{
    g_autoptr (GError) error = NULL;
    g_autofree char *filename = NULL;
    g_autoptr (GMappedFile) map = NULL;
    const char *locations_path;
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
        filename = g_strdup (GWEATHER_LOCATIONS_DB);
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

    db->cities_kdtree = index_lat_lon (db->locations_ref);

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

gssize
_gweather_find_nearest_city_index (double lat,
                                   double lon)
{
    double lat_r = lat * (M_PI / 180.0);
    double lon_r = lon * (M_PI / 180.0);
    double x = R * cos (lat_r) * cos (lon_r);
    double y = R * cos (lat_r) * sin (lon_r);
    double z = R * sin (lat_r);
    struct kdres *set;

    if (world_db == NULL || world_db->cities_kdtree == NULL)
        return -1;

    set = kd_nearest3 (world_db->cities_kdtree, x, y, z);
    if (set == NULL || kd_res_size (set) == 0) {
        kd_res_free (set);
        return -1;
    }

    gssize res = (gssize) GPOINTER_TO_SIZE (kd_res_item_data (set));
    kd_res_free (set);

    return res;
}
