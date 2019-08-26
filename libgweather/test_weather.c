
#include <gweather-version.h>
#include "gweather-weather.h"

#include <locale.h>

static char *search_str = "KCMH";
static GWeatherProvider providers = GWEATHER_PROVIDER_METAR | GWEATHER_PROVIDER_IWIN;

static void
find_loc_children (GWeatherLocation  *location,
		   const char        *search_str,
		   GWeatherLocation **ret)
{
    GWeatherLocation **children;
    guint i;

    children = gweather_location_get_children (location);
    for (i = 0; children[i] != NULL; i++) {
        if (gweather_location_get_level (children[i]) == GWEATHER_LOCATION_WEATHER_STATION) {
            const char *code;

            code = gweather_location_get_code (children[i]);
            if (g_strcmp0 (search_str, code) == 0) {
                *ret = gweather_location_ref (children[i]);
                return;
            }
        } else {
            find_loc_children (children[i], search_str, ret);
        }
    }
}

static GWeatherLocation *
find_loc (GWeatherLocation *world,
	  const char       *search_str)
{
    GWeatherLocation *loc = NULL;

    find_loc_children (world, search_str, &loc);
    return loc;
}

static void
weather_updated (GWeatherInfo *info,
                 GMainLoop    *loop)
{
    //FIXME print stuff
    g_message ("Got weather: %s", gweather_info_get_temp_summary (info));
    // g_main_loop_quit (loop);
}

int
main (int argc, char **argv)
{
    GWeatherLocation *world, *loc;
    GWeatherInfo *info;
    GMainLoop *loop;

    setlocale (LC_ALL, "");

    world = gweather_location_get_world ();
    loc = find_loc (world, search_str);

    if (!loc) {
        g_message ("Could not find station for %s", search_str);
        return 1;
    }

    g_message ("Found station %s for '%s'", gweather_location_get_name (loc), search_str);

    loop = g_main_loop_new (NULL, TRUE);
    info = gweather_info_new (loc);
    gweather_info_set_enabled_providers (info, providers);
    g_signal_connect (G_OBJECT (info), "updated",
                      G_CALLBACK (weather_updated), loop);
    gweather_info_update (info);

    g_main_loop_run (loop);
    g_main_loop_unref (loop);

    return 0;
}
