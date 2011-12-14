/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gweather-xml.h
 *
 * Copyright (C) 2004 Gareth Owen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __GWEATHER_XML_H__
#define __GWEATHER_XML_H__

#include <gtk/gtk.h>
#include <libgweather/gweather-weather.h>

enum
{
    GWEATHER_XML_COL_LOCATION_NAME = 0,
    GWEATHER_XML_COL_METAR_CODE,
    GWEATHER_XML_COL_LATLON_VALID,
    GWEATHER_XML_COL_LATITUDE,
    GWEATHER_XML_COL_LONGITUDE,
    GWEATHER_XML_NUM_COLUMNS
};

GtkTreeModel *gweather_xml_load_locations (void);

#endif /* __GWEATHER_XML_H__ */
