/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gweather-xml.c - Locations.xml parsing code
 *
 * Copyright (C) 2005 Ryan Lortie, 2004 Gareth Owen
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <math.h>
#include <locale.h>
#include <gtk/gtk.h>
#include <libxml/xmlreader.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include "gweather-xml.h"
#include "weather-priv.h"


/**
 * SECTION:gweather-xml
 * @Title: gweather-xml
 */


static gboolean
gweather_xml_parse_node (GWeatherLocation *gloc,
			 GtkTreeStore *store, GtkTreeIter *parent)
{
    GtkTreeIter iter, *self = &iter;
    GWeatherLocation **children, *parent_loc;
    GWeatherLocationLevel level;
    WeatherLocation *wloc;
    const char *name;
    int i;

    name = gweather_location_get_name (gloc);
    children = gweather_location_get_children (gloc);
    level = gweather_location_get_level (gloc);

    if (!children[0] && level < GWEATHER_LOCATION_WEATHER_STATION) {
	gweather_location_free_children (gloc, children);
	return TRUE;
    }

    switch (gweather_location_get_level (gloc)) {
    case GWEATHER_LOCATION_WORLD:
    case GWEATHER_LOCATION_ADM2:
	self = parent;
	break;

    case GWEATHER_LOCATION_REGION:
    case GWEATHER_LOCATION_COUNTRY:
    case GWEATHER_LOCATION_ADM1:
	/* Create a row with a name but no WeatherLocation */
	gtk_tree_store_append (store, &iter, parent);
	gtk_tree_store_set (store, &iter,
			    GWEATHER_XML_COL_LOCATION_NAME, name,
			    -1);
	break;

    case GWEATHER_LOCATION_CITY:
	/* If multiple children, treat this like a
	 * region/country/adm1. If a single child, merge with that
	 * location.
	 */
	gtk_tree_store_append (store, &iter, parent);
	gtk_tree_store_set (store, &iter,
			    GWEATHER_XML_COL_LOCATION_NAME, name,
			    -1);
	if (children[0] && !children[1]) {
	    wloc = _weather_location_from_gweather_location (children[0], name);
	    gtk_tree_store_set (store, &iter,
				GWEATHER_XML_COL_METAR_CODE, wloc->code,
				GWEATHER_XML_COL_LATLON_VALID, wloc->latlon_valid,
				GWEATHER_XML_COL_LATITUDE, wloc->latitude,
				GWEATHER_XML_COL_LONGITUDE, wloc->longitude,
				-1);
	    _weather_location_free (wloc);
	}
	break;

    case GWEATHER_LOCATION_WEATHER_STATION:
	gtk_tree_store_append (store, &iter, parent);
	gtk_tree_store_set (store, &iter,
			    GWEATHER_XML_COL_LOCATION_NAME, name,
			    -1);

	parent_loc = gweather_location_get_parent (gloc);
	if (parent_loc && gweather_location_get_level (parent_loc) == GWEATHER_LOCATION_CITY)
	    name = gweather_location_get_name (parent_loc);
	wloc = _weather_location_from_gweather_location (gloc, name);
	gtk_tree_store_set (store, &iter,
			    GWEATHER_XML_COL_METAR_CODE, wloc->code,
			    GWEATHER_XML_COL_LATLON_VALID, wloc->latlon_valid,
			    GWEATHER_XML_COL_LATITUDE, wloc->latitude,
			    GWEATHER_XML_COL_LONGITUDE, wloc->longitude,
			    -1);
	_weather_location_free (wloc);

    case GWEATHER_LOCATION_DETACHED:
	g_assert_not_reached ();

	break;
    }

    for (i = 0; children[i]; i++) {
	if (!gweather_xml_parse_node (children[i], store, self)) {
	    gweather_location_free_children (gloc, children);
	    return FALSE;
	}
    }

    gweather_location_free_children (gloc, children);
    return TRUE;
}

GtkTreeModel *
gweather_xml_load_locations (void)
{
    GWeatherLocation *world;
    GtkTreeStore *store;

    world = gweather_location_new_world (TRUE);
    if (!world)
	return NULL;

    store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_DOUBLE, G_TYPE_DOUBLE);

    if (!gweather_xml_parse_node (world, store, NULL)) {
	g_object_unref (store);
	store = NULL;
    }

    gweather_location_unref (world);

    return (GtkTreeModel *)store;
}
