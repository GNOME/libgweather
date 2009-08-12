/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* location-entry.h - Location-selecting text entry
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

#ifndef GWEATHER_LOCATION_ENTRY_H
#define GWEATHER_LOCATION_ENTRY_H 1

#include <gtk/gtk.h>
#include <libgweather/gweather-location.h>

#define GWEATHER_TYPE_LOCATION_ENTRY            (gweather_location_entry_get_type ())
#define GWEATHER_LOCATION_ENTRY(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GWEATHER_TYPE_LOCATION_ENTRY, GWeatherLocationEntry))
#define GWEATHER_LOCATION_ENTRY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GWEATHER_TYPE_LOCATION_ENTRY, GWeatherLocationEntryClass))
#define GWEATHER_IS_LOCATION_ENTRY(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GWEATHER_TYPE_LOCATION_ENTRY))
#define GWEATHER_IS_LOCATION_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GWEATHER_TYPE_LOCATION_ENTRY))
#define GWEATHER_LOCATION_ENTRY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GWEATHER_TYPE_LOCATION_ENTRY, GWeatherLocationEntryClass))

typedef struct {
    GtkEntry parent;

    /*< private >*/
    GWeatherLocation *location, *top;
    guint custom_text : 1;
} GWeatherLocationEntry;

typedef struct {
    GtkEntryClass parent_class;

} GWeatherLocationEntryClass;

GType             gweather_location_entry_get_type     (void);

GtkWidget        *gweather_location_entry_new          (GWeatherLocation      *top);

void              gweather_location_entry_set_location (GWeatherLocationEntry *entry,
							GWeatherLocation      *loc);
GWeatherLocation *gweather_location_entry_get_location (GWeatherLocationEntry *entry);

gboolean          gweather_location_entry_has_custom_text (GWeatherLocationEntry *entry);

gboolean          gweather_location_entry_set_city     (GWeatherLocationEntry *entry,
							const char            *city_name,
							const char            *code);

#endif
