/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* location-entry.c - Location-selecting text entry
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
#include <geocode-glib/geocode-glib.h>
#include <gio/gio.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include "location-entry.h"
#include "weather-priv.h"

/**
 * SECTION:gweatherlocationentry
 * @Title: GWeatherLocationEntry
 *
 * A subclass of #GtkSearchEntry that provides autocompletion on
 * #GWeatherLocation<!-- -->s
 */

struct _GWeatherLocationEntryPrivate {
    GtkTreeModel     *filter_model;
    GWeatherLocation *location;
    GWeatherLocation *top;
    gboolean          custom_text;
    GCancellable     *cancellable;
    GtkTreeModel     *model;
};

G_DEFINE_TYPE (GWeatherLocationEntry, gweather_location_entry, GTK_TYPE_SEARCH_ENTRY)

enum {
    PROP_0,

    PROP_TOP,
    PROP_LOCATION,

    LAST_PROP
};

static void gweather_location_entry_build_model (GWeatherLocationEntry *entry,
						 GWeatherLocation *top);
static void set_property (GObject *object, guint prop_id,
			  const GValue *value, GParamSpec *pspec);
static void get_property (GObject *object, guint prop_id,
			  GValue *value, GParamSpec *pspec);

static void set_location_internal (GWeatherLocationEntry *entry,
				   GtkTreeModel          *model,
				   GtkTreeIter           *iter,
				   GWeatherLocation      *loc);
static GWeatherLocation *
create_new_detached_location (GWeatherLocation *nearest_station,
                              const char       *name,
                              gboolean          latlon_valid,
                              gdouble           latitude,
                              gdouble           longitude);

enum LOC
{
    LOC_GWEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME = 0,
    LOC_GWEATHER_LOCATION_ENTRY_COL_LOCATION,
    LOC_GWEATHER_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME,
    LOC_GWEATHER_LOCATION_ENTRY_COL_ENGLISH_COMPARE_NAME,
    LOC_GWEATHER_LOCATION_ENTRY_NUM_COLUMNS
};

enum PLACE
{
    PLACE_GWEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME = 0,
    PLACE_GWEATHER_LOCATION_ENTRY_COL_PLACE,
    PLACE_GWEATHER_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME
};

static gboolean matcher (GtkEntryCompletion *completion, const char *key,
			 GtkTreeIter *iter, gpointer user_data);
static gboolean match_selected (GtkEntryCompletion *completion,
				GtkTreeModel       *model,
				GtkTreeIter        *iter,
				gpointer            entry);
static void     entry_changed (GWeatherLocationEntry *entry);
static void _no_matches (GtkEntryCompletion *completion, GWeatherLocationEntry *entry);

static void
hack_filter_model_data_func (GtkCellLayout   *layout,
			     GtkCellRenderer *renderer,
			     GtkTreeModel    *model,
			     GtkTreeIter     *iter,
			     gpointer         data)
{
    GWeatherLocationEntry *self;
    GWeatherLocationEntryPrivate *priv;

    self = GWEATHER_LOCATION_ENTRY (gtk_entry_completion_get_entry (GTK_ENTRY_COMPLETION (layout)));
    priv = self->priv;

    if (priv->filter_model == model)
      return;

    if (priv->filter_model)
      g_object_remove_weak_pointer (G_OBJECT (priv->filter_model), (gpointer *)&priv->filter_model);

    priv->filter_model = model;

    if (priv->filter_model)
      g_object_add_weak_pointer (G_OBJECT (priv->filter_model), (gpointer *)&priv->filter_model);
}

static void
gweather_location_entry_init (GWeatherLocationEntry *entry)
{
    GtkEntryCompletion *completion;
    GWeatherLocationEntryPrivate *priv;

    priv = entry->priv = G_TYPE_INSTANCE_GET_PRIVATE (entry, GWEATHER_TYPE_LOCATION_ENTRY, GWeatherLocationEntryPrivate);

    completion = gtk_entry_completion_new ();

    gtk_entry_completion_set_popup_set_width (completion, FALSE);
    gtk_entry_completion_set_text_column (completion, LOC_GWEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME);
    gtk_entry_completion_set_match_func (completion, matcher, NULL, NULL);

    /* Giant Hack!
       We want to grab the filter model used internally by GtkEntryCompletion.
       We know that it is "leaked" by a few functions and signals, such
       as the CellDataFunc in GtkCellLayout. So we set that in a way that
       when it gets called, it sets the filter model where we can access it.
    */
    {
	GList *renderers;

	renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (completion));
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (completion),
					    renderers->data,
					    hack_filter_model_data_func, NULL, NULL);

	g_list_free (renderers);
    }

    g_signal_connect (completion, "match-selected",
		      G_CALLBACK (match_selected), entry);

    g_signal_connect (completion, "no-matches",
                      G_CALLBACK (_no_matches), entry);

    gtk_entry_set_completion (GTK_ENTRY (entry), completion);
    g_object_unref (completion);

    priv->custom_text = FALSE;
    g_signal_connect (entry, "changed",
		      G_CALLBACK (entry_changed), NULL);
}

static void
finalize (GObject *object)
{
    GWeatherLocationEntry *entry;
    GWeatherLocationEntryPrivate *priv;

    entry = GWEATHER_LOCATION_ENTRY (object);
    priv = entry->priv;

    if (priv->location)
	gweather_location_unref (priv->location);
    if (priv->top)
	gweather_location_unref (priv->top);
    if (priv->model)
        g_object_unref (priv->model);

    if (priv->filter_model)
      g_object_remove_weak_pointer (G_OBJECT (priv->filter_model), (gpointer *)&priv->filter_model);

    G_OBJECT_CLASS (gweather_location_entry_parent_class)->finalize (object);
}

static void
dispose (GObject *object)
{
    GWeatherLocationEntry *entry;
    GWeatherLocationEntryPrivate *priv;

    entry = GWEATHER_LOCATION_ENTRY (object);
    priv = entry->priv;

    if (priv->cancellable) {
        g_cancellable_cancel (priv->cancellable);
        g_object_unref (priv->cancellable);
        priv->cancellable = NULL;
    }

    G_OBJECT_CLASS (gweather_location_entry_parent_class)->dispose (object);
}

static void
gweather_location_entry_activate (GtkEntry *entry)
{
    GWeatherLocationEntry *self;
    GWeatherLocationEntryPrivate *priv;
    GtkEntryCompletion *completion;

    self = GWEATHER_LOCATION_ENTRY (entry);
    priv = self->priv;

    completion = gtk_entry_get_completion (entry);
    gtk_entry_completion_complete (completion);

    if (priv->custom_text &&
        priv->filter_model &&
	gtk_tree_model_iter_n_children (priv->filter_model, NULL) == 1) {
	GtkTreeIter iter, real_iter;

	gtk_tree_model_get_iter_first (priv->filter_model, &iter);
	gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (priv->filter_model),
							  &real_iter, &iter);

	set_location_internal (self, gtk_entry_completion_get_model (completion),
			       &real_iter, NULL);
    }

    GTK_ENTRY_CLASS (gweather_location_entry_parent_class)->activate (entry);
}

static void
gweather_location_entry_class_init (GWeatherLocationEntryClass *location_entry_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (location_entry_class);
    GtkEntryClass *entry_class = GTK_ENTRY_CLASS (location_entry_class);

    object_class->finalize = finalize;
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->dispose = dispose;

    entry_class->activate = gweather_location_entry_activate;

    /* properties */
    g_object_class_install_property (
	object_class, PROP_TOP,
	g_param_spec_boxed ("top",
			    "Top Location",
			    "The GWeatherLocation whose children will be used to fill in the entry",
			    GWEATHER_TYPE_LOCATION,
			    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (
	object_class, PROP_LOCATION,
	g_param_spec_boxed ("location",
			    "Location",
			    "The selected GWeatherLocation",
			    GWEATHER_TYPE_LOCATION,
			    G_PARAM_READWRITE));

    g_type_class_add_private (location_entry_class, sizeof (GWeatherLocationEntryPrivate));
}

static void
set_property (GObject *object, guint prop_id,
	      const GValue *value, GParamSpec *pspec)
{
    switch (prop_id) {
    case PROP_TOP:
	gweather_location_entry_build_model (GWEATHER_LOCATION_ENTRY (object),
					     g_value_get_boxed (value));
	break;
    case PROP_LOCATION:
	gweather_location_entry_set_location (GWEATHER_LOCATION_ENTRY (object),
					      g_value_get_boxed (value));
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
    GWeatherLocationEntry *entry = GWEATHER_LOCATION_ENTRY (object);

    switch (prop_id) {
    case PROP_LOCATION:
	g_value_set_boxed (value, entry->priv->location);
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
    }
}

static void
entry_changed (GWeatherLocationEntry *entry)
{
    GtkEntryCompletion *completion;
    const gchar *text;

    completion = gtk_entry_get_completion (GTK_ENTRY (entry));

    if (entry->priv->cancellable) {
        g_cancellable_cancel (entry->priv->cancellable);
        g_object_unref (entry->priv->cancellable);
        entry->priv->cancellable = NULL;
        gtk_entry_completion_delete_action (completion, 0);
    }

    gtk_entry_completion_set_match_func (gtk_entry_get_completion (GTK_ENTRY (entry)), matcher, NULL, NULL);
    gtk_entry_completion_set_model (gtk_entry_get_completion (GTK_ENTRY (entry)), entry->priv->model);

    text = gtk_entry_get_text (GTK_ENTRY (entry));

    if (text && *text)
	entry->priv->custom_text = TRUE;
    else
	set_location_internal (entry, NULL, NULL, NULL);
}

static void
set_location_internal (GWeatherLocationEntry *entry,
		       GtkTreeModel          *model,
		       GtkTreeIter           *iter,
		       GWeatherLocation      *loc)
{
    GWeatherLocationEntryPrivate *priv;
    char *name;

    priv = entry->priv;

    if (priv->location)
	gweather_location_unref (priv->location);

    g_assert (iter == NULL || loc == NULL);

    if (iter) {
	gtk_tree_model_get (model, iter,
			    LOC_GWEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME, &name,
			    LOC_GWEATHER_LOCATION_ENTRY_COL_LOCATION, &priv->location,
			    -1);
	gtk_entry_set_text (GTK_ENTRY (entry), name);
	priv->custom_text = FALSE;
	g_free (name);
    } else if (loc) {
	priv->location = gweather_location_ref (loc);
	gtk_entry_set_text (GTK_ENTRY (entry), loc->local_name);
	priv->custom_text = FALSE;
    } else {
	priv->location = NULL;
	gtk_entry_set_text (GTK_ENTRY (entry), "");
	priv->custom_text = TRUE;
    }

    gtk_editable_set_position (GTK_EDITABLE (entry), -1);
    g_object_notify (G_OBJECT (entry), "location");
}

/**
 * gweather_location_entry_set_location:
 * @entry: a #GWeatherLocationEntry
 * @loc: (allow-none): a #GWeatherLocation in @entry, or %NULL to
 * clear @entry
 *
 * Sets @entry's location to @loc, and updates the text of the
 * entry accordingly.
 * Note that if the database contains a location that compares
 * equal to @loc, that will be chosen in place of @loc.
 **/
void
gweather_location_entry_set_location (GWeatherLocationEntry *entry,
				      GWeatherLocation      *loc)
{
    GtkEntryCompletion *completion;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GWeatherLocation *cmploc;

    g_return_if_fail (GWEATHER_IS_LOCATION_ENTRY (entry));

    completion = gtk_entry_get_completion (GTK_ENTRY (entry));
    model = gtk_entry_completion_get_model (completion);

    if (loc == NULL) {
	set_location_internal (entry, model, NULL, NULL);
	return;
    }

    gtk_tree_model_get_iter_first (model, &iter);
    do {
	gtk_tree_model_get (model, &iter,
			    LOC_GWEATHER_LOCATION_ENTRY_COL_LOCATION, &cmploc,
			    -1);
	if (gweather_location_equal (loc, cmploc)) {
	    set_location_internal (entry, model, &iter, NULL);
	    gweather_location_unref (cmploc);
	    return;
	}

	gweather_location_unref (cmploc);
    } while (gtk_tree_model_iter_next (model, &iter));

    set_location_internal (entry, model, NULL, loc);
}

/**
 * gweather_location_entry_get_location:
 * @entry: a #GWeatherLocationEntry
 *
 * Gets the location that was set by a previous call to
 * gweather_location_entry_set_location() or was selected by the user.
 *
 * Return value: (transfer full) (allow-none): the selected location
 * (which you must unref when you are done with it), or %NULL if no
 * location is selected.
 **/
GWeatherLocation *
gweather_location_entry_get_location (GWeatherLocationEntry *entry)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION_ENTRY (entry), NULL);

    if (entry->priv->location)
	return gweather_location_ref (entry->priv->location);
    else
	return NULL;
}

/**
 * gweather_location_entry_has_custom_text:
 * @entry: a #GWeatherLocationEntry
 *
 * Checks whether or not @entry's text has been modified by the user.
 * Note that this does not mean that no location is associated with @entry.
 * gweather_location_entry_get_location() should be used for this.
 *
 * Return value: %TRUE if @entry's text was modified by the user, or %FALSE if
 * it's set to the default text of a location.
 **/
gboolean
gweather_location_entry_has_custom_text (GWeatherLocationEntry *entry)
{
    g_return_val_if_fail (GWEATHER_IS_LOCATION_ENTRY (entry), FALSE);

    return entry->priv->custom_text;
}

/**
 * gweather_location_entry_set_city:
 * @entry: a #GWeatherLocationEntry
 * @city_name: (allow-none): the city name, or %NULL
 * @code: the METAR station code
 *
 * Sets @entry's location to a city with the given @code, and given
 * @city_name, if non-%NULL. If there is no matching city, sets
 * @entry's location to %NULL.
 *
 * Return value: %TRUE if @entry's location could be set to a matching city,
 * %FALSE otherwise.
 **/
gboolean
gweather_location_entry_set_city (GWeatherLocationEntry *entry,
				  const char            *city_name,
				  const char            *code)
{
    GtkEntryCompletion *completion;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GWeatherLocation *cmploc;
    const char *cmpcode;
    char *cmpname;

    g_return_val_if_fail (GWEATHER_IS_LOCATION_ENTRY (entry), FALSE);
    g_return_val_if_fail (code != NULL, FALSE);

    completion = gtk_entry_get_completion (GTK_ENTRY (entry));
    model = gtk_entry_completion_get_model (completion);

    gtk_tree_model_get_iter_first (model, &iter);
    do {
	gtk_tree_model_get (model, &iter,
			    LOC_GWEATHER_LOCATION_ENTRY_COL_LOCATION, &cmploc,
			    -1);

	cmpcode = gweather_location_get_code (cmploc);
	if (!cmpcode || strcmp (cmpcode, code) != 0) {
	    gweather_location_unref (cmploc);
	    continue;
	}

	if (city_name) {
	    cmpname = gweather_location_get_city_name (cmploc);
	    if (!cmpname || strcmp (cmpname, city_name) != 0) {
		gweather_location_unref (cmploc);
		g_free (cmpname);
		continue;
	    }
	    g_free (cmpname);
	}

	set_location_internal (entry, model, &iter, NULL);
	gweather_location_unref (cmploc);
	return TRUE;
    } while (gtk_tree_model_iter_next (model, &iter));

    set_location_internal (entry, model, NULL, NULL);

    return FALSE;
}

static void
fill_location_entry_model (GtkTreeStore *store, GWeatherLocation *loc,
			   const char *parent_display_name,
			   const char *parent_compare_local_name,
			   const char *parent_compare_english_name)
{
    GWeatherLocation **children;
    char *display_name, *local_compare_name, *english_compare_name;
    GtkTreeIter iter;
    int i;

    children = gweather_location_get_children (loc);

    switch (gweather_location_get_level (loc)) {
    case GWEATHER_LOCATION_WORLD:
    case GWEATHER_LOCATION_REGION:
    case GWEATHER_LOCATION_ADM2:
	/* Ignore these levels of hierarchy; just recurse, passing on
	 * the names from the parent node.
	 */
	for (i = 0; children[i]; i++) {
	    fill_location_entry_model (store, children[i],
				       parent_display_name,
				       parent_compare_local_name,
				       parent_compare_english_name);
	}
	break;

    case GWEATHER_LOCATION_COUNTRY:
	/* Recurse, initializing the names to the country name */
	for (i = 0; children[i]; i++) {
	    fill_location_entry_model (store, children[i],
				       loc->local_name,
				       loc->local_sort_name,
				       loc->english_sort_name);
	}
	break;

    case GWEATHER_LOCATION_ADM1:
	/* Recurse, adding the ADM1 name to the country name */
	display_name = g_strdup_printf ("%s, %s", loc->local_name, parent_display_name);
	local_compare_name = g_strdup_printf ("%s, %s", loc->local_sort_name, parent_compare_local_name);
	english_compare_name = g_strdup_printf ("%s, %s", loc->english_sort_name, parent_compare_english_name);

	for (i = 0; children[i]; i++) {
	    fill_location_entry_model (store, children[i],
				       display_name, local_compare_name, english_compare_name);
	}

	g_free (display_name);
	g_free (local_compare_name);
	g_free (english_compare_name);
	break;

    case GWEATHER_LOCATION_CITY:
	/* If there are multiple (<location>) children, we use the one
	 * closest to the city center.
	 *
	 * Locations are already sorted by increasing distance from
	 * the city.
	 */
    case GWEATHER_LOCATION_WEATHER_STATION:
	/* <location> with no parent <city> */
	display_name = g_strdup_printf ("%s, %s",
					loc->local_name, parent_display_name);
	local_compare_name = g_strdup_printf ("%s, %s",
					      loc->local_sort_name, parent_compare_local_name);
	english_compare_name = g_strdup_printf ("%s, %s",
						loc->english_sort_name, parent_compare_english_name);

	gtk_tree_store_append (store, &iter, NULL);
	gtk_tree_store_set (store, &iter,
			    LOC_GWEATHER_LOCATION_ENTRY_COL_LOCATION, loc,
			    LOC_GWEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME, display_name,
			    LOC_GWEATHER_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME, local_compare_name,
			    LOC_GWEATHER_LOCATION_ENTRY_COL_ENGLISH_COMPARE_NAME, english_compare_name,
			    -1);

	g_free (display_name);
	g_free (local_compare_name);
	g_free (english_compare_name);
	break;

    case GWEATHER_LOCATION_DETACHED:
	g_assert_not_reached ();
    }
}

static void
gweather_location_entry_build_model (GWeatherLocationEntry *entry,
				     GWeatherLocation *top)
{
    GtkTreeStore *store = NULL;
    GtkEntryCompletion *completion;

    if (top)
	entry->priv->top = gweather_location_ref (top);
    else
	entry->priv->top = gweather_location_ref (gweather_location_get_world ());

    store = gtk_tree_store_new (4, G_TYPE_STRING, GWEATHER_TYPE_LOCATION, G_TYPE_STRING, G_TYPE_STRING);
    fill_location_entry_model (store, entry->priv->top, NULL, NULL, NULL);

    entry->priv->model = GTK_TREE_MODEL (store);
    completion = gtk_entry_get_completion (GTK_ENTRY (entry));
    gtk_entry_completion_set_match_func (completion, matcher, NULL, NULL);
    gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
}

static char *
find_word (const char *full_name, const char *word, int word_len,
	   gboolean whole_word, gboolean is_first_word)
{
    char *p;

    if (word == NULL || *word == '\0')
        return NULL;

    p = (char *)full_name - 1;
    while ((p = strchr (p + 1, *word))) {
	if (strncmp (p, word, word_len) != 0)
	    continue;

	if (p > (char *)full_name) {
	    char *prev = g_utf8_prev_char (p);

	    /* Make sure p points to the start of a word */
	    if (g_unichar_isalpha (g_utf8_get_char (prev)))
		continue;

	    /* If we're matching the first word of the key, it has to
	     * match the first word of the location, city, state, or
	     * country. Eg, it either matches the start of the string
	     * (which we already know it doesn't at this point) or
	     * it is preceded by the string ", " (which isn't actually
	     * a perfect test. FIXME)
	     */
	    if (is_first_word) {
		if (prev == (char *)full_name || strncmp (prev - 1, ", ", 2) != 0)
		    continue;
	    }
	}

	if (whole_word && g_unichar_isalpha (g_utf8_get_char (p + word_len)))
	    continue;

	return p;
    }
    return NULL;
}

static gboolean
match_compare_name (const char *key, const char *name)
{
    gboolean is_first_word = TRUE;
    int len;

    /* All but the last word in KEY must match a full word from NAME,
     * in order (but possibly skipping some words from NAME).
     */
    len = strcspn (key, " ");
    while (key[len]) {
	name = find_word (name, key, len, TRUE, is_first_word);
	if (!name)
	    return FALSE;

	key += len;
	while (*key && !g_unichar_isalpha (g_utf8_get_char (key)))
	    key = g_utf8_next_char (key);
	while (*name && !g_unichar_isalpha (g_utf8_get_char (name)))
	    name = g_utf8_next_char (name);

	len = strcspn (key, " ");
	is_first_word = FALSE;
    }

    /* The last word in KEY must match a prefix of a following word in NAME */
    return find_word (name, key, strlen (key), FALSE, is_first_word) != NULL;
}

static gboolean
matcher (GtkEntryCompletion *completion, const char *key,
	 GtkTreeIter *iter, gpointer user_data)
{
    char *local_compare_name, *english_compare_name;
    gboolean match;

    gtk_tree_model_get (gtk_entry_completion_get_model (completion), iter,
			LOC_GWEATHER_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME, &local_compare_name,
			LOC_GWEATHER_LOCATION_ENTRY_COL_ENGLISH_COMPARE_NAME, &english_compare_name,
			-1);
    match = match_compare_name (key, local_compare_name) || match_compare_name (key, english_compare_name);

    g_free (local_compare_name);
    g_free (english_compare_name);
    return match;
}

static gboolean
match_selected (GtkEntryCompletion *completion,
		GtkTreeModel       *model,
		GtkTreeIter        *iter,
		gpointer            entry)
{
    if (model != ((GWeatherLocationEntry *)entry)->priv->model) {
        GeocodePlace *place;
        gtk_tree_model_get (model, iter,
                            PLACE_GWEATHER_LOCATION_ENTRY_COL_PLACE, &place,
                            -1);
        GeocodeLocation *loc = geocode_place_get_location (place);

        GWeatherLocation *location;
        location = gweather_location_find_nearest_city (NULL, geocode_location_get_latitude (loc), geocode_location_get_longitude (loc));

        location = create_new_detached_location(location, geocode_location_get_description (loc), TRUE,
                                                geocode_location_get_latitude (loc) * M_PI / 180.0,
                                                geocode_location_get_longitude (loc) * M_PI / 180.0);

        set_location_internal (entry, model, NULL, location);
    } else {
        set_location_internal (entry, model, iter, NULL);
    }
    return TRUE;
}

static gboolean
new_matcher (GtkEntryCompletion *completion, const char *key,
             GtkTreeIter        *iter,       gpointer    user_data)
{
    return TRUE;
}

static void
fill_store (gpointer data, gpointer user_data)
{
    GeocodePlace *place = GEOCODE_PLACE (data);
    GeocodeLocation *loc = geocode_place_get_location (place);
    GtkTreeIter iter;

    gtk_tree_store_append (user_data, &iter, NULL);
    gtk_tree_store_set (user_data, &iter,
                        PLACE_GWEATHER_LOCATION_ENTRY_COL_PLACE, place,
                        PLACE_GWEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME, geocode_location_get_description (loc),
                        PLACE_GWEATHER_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME, geocode_location_get_description (loc),
                        -1);
}

static void
_got_places (GObject      *source_object,
             GAsyncResult *result,
             gpointer      user_data)
{
    GList *places;
    GWeatherLocationEntry *self = user_data;
    GError *error = NULL;
    GtkTreeStore *store = NULL;
    GtkEntryCompletion *completion;

    places = geocode_forward_search_finish (GEOCODE_FORWARD (source_object), result, &error);
    if (places == NULL) {
        /* return without touching anything if cancelled (the entry might have been disposed) */
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_clear_error (&error);
            return;
        }

        g_clear_error (&error);
        completion = gtk_entry_get_completion (user_data);
        gtk_entry_completion_set_match_func (completion, matcher, NULL, NULL);
        gtk_entry_completion_set_model (completion, self->priv->model);
        goto out;
    }

    completion = gtk_entry_get_completion (user_data);
    store = gtk_tree_store_new (4, G_TYPE_STRING, GEOCODE_TYPE_PLACE, G_TYPE_STRING, G_TYPE_STRING);
    g_list_foreach (places, fill_store, store);
    gtk_entry_completion_set_match_func (completion, new_matcher, NULL, NULL);
    gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
    g_object_unref (store);

 out:
    gtk_entry_completion_delete_action (completion, 0);
    g_clear_object (&self->priv->cancellable);
}

static void
_no_matches (GtkEntryCompletion *completion, GWeatherLocationEntry *entry) {
    const gchar *key = gtk_entry_get_text(GTK_ENTRY (entry));
    GeocodeForward *forward;

    if (entry->priv->cancellable) {
        g_cancellable_cancel (entry->priv->cancellable);
        g_object_unref (entry->priv->cancellable);
        entry->priv->cancellable = NULL;
    } else {
        gtk_entry_completion_insert_action_text (completion, 0, "Loading...");
    }

    entry->priv->cancellable = g_cancellable_new ();

    forward = geocode_forward_new_for_string(key);
    geocode_forward_search_async (forward, entry->priv->cancellable, _got_places, entry);
}

/**
 * gweather_location_entry_new:
 * @top: the top-level location for the entry.
 *
 * Creates a new #GWeatherLocationEntry.
 *
 * @top will normally be the location returned from
 * gweather_location_get_world(), but you can create an entry that
 * only accepts a smaller set of locations if you want.
 *
 * Return value: the new #GWeatherLocationEntry
 **/
GtkWidget *
gweather_location_entry_new (GWeatherLocation *top)
{
    return g_object_new (GWEATHER_TYPE_LOCATION_ENTRY,
			 "top", top,
			 NULL);
}

static GWeatherLocation *
create_new_detached_location (GWeatherLocation *nearest_station,
                              const char       *name,
                              gboolean          latlon_valid,
                              gdouble           latitude,
                              gdouble           longitude)
{
    GWeatherLocation *self;
    char *normalized;

    self = g_slice_new0 (GWeatherLocation);
    self->ref_count = 1;
    self->level = GWEATHER_LOCATION_DETACHED;
    self->english_name = g_strdup (name);
    self->local_name = g_strdup (name);

    normalized = g_utf8_normalize (name, -1, G_NORMALIZE_ALL);
    self->english_sort_name = g_utf8_casefold (normalized, -1);
    self->local_sort_name = g_strdup (self->english_sort_name);
    g_free (normalized);

    self->parent = nearest_station;
    self->children = NULL;

    if (nearest_station)
	self->station_code = g_strdup (nearest_station->station_code);

    g_assert (nearest_station || latlon_valid);

    if (latlon_valid) {
	self->latlon_valid = TRUE;
	self->latitude = latitude;
	self->longitude = longitude;
    } else {
	self->latlon_valid = nearest_station->latlon_valid;
	self->latitude = nearest_station->latitude;
	self->longitude = nearest_station->longitude;
    }

    return self;
}
