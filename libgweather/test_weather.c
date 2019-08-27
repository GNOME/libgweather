
#include <gweather-version.h>
#include "gweather-weather.h"

#include <locale.h>

static char *search_str;
// gnome-weather uses:
static GWeatherProvider providers = GWEATHER_PROVIDER_METAR | GWEATHER_PROVIDER_YR_NO | GWEATHER_PROVIDER_OWM;

// default:
//static GWeatherProvider providers = GWEATHER_PROVIDER_METAR | GWEATHER_PROVIDER_IWIN;

static gboolean
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
                return TRUE;
            }
        } else {
            if (find_loc_children (children[i], search_str, ret))
                return TRUE;
        }
    }

    return FALSE;
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
    GSList *forecasts, *l;
    time_t val;
    static gboolean weather_printed = FALSE;
    static gboolean forecast_printed = FALSE;

    if (!gweather_info_is_valid (info)) {
        g_warning ("Weather is invalid");
        return;
    }

    if (gweather_info_get_value_update (info, &val)) {
        g_message ("Weather now: %s", gweather_info_get_temp_summary (info));
        weather_printed = TRUE;
    }

    forecasts = gweather_info_get_forecast_list (info);
    if (!forecasts) {
        if (!weather_printed)
           g_warning ("No forecasts, but no weather either?!");
        return;
    }

    for (l = forecasts; l != NULL; l = l->next) {
        GWeatherInfo *i = l->data;

        if (gweather_info_get_value_update (i, &val)) {
            GDateTime *d;
            char *date_str;

            d = g_date_time_new_from_unix_utc (val);
            date_str = g_date_time_format (d, "%c");
            g_message ("Weather for %s: %s", date_str, gweather_info_get_temp_summary (i));
            g_free (date_str);
            g_date_time_unref (d);

            /* One will be enough... */
            forecast_printed = TRUE;
        }
    }

    if (weather_printed &&
        forecast_printed)
        g_main_loop_quit (loop);
}

#define ADD_PROVIDER_STR(x) {			\
	if (s->len != 0)			\
		g_string_append (s, ", ");	\
	g_string_append(s, x);			\
}

static gboolean
set_providers (GWeatherInfo *info)
{
    GString *s;

    s = g_string_new (NULL);
    if (providers & GWEATHER_PROVIDER_METAR)
        ADD_PROVIDER_STR("METAR");
    if (providers & GWEATHER_PROVIDER_IWIN)
        ADD_PROVIDER_STR("IWIN");
    if (providers & GWEATHER_PROVIDER_YAHOO)
        ADD_PROVIDER_STR("YAHOO");
    if (providers & GWEATHER_PROVIDER_YR_NO)
        ADD_PROVIDER_STR("YR_NO");
    if (providers & GWEATHER_PROVIDER_OWM)
        ADD_PROVIDER_STR("OWM");
    if (providers == GWEATHER_PROVIDER_NONE) {
        g_string_free (s, TRUE);
        g_warning ("No providers enabled, failing");
        return FALSE;
    }
    g_message ("Enabling providers (%s)", s->str);
    g_string_free (s, TRUE);
    gweather_info_set_enabled_providers (info, providers);
    return TRUE;
}

int
main (int argc, char **argv)
{
    GWeatherLocation *world, *loc;
    GWeatherInfo *info;
    GMainLoop *loop;

    setlocale (LC_ALL, "");

    // FIXME add options
    if (argc != 2)
        return 1;
    search_str = argv[1];

    world = gweather_location_get_world ();
    loc = find_loc (world, search_str);

    if (!loc) {
        g_message ("Could not find station for %s", search_str);
        return 1;
    }

    g_message ("Found station %s for '%s'", gweather_location_get_name (loc), search_str);

    loop = g_main_loop_new (NULL, TRUE);
    info = gweather_info_new (NULL);
    if (!set_providers (info))
        return 1;
    gweather_info_set_location (info, loc);
    g_signal_connect (G_OBJECT (info), "updated",
                      G_CALLBACK (weather_updated), loop);
    gweather_info_update (info);

    g_main_loop_run (loop);
    g_main_loop_unref (loop);

    return 0;
}
