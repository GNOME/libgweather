/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* timezone-menu.c - Timezone-selecting menu
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

#include "gweather-timezone-menu.h"
#include "gweather-private.h"

#include <string.h>

/**
 * SECTION:gweathertimezonemenu
 * @Title: GWeatherTimezoneMenu
 *
 * A #GtkComboBox subclass for choosing a #GWeatherTimezone
 */

G_DEFINE_TYPE (GWeatherTimezoneMenu, gweather_timezone_menu, GTK_TYPE_COMBO_BOX)

enum {
    PROP_0,

    PROP_TOP,
    PROP_TZID,

    LAST_PROP
};

static void set_property (GObject *object, guint prop_id,
			  const GValue *value, GParamSpec *pspec);
static void get_property (GObject *object, guint prop_id,
			  GValue *value, GParamSpec *pspec);

static void changed      (GtkComboBox *combo);

static GtkTreeModel *gweather_timezone_model_new (GWeatherLocation *top);
static gboolean row_separator_func (GtkTreeModel *model, GtkTreeIter *iter,
				    gpointer data);
static void is_sensitive (GtkCellLayout *cell_layout, GtkCellRenderer *cell,
			  GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data);

static void
gweather_timezone_menu_init (GWeatherTimezoneMenu *menu)
{
    GtkCellRenderer *renderer;

    gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (menu),
					  row_separator_func, NULL, NULL);

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (menu), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (menu), renderer,
				    "markup", 0,
				    NULL);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (menu),
					renderer, is_sensitive, NULL, NULL);
}

static void
finalize (GObject *object)
{
    GWeatherTimezoneMenu *menu = GWEATHER_TIMEZONE_MENU (object);

    if (menu->zone)
	gweather_timezone_unref (menu->zone);

    G_OBJECT_CLASS (gweather_timezone_menu_parent_class)->finalize (object);
}

static void
gweather_timezone_menu_class_init (GWeatherTimezoneMenuClass *timezone_menu_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (timezone_menu_class);
    GtkComboBoxClass *combo_class = GTK_COMBO_BOX_CLASS (timezone_menu_class);

    object_class->finalize = finalize;
    object_class->set_property = set_property;
    object_class->get_property = get_property;

    combo_class->changed = changed;

    /* properties */
    g_object_class_install_property (
	object_class, PROP_TOP,
	g_param_spec_boxed ("top",
			    "Top Location",
			    "The GWeatherLocation whose children will be used to fill in the menu",
			    GWEATHER_TYPE_LOCATION,
			    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (
	object_class, PROP_TZID,
	g_param_spec_string ("tzid",
			     "TZID",
			     "The selected TZID",
			     NULL,
			     G_PARAM_READWRITE));
}

static void
set_property (GObject *object, guint prop_id,
	      const GValue *value, GParamSpec *pspec)
{
    GtkTreeModel *model;

    switch (prop_id) {
    case PROP_TOP:
	model = gweather_timezone_model_new (g_value_get_boxed (value));
	gtk_combo_box_set_model (GTK_COMBO_BOX (object), model);
	g_object_unref (model);
	gtk_combo_box_set_active (GTK_COMBO_BOX (object), 0);
	break;

    case PROP_TZID:
	gweather_timezone_menu_set_tzid (GWEATHER_TIMEZONE_MENU (object),
					 g_value_get_string (value));
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
    }
}

static void
get_property (GObject *object, guint prop_id,
	      GValue *value, GParamSpec *pspec)
{
    GWeatherTimezoneMenu *menu = GWEATHER_TIMEZONE_MENU (object);

    switch (prop_id) {
    case PROP_TZID:
	g_value_set_string (value, gweather_timezone_menu_get_tzid (menu));
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
    }
}

enum {
    GWEATHER_TIMEZONE_MENU_NAME,
    GWEATHER_TIMEZONE_MENU_ZONE
};

static void
changed (GtkComboBox *combo)
{
    GWeatherTimezoneMenu *menu = GWEATHER_TIMEZONE_MENU (combo);
    GtkTreeIter iter;

    if (menu->zone)
	gweather_timezone_unref (menu->zone);

    gtk_combo_box_get_active_iter (combo, &iter);
    gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter,
			GWEATHER_TIMEZONE_MENU_ZONE, &menu->zone,
			-1);

    g_object_notify (G_OBJECT (combo), "tzid");
}

static void
append_offset (GString *desc, int offset)
{
    int hours, minutes;

    hours = offset / 60;
    minutes = (offset > 0) ? offset % 60 : -offset % 60;

    if (minutes)
	g_string_append_printf (desc, "GMT%+d:%02d", hours, minutes);
    else if (hours)
	g_string_append_printf (desc, "GMT%+d", hours);
    else
	g_string_append (desc, "GMT");
}

static char *
get_offset (GWeatherTimezone *zone)
{
    GString *desc;

    desc = g_string_new (NULL);
    append_offset (desc, gweather_timezone_get_offset (zone));
    if (gweather_timezone_has_dst (zone)) {
	g_string_append (desc, " / ");
	append_offset (desc, gweather_timezone_get_dst_offset (zone));
    }
    return g_string_free (desc, FALSE);
}

static void
insert_location (GtkTreeStore *store, GWeatherTimezone *zone, const char *loc_name, GtkTreeIter *parent)
{
    GtkTreeIter iter;
    char *name, *offset;

    offset = get_offset (zone);
    name = g_strdup_printf ("%s <small>(%s)</small>",
                            loc_name ? loc_name : gweather_timezone_get_name (zone),
                            offset);
    gtk_tree_store_append (store, &iter, parent);
    gtk_tree_store_set (store, &iter,
                        GWEATHER_TIMEZONE_MENU_NAME, name,
                        GWEATHER_TIMEZONE_MENU_ZONE, zone,
                        -1);
    g_free (name);
    g_free (offset);
}

static void
insert_locations (GtkTreeStore *store, GWeatherLocation *loc)
{
    int i;

    if (gweather_location_get_level (loc) < GWEATHER_LOCATION_COUNTRY) {
	GWeatherLocation **children;

	children = gweather_location_get_children (loc);
	for (i = 0; children[i]; i++)
	    insert_locations (store, children[i]);
    } else {
	GWeatherTimezone **zones;
	GtkTreeIter iter;

	zones = gweather_location_get_timezones (loc);
	if (zones[1]) {
	    gtk_tree_store_append (store, &iter, NULL);
	    gtk_tree_store_set (store, &iter,
				GWEATHER_TIMEZONE_MENU_NAME, gweather_location_get_name (loc),
				-1);

	    for (i = 0; zones[i]; i++) {
                insert_location (store, zones[i], NULL, &iter);
	    }
	} else if (zones[0]) {
            insert_location (store, zones[0], gweather_location_get_name (loc), NULL);
	}

	gweather_location_free_timezones (loc, zones);
    }
}

static GtkTreeModel *
gweather_timezone_model_new (GWeatherLocation *top)
{
    GtkTreeStore *store;
    GtkTreeModel *model;
    GtkTreeIter iter;
    char *unknown;
    GWeatherTimezone *utc;

    store = gtk_tree_store_new (2, G_TYPE_STRING, GWEATHER_TYPE_TIMEZONE);
    model = GTK_TREE_MODEL (store);

    unknown = g_markup_printf_escaped ("<i>%s</i>", C_("timezone", "Unknown"));

    gtk_tree_store_append (store, &iter, NULL);
    gtk_tree_store_set (store, &iter,
			GWEATHER_TIMEZONE_MENU_NAME, unknown,
			GWEATHER_TIMEZONE_MENU_ZONE, NULL,
			-1);

    utc = gweather_timezone_get_utc ();
    if (utc) {
        insert_location (store, utc, NULL, NULL);
        gweather_timezone_unref (utc);
    }

    gtk_tree_store_append (store, &iter, NULL);

    g_free (unknown);

    if (!top)
	top = gweather_location_get_world ();

    insert_locations (store, top);

    return model;
}

static gboolean
row_separator_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    char *name;

    gtk_tree_model_get (model, iter,
			GWEATHER_TIMEZONE_MENU_NAME, &name,
			-1);
    if (name) {
	g_free (name);
	return FALSE;
    } else
	return TRUE;
}

static void
is_sensitive (GtkCellLayout *cell_layout, GtkCellRenderer *cell,
	      GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data)
{
    gboolean sensitive;

    sensitive = !gtk_tree_model_iter_has_child (tree_model, iter);
    g_object_set (cell, "sensitive", sensitive, NULL);
}

/**
 * gweather_timezone_menu_new:
 * @top: the top-level location for the menu.
 *
 * Creates a new #GWeatherTimezoneMenu.
 *
 * @top will normally be the location returned from
 * gweather_location_get_world(), but you can create a menu that
 * contains the timezones from a smaller set of locations if you want.
 *
 * Return value: the new #GWeatherTimezoneMenu
 **/
GtkWidget *
gweather_timezone_menu_new (GWeatherLocation *top)
{
    return g_object_new (GWEATHER_TYPE_TIMEZONE_MENU,
			 "top", top,
			 NULL);
}

typedef struct {
    GtkComboBox *combo;
    const char  *tzid;
} SetTimezoneData;

static gboolean
check_tzid (GtkTreeModel *model, GtkTreePath *path,
	    GtkTreeIter *iter, gpointer data)
{
    SetTimezoneData *tzd = data;
    GWeatherTimezone *zone;
    gboolean ok;

    gtk_tree_model_get (model, iter,
			GWEATHER_TIMEZONE_MENU_ZONE, &zone,
			-1);
    if (!zone)
	return FALSE;

    if (!strcmp (gweather_timezone_get_tzid (zone), tzd->tzid)) {
	gtk_combo_box_set_active_iter (tzd->combo, iter);
	ok = TRUE;
    } else
	ok = FALSE;

    gweather_timezone_unref (zone);
    return ok;
}

/**
 * gweather_timezone_menu_set_tzid:
 * @menu: a #GWeatherTimezoneMenu
 * @tzid: (allow-none): a tzdata id (eg, "America/New_York")
 *
 * Sets @menu to the given @tzid. If @tzid is %NULL, sets @menu to
 * "Unknown".
 **/
void
gweather_timezone_menu_set_tzid (GWeatherTimezoneMenu *menu,
				 const char           *tzid)
{
    SetTimezoneData tzd;

    g_return_if_fail (GWEATHER_IS_TIMEZONE_MENU (menu));

    if (!tzid) {
	gtk_combo_box_set_active (GTK_COMBO_BOX (menu), 0);
	return;
    }

    tzd.combo = GTK_COMBO_BOX (menu);
    tzd.tzid = tzid;
    gtk_tree_model_foreach (gtk_combo_box_get_model (tzd.combo),
			    check_tzid, &tzd);
}

/**
 * gweather_timezone_menu_get_tzid:
 * @menu: a #GWeatherTimezoneMenu
 *
 * Gets @menu's timezone id.
 *
 * Return value: (allow-none): @menu's tzid, or %NULL if no timezone
 * is selected.
 **/
const char *
gweather_timezone_menu_get_tzid (GWeatherTimezoneMenu *menu)
{
    g_return_val_if_fail (GWEATHER_IS_TIMEZONE_MENU (menu), NULL);

    if (!menu->zone)
	return NULL;
    return gweather_timezone_get_tzid (menu->zone);
}

