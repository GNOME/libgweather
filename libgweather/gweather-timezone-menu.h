/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* timezone-menu.h - Timezone-selecting menu
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

#ifndef GWEATHER_TIMEZONE_MENU_H
#define GWEATHER_TIMEZONE_MENU_H 1

#if !(defined(IN_GWEATHER_H) || defined(GWEATHER_COMPILATION))
#error "gweather-timezone-menu.h must not be included individually, include gweather.h instead"
#endif

#include <gtk/gtk.h>
#include <libgweather/gweather-location.h>

typedef struct _GWeatherTimezoneMenu GWeatherTimezoneMenu;
typedef struct _GWeatherTimezoneMenuClass GWeatherTimezoneMenuClass;

#define GWEATHER_TYPE_TIMEZONE_MENU            (gweather_timezone_menu_get_type ())
#define GWEATHER_TIMEZONE_MENU(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GWEATHER_TYPE_TIMEZONE_MENU, GWeatherTimezoneMenu))
#define GWEATHER_TIMEZONE_MENU_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GWEATHER_TYPE_TIMEZONE_MENU, GWeatherTimezoneMenuClass))
#define GWEATHER_IS_TIMEZONE_MENU(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GWEATHER_TYPE_TIMEZONE_MENU))
#define GWEATHER_IS_TIMEZONE_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GWEATHER_TYPE_TIMEZONE_MENU))
#define GWEATHER_TIMEZONE_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GWEATHER_TYPE_TIMEZONE_MENU, GWeatherTimezoneMenuClass))

struct _GWeatherTimezoneMenu {
    GtkComboBox parent;

    /*< private >*/
    GWeatherTimezone *zone;
};

struct _GWeatherTimezoneMenuClass {
    GtkComboBoxClass parent_class;

};

GWEATHER_EXTERN
GType       gweather_timezone_menu_get_type         (void);

GWEATHER_EXTERN
GtkWidget  *gweather_timezone_menu_new              (GWeatherLocation     *top);

GWEATHER_EXTERN
void        gweather_timezone_menu_set_tzid         (GWeatherTimezoneMenu *menu,
						     const char           *tzid);
GWEATHER_EXTERN
const char *gweather_timezone_menu_get_tzid         (GWeatherTimezoneMenu *menu);

#endif
