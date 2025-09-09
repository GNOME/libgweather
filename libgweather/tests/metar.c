/* metar.c: METAR tests
 *
 * SPDX-FileCopyrightText: 2021  Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gweather-test-utils.h"

#include <libsoup/soup.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

/* For test_metar_weather_stations */
#define METAR_SOURCES "https://aviationweather.gov/data/cache/stations.cache.xml.gz"

typedef struct
{
  char     *id;
  gboolean  has_metar;
} Station;

static Station *
station_new (const char *id,
             gboolean    has_metar)
{
  Station *station;

  station = g_new0 (Station, 1);
  station->id = g_strdup (id);
  station->has_metar = has_metar;

  return station;
}

static void
station_free (Station *station)
{
  g_free (station->id);
  g_free (station);
}

static char *
decompress_contents (const char *contents,
                     gsize       len)
{
  GZlibDecompressor *decompressor;
  GString *decompressed_str;

  decompressor = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
  decompressed_str = g_string_new (NULL);

  while (len > 0)
    {
      GConverterResult result;
      char buff[1024];
      gsize bytes_read;
      gsize bytes_written;
      GError *error;

      error = NULL;
      result = g_converter_convert (G_CONVERTER(decompressor),
                                    contents,
                                    len,
                                    buff,
                                    sizeof(buff),
                                    G_CONVERTER_NO_FLAGS,
                                    &bytes_read,
                                    &bytes_written,
                                    &error);

      contents += bytes_read;
      len -= bytes_read;

      if (result == G_CONVERTER_CONVERTED || result == G_CONVERTER_FINISHED)
        {
          g_string_append_len (decompressed_str, buff, bytes_written);

          if (result == G_CONVERTER_FINISHED)
				    break;
        }
      else
        {
          if (error != NULL)
            {
              g_test_message ("Failed to decompress " METAR_SOURCES ": %s\n", error->message);
              g_error_free (error);
            }

          g_string_free (decompressed_str, TRUE);
          g_object_unref (decompressor);

          g_test_fail ();

          return NULL;
        }
    }

  g_object_unref (decompressor);

  return g_string_free (decompressed_str, FALSE);
}

static GHashTable *
parse_metar_stations (const char *contents)
{
    xmlDocPtr doc;
    xmlXPathContextPtr ctx;
    xmlXPathObjectPtr result;
    GHashTable *stations_ht;
    guint num_stations;
    int i;

    doc = xmlParseMemory (contents, strlen (contents));

    if (doc == NULL)
        return NULL;

    ctx = xmlXPathNewContext (doc);
    result = xmlXPathEval ((const xmlChar *) "/response/data/Station", ctx);

    if (result == NULL || result->type != XPATH_NODESET) {
        if (result != NULL)
            xmlXPathFreeObject (result);

        xmlXPathFreeContext (ctx);
        xmlFreeDoc (doc);
    }

    stations_ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) station_free);
    num_stations = 0;

    for (i = 0; i < result->nodesetval->nodeNr; i++) {
        xmlNodePtr node;
        char *station_id;
        gboolean has_metar;
        xmlNode *n;
        Station *station;

        node = result->nodesetval->nodeTab[i];

        if (g_strcmp0 ((const char *) node->name, "Station") != 0)
            continue;

        station_id = NULL;
        has_metar = FALSE;

        for (n = node->children; n; n = n->next) {
            const char *node_name;

            node_name = (const char *) n->name;

            if (g_strcmp0 (node_name, "station_id") == 0) {
                xmlChar *val;

                val = xmlNodeGetContent (n);
                station_id = g_strdup ((const char *) val);
                xmlFree (val);

                if (strlen (station_id) != 4) {
                    g_free (station_id);
                    station_id = NULL;
                }
            } else if (g_strcmp0 (node_name, "site_type") == 0) {
                xmlNode *tmp;

                for (tmp = n->children; tmp; tmp = tmp->next) {
                    if (g_strcmp0 ((const char *) tmp->name, "METAR") == 0) {
                        has_metar = TRUE;
                        break;
                    }
                }
            }
        }

        if (station_id == NULL)
            continue;

        /* If it is a duplicate discard it */
        if (g_hash_table_lookup (stations_ht, station_id)) {
            g_test_message ("Weather station '%s' already defined\n", station_id);
            g_free (station_id);
            continue;
        }

        station = station_new (station_id, has_metar);
        g_hash_table_insert (stations_ht, station_id, station);
        num_stations++;
    }

    xmlXPathFreeObject (result);
    xmlXPathFreeContext (ctx);
    xmlFreeDoc (doc);

    /* Duplicates? */
    g_assert_cmpuint (num_stations, ==, g_hash_table_size (stations_ht));

    g_test_message ("Parsed %u weather stations", num_stations);

    return stations_ht;
}

static void
test_metar_weather_station (GWeatherLocation *location,
                            GHashTable *stations_ht)
{
    const char *code;
    const Station *station;

    code = gweather_location_get_code (location);
    g_assert_nonnull (code);

    station = g_hash_table_lookup (stations_ht, code);
    if (station == NULL) {
        g_test_message ("Could not find airport for '%s' in " METAR_SOURCES "\n", code);
        g_test_fail ();
    } else {
        if (!station->has_metar) {
            g_test_message ("Could not find weather station for '%s' in " METAR_SOURCES "\n", code);
            g_test_fail ();
        }
    }
}

static void
test_metar_weather_stations_children (GWeatherLocation *location,
                                      GHashTable *stations_ht)
{
    g_autoptr (GWeatherLocation) child = NULL;

    while ((child = gweather_location_next_child (location, child)) != NULL) {
        if (gweather_location_get_level (child) == GWEATHER_LOCATION_WEATHER_STATION)
            test_metar_weather_station (child, stations_ht);
        else
            test_metar_weather_stations_children (child, stations_ht);
    }
}

static void
test_metar_weather_stations (void)
{
    g_autoptr (GWeatherLocation) world = NULL;
    SoupMessage *msg;
    SoupSession *session;
    GHashTable *stations_ht;
    char *contents;
    GBytes *body;
    GError *error = NULL;
    const char *data;
    gsize bsize;

    world = gweather_location_get_world ();
    g_assert_nonnull (world);

    msg = soup_message_new ("GET", METAR_SOURCES);
    session = soup_session_new ();
    body = soup_session_send_and_read (session, msg, NULL, &error);
    if (error && error->domain == G_TLS_ERROR) {
        g_clear_error (&error);
        g_test_message ("SSL/TLS failure, please check your glib-networking installation");
        g_test_failed ();
        return;
    }
    g_assert_no_error (error);
    g_assert_cmpint (soup_message_get_status (msg), >=, 200);
    g_assert_cmpint (soup_message_get_status (msg), <, 300);
    g_assert_nonnull (body);

    data = g_bytes_get_data (body, &bsize);
    contents = decompress_contents (data, bsize);

    g_bytes_unref (body);
    g_object_unref (session);
    g_object_unref (msg);

    stations_ht = parse_metar_stations (contents);
    g_assert_nonnull (stations_ht);
    g_free (contents);

    test_metar_weather_stations_children (world, stations_ht);

    g_hash_table_unref (stations_ht);

    g_clear_object (&world);

    gweather_test_reset_world ();
}

int
main (int argc, char *argv[])
{
    setlocale (LC_ALL, "");

    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("http://gitlab.gnome.org/GNOME/libgweather/issues/");

    g_autofree char *schemas_dir = gweather_test_setup_gsettings ();

    g_test_add_func ("/weather/metar_weather_stations", test_metar_weather_stations);

    int res = g_test_run ();

    gweather_test_teardown_gsettings (schemas_dir);

    return res;
}
