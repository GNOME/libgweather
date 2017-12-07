
#include <gweather-version.h>
#include "gweather-location-entry.h"
#include "gweather-timezone-menu.h"

static void
deleted (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    gtk_main_quit ();
}

static void
location_changed (GObject *object, GParamSpec *param, gpointer tzmenu)
{
    GWeatherLocationEntry *entry = GWEATHER_LOCATION_ENTRY (object);
    GWeatherLocation *loc;
    GWeatherTimezone *zone;

    loc = gweather_location_entry_get_location (entry);
    if (loc == NULL)
      return;

    zone = gweather_location_get_timezone (loc);
    if (zone)
	gweather_timezone_menu_set_tzid (tzmenu, gweather_timezone_get_tzid (zone));
    else
	gweather_timezone_menu_set_tzid (tzmenu, NULL);
    if (zone)
	gweather_timezone_unref (zone);
    gweather_location_unref (loc);
}

int
main (int argc, char **argv)
{
    GtkWidget *window, *vbox, *entry;
    GtkWidget *combo;
    gtk_init (&argc, &argv);

    g_setenv ("LIBGWEATHER_LOCATIONS_PATH",
              TEST_SRCDIR "../data/Locations.xml",
              FALSE);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "location");
    gtk_container_set_border_width (GTK_CONTAINER (window), 8);
    g_signal_connect (window, "delete-event",
		      G_CALLBACK (deleted), NULL);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    entry = gweather_location_entry_new (NULL);
    gtk_widget_set_size_request (entry, 400, -1);
    gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, TRUE, 0);

    combo = gweather_timezone_menu_new (NULL);
    gtk_box_pack_start (GTK_BOX (vbox), combo, FALSE, TRUE, 0);

    g_signal_connect (entry, "notify::location",
		      G_CALLBACK (location_changed), combo);

    gtk_widget_show_all (window);

    gtk_main ();

    return 0;
}
