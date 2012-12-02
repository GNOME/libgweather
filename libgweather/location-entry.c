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

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include "location-entry.h"
#include "weather-priv.h"

/**
 * SECTION:gweatherlocationentry
 * @Title: GWeatherLocationEntry
 *
 * A subclass of #GtkEntry that provides autocompletion on
 * #GWeatherLocation<!-- -->s
 */

G_DEFINE_TYPE (GWeatherLocationEntry, gweather_location_entry, GTK_TYPE_ENTRY)

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

enum
{
    GWEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME = 0,
    GWEATHER_LOCATION_ENTRY_COL_LOCATION,
    GWEATHER_LOCATION_ENTRY_COL_COMPARE_NAME,
    GWEATHER_LOCATION_ENTRY_COL_SORT_NAME,
    GWEATHER_LOCATION_ENTRY_NUM_COLUMNS
};

static gboolean matcher (GtkEntryCompletion *completion, const char *key,
			 GtkTreeIter *iter, gpointer user_data);
static gboolean match_selected (GtkEntryCompletion *completion,
				GtkTreeModel       *model,
				GtkTreeIter        *iter,
				gpointer            entry);
static void     entry_changed (GWeatherLocationEntry *entry);

static void
gweather_location_entry_init (GWeatherLocationEntry *entry)
{
    GtkEntryCompletion *completion;

    completion = gtk_entry_completion_new ();

    gtk_entry_completion_set_popup_set_width (completion, FALSE);
    gtk_entry_completion_set_text_column (completion, GWEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME);
    gtk_entry_completion_set_match_func (completion, matcher, NULL, NULL);

    g_signal_connect (completion, "match_selected",
		      G_CALLBACK (match_selected), entry);

    gtk_entry_set_completion (GTK_ENTRY (entry), completion);
    g_object_unref (completion);

    entry->custom_text = FALSE;
    g_signal_connect (entry, "changed",
		      G_CALLBACK (entry_changed), NULL);
}

static void
finalize (GObject *object)
{
    GWeatherLocationEntry *entry = GWEATHER_LOCATION_ENTRY (object);

    if (entry->location)
	gweather_location_unref (entry->location);
    if (entry->top)
	gweather_location_unref (entry->top);

    G_OBJECT_CLASS (gweather_location_entry_parent_class)->finalize (object);
}

static void
gweather_location_entry_class_init (GWeatherLocationEntryClass *location_entry_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (location_entry_class);

    object_class->finalize = finalize;
    object_class->set_property = set_property;
    object_class->get_property = get_property;

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
	g_value_set_boxed (value, entry->location);
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
    }
}

static void
entry_changed (GWeatherLocationEntry *entry)
{
    entry->custom_text = TRUE;
}

static void
set_location_internal (GWeatherLocationEntry *entry,
		       GtkTreeModel          *model,
		       GtkTreeIter           *iter,
		       GWeatherLocation      *loc)
{
    char *name;

    if (entry->location)
	gweather_location_unref (entry->location);

    g_assert (iter == NULL || loc == NULL);

    if (iter) {
	gtk_tree_model_get (model, iter,
			    GWEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME, &name,
			    GWEATHER_LOCATION_ENTRY_COL_LOCATION, &loc,
			    -1);
	entry->location = gweather_location_ref (loc);
	gtk_entry_set_text (GTK_ENTRY (entry), name);
	entry->custom_text = FALSE;
	g_free (name);
    } else if (loc) {
	entry->location = gweather_location_ref (loc);
	gtk_entry_set_text (GTK_ENTRY (entry), loc->name);
	entry->custom_text = TRUE;
    } else {
	entry->location = NULL;
	gtk_entry_set_text (GTK_ENTRY (entry), "");
	entry->custom_text = TRUE;
    }

    gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
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

    gtk_tree_model_get_iter_first (model, &iter);
    do {
	gtk_tree_model_get (model, &iter,
			    GWEATHER_LOCATION_ENTRY_COL_LOCATION, &cmploc,
			    -1);
	if (gweather_location_equal (loc, cmploc)) {
	    set_location_internal (entry, model, &iter, NULL);
	    return;
	}
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

    if (entry->location)
	return gweather_location_ref (entry->location);
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

    return entry->custom_text;
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
			    GWEATHER_LOCATION_ENTRY_COL_LOCATION, &cmploc,
			    -1);

	cmpcode = gweather_location_get_code (cmploc);
	if (!cmpcode || strcmp (cmpcode, code) != 0)
	    continue;

	if (city_name) {
	    cmpname = gweather_location_get_city_name (cmploc);
	    if (!cmpname || strcmp (cmpname, city_name) != 0) {
		g_free (cmpname);
		continue;
	    }
	    g_free (cmpname);
	}

	set_location_internal (entry, model, &iter, NULL);
	return TRUE;
    } while (gtk_tree_model_iter_next (model, &iter));

    set_location_internal (entry, model, NULL, NULL);

    return FALSE;
}

static void
fill_location_entry_model (GtkTreeStore *store, GWeatherLocation *loc,
			   const char *parent_display_name,
			   const char *parent_compare_name)
{
    GWeatherLocation **children;
    char *display_name, *compare_name;
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
				       parent_compare_name);
	}
	break;

    case GWEATHER_LOCATION_COUNTRY:
	/* Recurse, initializing the names to the country name */
	for (i = 0; children[i]; i++) {
	    fill_location_entry_model (store, children[i],
				       gweather_location_get_name (loc),
				       gweather_location_get_sort_name (loc));
	}
	break;

    case GWEATHER_LOCATION_ADM1:
	/* Recurse, adding the ADM1 name to the country name */
	display_name = g_strdup_printf ("%s, %s", gweather_location_get_name (loc), parent_display_name);
	compare_name = g_strdup_printf ("%s, %s", gweather_location_get_sort_name (loc), parent_compare_name);

	for (i = 0; children[i]; i++) {
	    fill_location_entry_model (store, children[i],
				       display_name, compare_name);
	}

	g_free (display_name);
	g_free (compare_name);
	break;

    case GWEATHER_LOCATION_CITY:
	if (children[0] && children[1]) {
	    /* If there are multiple (<location>) children, add a line
	     * for each of them.
	     */
	    for (i = 0; children[i]; i++) {
		display_name = g_strdup_printf ("%s (%s), %s",
						gweather_location_get_name (loc),
						gweather_location_get_name (children[i]),
						parent_display_name);
		compare_name = g_strdup_printf ("%s (%s), %s",
						gweather_location_get_sort_name (loc),
						gweather_location_get_sort_name (children[i]),
						parent_compare_name);

		gtk_tree_store_append (store, &iter, NULL);
		gtk_tree_store_set (store, &iter,
				    GWEATHER_LOCATION_ENTRY_COL_LOCATION, children[i],
				    GWEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME, display_name,
				    GWEATHER_LOCATION_ENTRY_COL_COMPARE_NAME, compare_name,
				    -1);

		g_free (display_name);
		g_free (compare_name);
	    }

	    break;
	}

	/* fall through */

    case GWEATHER_LOCATION_WEATHER_STATION:
	/* <location> with no parent <city>, or <city> with a single
	 * child <location>.
	 */
	display_name = g_strdup_printf ("%s, %s",
					gweather_location_get_name (loc),
					parent_display_name);
	compare_name = g_strdup_printf ("%s, %s",
					gweather_location_get_sort_name (loc),
					parent_compare_name);

	gtk_tree_store_append (store, &iter, NULL);
	gtk_tree_store_set (store, &iter,
			    GWEATHER_LOCATION_ENTRY_COL_LOCATION, loc,
			    GWEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME, display_name,
			    GWEATHER_LOCATION_ENTRY_COL_COMPARE_NAME, compare_name,
			    -1);

	g_free (display_name);
	g_free (compare_name);
	break;

    case GWEATHER_LOCATION_DETACHED:
	g_assert_not_reached ();
    }

    gweather_location_free_children (loc, children);
}

static void
gweather_location_entry_build_model (GWeatherLocationEntry *entry,
				     GWeatherLocation *top)
{
    GtkTreeStore *store = NULL;

    entry->top = gweather_location_ref (top);

    store = gtk_tree_store_new (4, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING);
    fill_location_entry_model (store, top, NULL, NULL);
    gtk_entry_completion_set_model (gtk_entry_get_completion (GTK_ENTRY (entry)),
				    GTK_TREE_MODEL (store));
    g_object_unref (store);
}

static char *
find_word (const char *full_name, const char *word, int word_len,
	   gboolean whole_word, gboolean is_first_word)
{
    char *p = (char *)full_name - 1;

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
matcher (GtkEntryCompletion *completion, const char *key,
	 GtkTreeIter *iter, gpointer user_data)
{
    char *name, *name_mem;
    GWeatherLocation *loc;
    gboolean is_first_word = TRUE, match;
    int len;

    gtk_tree_model_get (gtk_entry_completion_get_model (completion), iter,
			GWEATHER_LOCATION_ENTRY_COL_COMPARE_NAME, &name_mem,
			GWEATHER_LOCATION_ENTRY_COL_LOCATION, &loc,
			-1);
    name = name_mem;

    if (!loc) {
	g_free (name_mem);
	return FALSE;
    }

    /* All but the last word in KEY must match a full word from NAME,
     * in order (but possibly skipping some words from NAME).
     */
    len = strcspn (key, " ");
    while (key[len]) {
	name = find_word (name, key, len, TRUE, is_first_word);
	if (!name) {
	    g_free (name_mem);
	    return FALSE;
	}

	key += len;
	while (*key && !g_unichar_isalpha (g_utf8_get_char (key)))
	    key = g_utf8_next_char (key);
	while (*name && !g_unichar_isalpha (g_utf8_get_char (name)))
	    name = g_utf8_next_char (name);

	len = strcspn (key, " ");
	is_first_word = FALSE;
    }

    /* The last word in KEY must match a prefix of a following word in NAME */
    match = find_word (name, key, strlen (key), FALSE, is_first_word) != NULL;
    g_free (name_mem);
    return match;
}

static gboolean
match_selected (GtkEntryCompletion *completion,
		GtkTreeModel       *model,
		GtkTreeIter        *iter,
		gpointer            entry)
{
    set_location_internal (entry, model, iter, NULL);
    return TRUE;
}

/**
 * gweather_location_entry_new:
 * @top: the top-level location for the entry.
 *
 * Creates a new #GWeatherLocationEntry.
 *
 * @top will normally be a location returned from
 * gweather_location_new_world(), but you can create an entry that
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
