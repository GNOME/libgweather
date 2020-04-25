/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gweather-timezone.c - Timezone handling
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
 * <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gweather-timezone.h"
#include "gweather-private.h"

/**
 * SECTION:gweathertimezone
 * @Title: GWeatherTimezone
 *
 * A timezone.
 *
 * Timezones are global to the GWeather world (as obtained by
 * gweather_location_get_world()); they can be gotten by passing
 * gweather_timezone_get_by_tzid() with a tzid like "America/New_York"
 * or "Europe/London".
 */

struct _GWeatherTimezone {
    GWeatherDb *db;
    guint db_idx;

    /* Attributes with _ may be fetched/filled from the database on the fly. */
    char *_id, *_name;
    int offset, dst_offset;
    gboolean has_dst;

    int ref_count;
};

#define TZ_MAGIC "TZif"
#define TZ_HEADER_SIZE 44
#define TZ_TIMECNT_OFFSET 32
#define TZ_TRANSITIONS_OFFSET 44

#define TZ_TTINFO_SIZE 6
#define TZ_TTINFO_GMTOFF_OFFSET 0
#define TZ_TTINFO_ISDST_OFFSET 4

static gboolean
parse_tzdata (const char *tz_name, time_t start, time_t end,
	      int *offset, gboolean *has_dst, int *dst_offset)
{
    char *filename, *contents;
    gsize length;
    int timecnt, transitions_size, ttinfo_map_size;
    int initial_transition = -1, second_transition = -1;
    gint32 *transitions;
    char *ttinfo_map, *ttinfos;
    gint32 initial_offset, second_offset;
    char initial_isdst, second_isdst;
    int i;

    filename = g_build_filename (ZONEINFO_DIR, tz_name, NULL);
    if (!g_file_get_contents (filename, &contents, &length, NULL)) {
	g_free (filename);
	return FALSE;
    }
    g_free (filename);

    if (length < TZ_HEADER_SIZE ||
	strncmp (contents, TZ_MAGIC, strlen (TZ_MAGIC)) != 0) {
	g_free (contents);
	return FALSE;
    }

    timecnt = GUINT32_FROM_BE (*(guint32 *)(void *)(contents + TZ_TIMECNT_OFFSET));
    transitions = (void *)(contents + TZ_TRANSITIONS_OFFSET);
    transitions_size = timecnt * sizeof (*transitions);
    ttinfo_map = (void *)(contents + TZ_TRANSITIONS_OFFSET + transitions_size);
    ttinfo_map_size = timecnt;
    ttinfos = (void *)(ttinfo_map + ttinfo_map_size);

    /* @transitions is an array of @timecnt time_t values. We need to
     * find the transition into the current offset, which is the last
     * transition before @start. If the following transition is before
     * @end, then note that one too, since it presumably means we're
     * doing DST.
     */
    for (i = 1; i < timecnt && initial_transition == -1; i++) {
	if (GINT32_FROM_BE (transitions[i]) > start) {
	    initial_transition = ttinfo_map[i - 1];
	    if (GINT32_FROM_BE (transitions[i]) < end)
		second_transition = ttinfo_map[i];
	}
    }
    if (initial_transition == -1) {
	if (timecnt)
	    initial_transition = ttinfo_map[timecnt - 1];
	else
	    initial_transition = 0;
    }

    /* Copy the data out of the corresponding ttinfo structs */
    initial_offset = *(gint32 *)(void *)(ttinfos +
				 initial_transition * TZ_TTINFO_SIZE +
				 TZ_TTINFO_GMTOFF_OFFSET);
    initial_offset = GINT32_FROM_BE (initial_offset);
    initial_isdst = *(ttinfos +
		      initial_transition * TZ_TTINFO_SIZE +
		      TZ_TTINFO_ISDST_OFFSET);

    if (second_transition != -1) {
	second_offset = *(gint32 *)(void *)(ttinfos +
				    second_transition * TZ_TTINFO_SIZE +
				    TZ_TTINFO_GMTOFF_OFFSET);
	second_offset = GINT32_FROM_BE (second_offset);
	second_isdst = *(ttinfos +
			 second_transition * TZ_TTINFO_SIZE +
			 TZ_TTINFO_ISDST_OFFSET);

	*has_dst = (initial_isdst != second_isdst);
    } else
	*has_dst = FALSE;

    if (!*has_dst)
	*offset = initial_offset / 60;
    else {
	if (initial_isdst) {
	    *offset = second_offset / 60;
	    *dst_offset = initial_offset / 60;
	} else {
	    *offset = initial_offset / 60;
	    *dst_offset = second_offset / 60;
	}
    }

    g_free (contents);
    return TRUE;
}

GWeatherTimezone *
_gweather_timezone_ref_for_idx (GWeatherDb       *db,
				 guint             idx)
{
    GWeatherTimezone *zone;
    DbWorldTimezonesEntryRef ref;
    const char *id;
    int offset = 0, dst_offset = 0;
    gboolean has_dst = FALSE;

    g_assert (db);
    g_assert (idx < db->timezones->len);
    zone = g_ptr_array_index (db->timezones, idx);
    if (zone)
        return gweather_timezone_ref (zone);

    ref = db_world_timezones_get_at (db->timezones_ref, idx);
    id = db_world_timezones_entry_get_key (ref);

    if (parse_tzdata (id, db->year_start, db->year_end,
		      &offset, &has_dst, &dst_offset)) {
	zone = g_slice_new0 (GWeatherTimezone);
	zone->ref_count = 1;
	zone->db = db;
	zone->db_idx = idx;

	zone->offset = offset;
	zone->has_dst = has_dst;
	zone->dst_offset = dst_offset;

	/* Insert weak reference */
	g_ptr_array_index (db->timezones, idx) = zone;
    }

    return zone;
}

/**
 * gweather_timezone_get_by_tzid:
 * @tzid: A timezone identifier, like "America/New_York" or "Europe/London"
 *
 * Get the #GWeatherTimezone for @tzid.
 *
 * Prior to version 40 no reference was returned.
 *
 * Returns: (transfer full): A #GWeatherTimezone.
 *
 * Since: 3.12
 */
GWeatherTimezone *
gweather_timezone_get_by_tzid (const char *tzid)
{
    GWeatherLocation *world;
    GWeatherDb *db;
    gsize idx;

    g_return_val_if_fail (tzid != NULL, NULL);

    /* TODO: Get the DB directly */
    world = gweather_location_get_world ();
    db = world->db;
    gweather_location_unref (world);

    if (!db_world_timezones_lookup (db->timezones_ref, tzid, &idx, NULL))
	return NULL;

    return _gweather_timezone_ref_for_idx (db, idx);
}

/**
 * gweather_timezone_ref:
 * @zone: a #GWeatherTimezone
 *
 * Adds 1 to @zone's reference count.
 *
 * Return value: @zone
 **/
GWeatherTimezone *
gweather_timezone_ref (GWeatherTimezone *zone)
{
    g_return_val_if_fail (zone != NULL, NULL);

    zone->ref_count++;
    return zone;
}

/**
 * gweather_timezone_unref:
 * @zone: a #GWeatherTimezone
 *
 * Subtracts 1 from @zone's reference count and frees it if it reaches 0.
 **/
void
gweather_timezone_unref (GWeatherTimezone *zone)
{
    g_return_if_fail (zone != NULL);

    if (!--zone->ref_count) {
	if (zone->db)
		g_ptr_array_index (zone->db->timezones, zone->db_idx) = 0;

	g_free (zone->_id);
	g_free (zone->_name);
	g_slice_free (GWeatherTimezone, zone);
    }
}

GType
gweather_timezone_get_type (void)
{
    static volatile gsize type_volatile = 0;

    if (g_once_init_enter (&type_volatile)) {
	GType type = g_boxed_type_register_static (
	    g_intern_static_string ("GWeatherTimezone"),
	    (GBoxedCopyFunc) gweather_timezone_ref,
	    (GBoxedFreeFunc) gweather_timezone_unref);
	g_once_init_leave (&type_volatile, type);
    }
    return type_volatile;
}

/**
 * gweather_timezone_get_utc:
 *
 * Gets the UTC timezone.
 *
 * Return value: a #GWeatherTimezone for UTC, or %NULL on error.
 **/
GWeatherTimezone *
gweather_timezone_get_utc (void)
{
    GWeatherTimezone *zone = NULL;

    zone = g_slice_new0 (GWeatherTimezone);
    zone->ref_count = 1;
    zone->db_idx = INVALID_IDX;
    zone->_id = g_strdup ("GMT");
    zone->_name = g_strdup (_("Greenwich Mean Time"));
    zone->offset = 0;
    zone->has_dst = FALSE;
    zone->dst_offset = 0;

    return zone;
}

/**
 * gweather_timezone_get_name:
 * @zone: a #GWeatherTimezone
 *
 * Gets @zone's name; a translated, user-presentable string.
 *
 * Note that the returned name might not be unique among timezones,
 * and may not make sense to the user unless it is presented along
 * with the timezone's country's name (or in some context where the
 * country is obvious).
 *
 * Return value: @zone's name
 **/
const char *
gweather_timezone_get_name (GWeatherTimezone *zone)
{
    DbTimezoneRef ref;
    const char *name;
    const char *msgctxt;

    g_return_val_if_fail (zone != NULL, NULL);
    if (zone->_name)
        return zone->_name;

    if (!zone->db || !IDX_VALID (zone->db_idx))
	return NULL;

    ref = db_world_timezones_entry_get_value (db_world_timezones_get_at (zone->db->timezones_ref, zone->db_idx));
    name = EMPTY_TO_NULL (db_i18n_get_str (db_timezone_get_name (ref)));
    msgctxt = EMPTY_TO_NULL (db_i18n_get_msgctxt (db_timezone_get_name (ref)));

    if (!name)
	return NULL;

    if (msgctxt)
        zone->_name = g_strdup (g_dpgettext2 ("libgweather-locations", msgctxt, name));
    else
        zone->_name = g_strdup (g_dgettext ("libgweather-locations", name));
    return zone->_name;
}

/**
 * gweather_timezone_get_tzid:
 * @zone: a #GWeatherTimezone
 *
 * Gets @zone's tzdata identifier, eg "America/New_York".
 *
 * Return value: @zone's tzid
 **/
const char *
gweather_timezone_get_tzid (GWeatherTimezone *zone)
{
    g_return_val_if_fail (zone != NULL, NULL);
    if (zone->_id)
        return zone->_id;

    if (!zone->db || !IDX_VALID (zone->db_idx))
	return NULL;

    return db_world_timezones_entry_get_key (db_world_timezones_get_at (zone->db->timezones_ref, zone->db_idx));
}

/**
 * gweather_timezone_get_offset:
 * @zone: a #GWeatherTimezone
 *
 * Gets @zone's standard offset from UTC, in minutes. Eg, a value of
 * 120 would indicate "GMT+2".
 *
 * Return value: @zone's standard offset, in minutes
 **/
int
gweather_timezone_get_offset (GWeatherTimezone *zone)
{
    g_return_val_if_fail (zone != NULL, 0);
    return zone->offset;
}

/**
 * gweather_timezone_has_dst:
 * @zone: a #GWeatherTimezone
 *
 * Checks if @zone observes daylight/summer time for part of the year.
 *
 * Return value: %TRUE if @zone observes daylight/summer time.
 **/
gboolean
gweather_timezone_has_dst (GWeatherTimezone *zone)
{
    g_return_val_if_fail (zone != NULL, FALSE);
    return zone->has_dst;
}

/**
 * gweather_timezone_get_dst_offset:
 * @zone: a #GWeatherTimezone
 *
 * Gets @zone's daylight/summer time offset from UTC, in minutes. Eg,
 * a value of 120 would indicate "GMT+2". This is only meaningful if
 * gweather_timezone_has_dst() returns %TRUE.
 *
 * Return value: @zone's daylight/summer time offset, in minutes
 **/
int
gweather_timezone_get_dst_offset (GWeatherTimezone *zone)
{
    g_return_val_if_fail (zone != NULL, 0);
    g_return_val_if_fail (zone->has_dst, 0);
    return zone->dst_offset;
}

