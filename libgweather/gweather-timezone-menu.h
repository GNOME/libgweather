/* gweather-timezone-menu.h - Timezone-selecting menu
 *
 * SPDX-FileCopyrightText: 2008, Red Hat, Inc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !(defined(IN_GWEATHER_H) || defined(GWEATHER_COMPILATION))
#error "gweather-timezone-menu.h must not be included individually, include gweather.h instead"
#endif

#include <gtk/gtk.h>
#include <libgweather/gweather-location.h>

G_BEGIN_DECLS

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

GWEATHER_AVAILABLE_IN_ALL
GType           gweather_timezone_menu_get_type         (void);
GWEATHER_AVAILABLE_IN_ALL
GtkWidget *     gweather_timezone_menu_new              (GWeatherLocation *top);
GWEATHER_AVAILABLE_IN_ALL
void            gweather_timezone_menu_set_tzid         (GWeatherTimezoneMenu *menu,
						         const char *tzid);
GWEATHER_AVAILABLE_IN_ALL
const char *    gweather_timezone_menu_get_tzid         (GWeatherTimezoneMenu *menu);

G_END_DECLS
