/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gweather-timezone.c - Timezone handling
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include "gweather-timezone.h"
#include "parser.h"

struct _GWeatherTimezone {
    char *id, *name;
    int offset, dst_offset;
    gboolean has_dst;

    int ref_count;
};

#define ZONEINFO_DIR "/usr/share/zoneinfo"

#define TZ_MAGIC "TZif"
#define TZ_HEADER_SIZE 44
#define TZ_TIMECNT_OFFSET 32
#define TZ_TRANSITIONS_OFFSET 44

#define TZ_TTINFO_SIZE 6
#define TZ_TTINFO_GMTOFF_OFFSET 0
#define TZ_TTINFO_ISDST_OFFSET 4

static gboolean
parse_tzdata (const char *tzname, time_t start, time_t end,
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

    filename = g_build_filename (ZONEINFO_DIR, tzname, NULL);
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

    timecnt = GUINT32_FROM_BE (*(guint32 *)(contents + TZ_TIMECNT_OFFSET));
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
    initial_offset = *(gint32 *)(ttinfos +
				 initial_transition * TZ_TTINFO_SIZE +
				 TZ_TTINFO_GMTOFF_OFFSET);
    initial_offset = GINT32_FROM_BE (initial_offset);
    initial_isdst = *(ttinfos +
		      initial_transition * TZ_TTINFO_SIZE +
		      TZ_TTINFO_ISDST_OFFSET);

    if (second_transition != -1) {
	second_offset = *(gint32 *)(ttinfos +
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

static GWeatherTimezone *
parse_timezone (GWeatherParser *parser)
{
    GWeatherTimezone *zone = NULL;
    char *id = NULL, *name = NULL;
    int offset, dst_offset;
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

	    if (!strcmp ((const char *) xmlTextReaderConstName (parser->xml), "name"))
		name = gweather_parser_get_localized_value (parser);
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
	zone->name = g_strdup (name);
	zone->offset = offset;
	zone->has_dst = has_dst;
	zone->dst_offset = dst_offset;
    }

    xmlFree (id);
    if (name)
	xmlFree (name);

    return zone;
}

GWeatherTimezone **
gweather_timezones_parse_xml (GWeatherParser *parser)
{
    GPtrArray *zones;
    GWeatherTimezone *zone;
    const char *tagname;
    int tagtype, i;

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
	    if (!zone)
		goto error_out;
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

GWeatherTimezone *
gweather_timezone_ref (GWeatherTimezone *zone)
{
    zone->ref_count++;
    return zone;
}

void
gweather_timezone_unref (GWeatherTimezone *zone)
{
    if (!--zone->ref_count) {
	g_free (zone->id);
	g_free (zone->name);
	g_slice_free (GWeatherTimezone, zone);
    }
}

const char *
gweather_timezone_get_name (GWeatherTimezone *zone)
{
    return zone->name;
}

const char *
gweather_timezone_get_tzid (GWeatherTimezone *zone)
{
    return zone->id;
}

int
gweather_timezone_get_offset (GWeatherTimezone *zone)
{
    return zone->offset;
}

gboolean
gweather_timezone_has_dst (GWeatherTimezone *zone)
{
    return zone->has_dst;
}

int
gweather_timezone_get_dst_offset (GWeatherTimezone *zone)
{
    g_return_val_if_fail (zone->has_dst, 0);
    return zone->dst_offset;
}

