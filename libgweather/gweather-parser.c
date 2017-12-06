/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* parser.c - Locations.xml parser
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
#include <glib.h>
#include <libxml/xmlreader.h>
#include <glib/gi18n-lib.h>

#include "gweather-private.h"
#include "gweather-parser.h"

/*
 * _gweather_parser_get_value:
 * @parser: a #GWeatherParser
 *
 * Gets the text of the element whose start tag @parser is pointing to.
 * Leaves @parser pointing at the next node after the element's end tag.
 *
 * Return value: the text of the current node, as a libxml-allocated
 * string, or %NULL if the node is empty.
 **/
char *
_gweather_parser_get_value (GWeatherParser *parser)
{
    char *value;

    /* check for null node */
    if (xmlTextReaderIsEmptyElement (parser->xml))
	return NULL;

    /* the next "node" is the text node containing the value we want to get */
    if (xmlTextReaderRead (parser->xml) != 1)
	return NULL;

    value = (char *) xmlTextReaderValue (parser->xml);

    /* move on to the end of this node */
    while (xmlTextReaderNodeType (parser->xml) != XML_READER_TYPE_END_ELEMENT) {
	if (xmlTextReaderRead (parser->xml) != 1) {
	    xmlFree (value);
	    return NULL;
	}
    }

    /* consume the end element too */
    if (xmlTextReaderRead (parser->xml) != 1) {
	xmlFree (value);
	return NULL;
    }

    return value;
}

/*
 * _gweather_parser_get_localized_value:
 * @parser: a #GWeatherParser
 *
 * Looks at the name of the element @parser is currently pointing to, and
 * returns the content of either that node, or the translation for
 * it from the gettext domain for gweather locations.
 *
 * Return value: the localized (or unlocalized) text, as a
 * glib-allocated string, or %NULL if the node is empty.
 **/
char *
_gweather_parser_get_localized_value (GWeatherParser *parser)
{
    char *untranslated_value = _gweather_parser_get_value (parser);
    char *ret;

    ret = (char*) g_dgettext ("libgweather-locations", (char*) untranslated_value);

    ret = g_strdup (ret);
    xmlFree (untranslated_value);
    return ret;
}

char *
_gweather_parser_get_msgctxt_value (GWeatherParser *parser)
{
    const char *value;
    const char *name;

    while(xmlTextReaderMoveToNextAttribute(parser->xml)) {
	name = (const char *)xmlTextReaderConstName(parser->xml);
	if (!strcmp (name, "msgctxt")) {
	    value = (const char *)xmlTextReaderConstValue(parser->xml);
	    return g_strdup (value);
	}
    }

    return NULL;
}

static void
gweather_location_list_free (gpointer list)
{
    g_list_free_full (list, (GDestroyNotify) gweather_location_unref);
}

GWeatherParser *
_gweather_parser_new (void)
{
    int zlib_support;
    char *filename;
    GWeatherParser *parser;

    zlib_support = xmlHasFeature (XML_WITH_ZLIB);

    filename = g_build_filename (GWEATHER_XML_LOCATION_DIR, "Locations.xml", NULL);

    if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR) && zlib_support) {
        g_free (filename);
	filename = g_build_filename (GWEATHER_XML_LOCATION_DIR, "Locations.xml.gz", NULL);
    }

    parser = _gweather_parser_new_for_path (filename);

    g_free (filename);

    return parser;
}

GWeatherParser *
_gweather_parser_new_for_path (const char *path)
{
    GWeatherParser *parser;
    int keep_going;
    char *tagname, *format;
    time_t now;
    struct tm tm;

    _gweather_gettext_init ();

    parser = g_slice_new0 (GWeatherParser);

    /* Open the xml file containing the different locations */
    parser->xml = xmlNewTextReaderFilename (path);

    if (parser->xml == NULL)
	goto error_out;

    /* fast forward to the first element */
    do {
	/* if we encounter a problem here, exit right away */
	if (xmlTextReaderRead (parser->xml) != 1)
	    goto error_out;
    } while (xmlTextReaderNodeType (parser->xml) != XML_READER_TYPE_ELEMENT);

    /* check the name and format */
    tagname = (char *) xmlTextReaderName (parser->xml);
    keep_going = tagname && !strcmp (tagname, "gweather");
    xmlFree (tagname);

    if (!keep_going)
	goto error_out;

    format = (char *) xmlTextReaderGetAttribute (parser->xml, (xmlChar *) "format");
    keep_going = format && !strcmp (format, "1.0");
    xmlFree (format);

    if (!keep_going)
	goto error_out;

    /* Get timestamps for the start and end of this year */
    now = time (NULL);
    tm = *gmtime (&now);
    tm.tm_mon = 0;
    tm.tm_mday = 1;
    tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    parser->year_start = mktime (&tm);
    tm.tm_year++;
    parser->year_end = mktime (&tm);

    parser->metar_code_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
						      NULL, gweather_location_list_free);
    parser->timezone_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
						    NULL, (GDestroyNotify) gweather_timezone_unref);
    parser->country_code_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
							NULL, (GDestroyNotify) gweather_location_unref);

    return parser;

error_out:
    _gweather_parser_free (parser);
    return NULL;
}

void
_gweather_parser_free (GWeatherParser *parser)
{
    if (parser->xml)
	xmlFreeTextReader (parser->xml);
    if (parser->metar_code_cache)
	g_hash_table_unref (parser->metar_code_cache);
    if (parser->country_code_cache)
	g_hash_table_unref (parser->country_code_cache);
    if (parser->timezone_cache)
	g_hash_table_unref (parser->timezone_cache);

    g_slice_free (GWeatherParser, parser);
}
