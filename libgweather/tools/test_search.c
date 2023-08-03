#include <libgweather/gweather.h>

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GWeatherSearch) search = gweather_search_get_world ();
  g_autoptr(GArray) terms = g_array_new (TRUE, FALSE, sizeof (char*));
  g_autoptr(GListModel) model = NULL;
  g_autoptr(GTimer) timer = NULL;
  guint n_items;
  guint elapsed;

  if (argc < 2)
    {
      g_printerr ("usage: %s TERM...\n", argv[0]);
      return 1;
    }

  for (int i = 1; i < argc; i++)
    g_array_append_val (terms, argv[i]);

  timer = g_timer_new ();
  model = gweather_search_find_matching (search, (const char * const *)(gpointer)terms->data);
  elapsed = g_timer_elapsed (timer, NULL) * 1000;
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GWeatherLocation) location = g_list_model_get_item (model, i);
      g_autofree char *city = gweather_location_get_city_name (location);
      g_autofree char *country = gweather_location_get_country_name (location);

      g_print ("%s, %s\n", city, country);
    }

  g_printerr ("=====\n"
              "Found %u items in %u milliseconds\n",
              g_list_model_get_n_items (model),
              elapsed);

  return 0;
}
