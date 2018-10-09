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
 * <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gweather-timezone.h"
#include "gweather-parser.h"
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
    char *id, *name;
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

/**
 * gweather_timezone_get_by_tzid:
 * @tzid: A timezone identifier, like "America/New_York" or "Europe/London"
 *
 * Get the #GWeatherTimezone for @tzid.
 *
 * Returns: (transfer none): A #GWeatherTimezone. This object
 * belongs to GWeather, do not unref it.
 *
 * Since: 3.12
 */
GWeatherTimezone *
gweather_timezone_get_by_tzid (const char *tzid)
{
    GWeatherLocation *world;

    g_return_val_if_fail (tzid != NULL, NULL);

    world = gweather_location_get_world ();

    return g_hash_table_lookup (world->timezone_cache, tzid);
}

static GWeatherTimezone *
parse_timezone (GWeatherParser *parser)
{
    GWeatherTimezone *zone = NULL;
    char *id = NULL;
    char *name = NULL;
    int offset = 0, dst_offset = 0;
    gboolean has_dst = FALSE;

    id = (char *) xmlTextReaderGetAttribute (parser->xml, (xmlChar *) "id");
    if (!id) {
	xmlTextReaderNext (parser->xml);
	return NULL;
    }

    if (!xmlTextReaderIsEmptyElement (parser->xml)) {
	if (xmlTextReaderRead (parser->xml) != 1) {
	    xmlFree (id);
	    return NULL;
	}

	while (xmlTextReaderNodeType (parser->xml) != XML_READER_TYPE_END_ELEMENT) {
	    if (xmlTextReaderNodeType (parser->xml) != XML_READER_TYPE_ELEMENT) {
		if (xmlTextReaderRead (parser->xml) != 1)
		    break;
		continue;
	    }

	    if (!strcmp ((const char *) xmlTextReaderConstName (parser->xml), "_name"))
		name = _gweather_parser_get_localized_value (parser);
	    else {
		if (xmlTextReaderNext (parser->xml) != 1)
		    break;
	    }
	}
    }

    if (parse_tzdata (id, parser->year_start, parser->year_end,
		      &offset, &has_dst, &dst_offset)) {
	zone = g_slice_new0 (GWeatherTimezone);
	zone->ref_count = 1;
	zone->id = g_strdup (id);
	zone->name = name;
	zone->offset = offset;
	zone->has_dst = has_dst;
	zone->dst_offset = dst_offset;

	g_hash_table_insert (parser->timezone_cache, zone->id, gweather_timezone_ref (zone));

	name = NULL;
    }

    g_free (name);
    xmlFree (id);

    return zone;
}

GWeatherTimezone **
_gweather_timezones_parse_xml (GWeatherParser *parser)
{
    GPtrArray *zones;
    GWeatherTimezone *zone;
    const char *tagname;
    int tagtype;
    unsigned int i;

    zones = g_ptr_array_new ();

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

	if (!strcmp (tagname, "timezone")) {
	    zone = parse_timezone (parser);
	    if (zone)
		g_ptr_array_add (zones, zone);
	}

	if (xmlTextReaderNext (parser->xml) != 1)
	    goto error_out;
    }
    if (xmlTextReaderRead (parser->xml) != 1)
	goto error_out;

    g_ptr_array_add (zones, NULL);
    return (GWeatherTimezone **)g_ptr_array_free (zones, FALSE);

error_out:
    for (i = 0; i < zones->len; i++)
	gweather_timezone_unref (zones->pdata[i]);
    g_ptr_array_free (zones, TRUE);
    return NULL;
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
	g_free (zone->id);
	g_free (zone->name);
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
    zone->id = g_strdup ("GMT");
    zone->name = g_strdup (_("Greenwich Mean Time"));
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
    g_return_val_if_fail (zone != NULL, NULL);
    return zone->name;
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
    return zone->id;
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

