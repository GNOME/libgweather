/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#ifndef GWEATHER_PARSER_H
#define GWEATHER_PARSER_H 1

#include <libxml/xmlreader.h>
#include "gweather-timezone.h"

typedef struct {
    xmlTextReaderPtr xml;
    const char * const *locales;
    gboolean use_regions;
    time_t year_start, year_end;
} GWeatherParser;

GWeatherParser *gweather_parser_new                 (gboolean        use_regions);
void            gweather_parser_free                (GWeatherParser *parser);

char           *gweather_parser_get_value           (GWeatherParser *parser);
char           *gweather_parser_get_localized_value (GWeatherParser *parser);

/* from gweather-timezone.c */
GWeatherTimezone **gweather_timezones_parse_xml (GWeatherParser *parser);

#endif
