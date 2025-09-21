/* weather-iwin.c - US National Weather Service IWIN forecast source
 *
 * SPDX-FileCopyrightText: The GWeather authors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "gweather-private.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>

static gboolean
hasAttr (xmlNode *node, const char *attr_name, const char *attr_value)
{
    xmlChar *attr;
    gboolean res = FALSE;

    if (!node)
        return res;

    attr = xmlGetProp (node, (const xmlChar *) attr_name);

    if (!attr)
        return res;

    res = g_str_equal ((const char *) attr, attr_value);

    xmlFree (attr);

    return res;
}

static GSList *
parseForecastXml (const char *buff, GWeatherInfo *original_info)
{
    GSList *res = NULL;
    xmlDocPtr doc;
    xmlNode *root, *node;

    g_return_val_if_fail (original_info != NULL, NULL);

    if (!buff || !*buff)
        return NULL;

#define XC                   (const xmlChar *)
#define isElem(_node, _name) g_str_equal ((const char *) _node->name, _name)

    doc = xmlParseMemory (buff, strlen (buff));
    if (!doc)
        return NULL;

    /* Description at http://www.weather.gov/mdl/XML/Design/MDL_XML_Design.pdf */
    root = xmlDocGetRootElement (doc);
    for (node = root->xmlChildrenNode; node; node = node->next) {
        if (node->name == NULL || node->type != XML_ELEMENT_NODE)
            continue;

        if (isElem (node, "data")) {
            xmlNode *n;
            char *time_layout = NULL;
            gint64 update_times[7] = { 0 };

            for (n = node->children; n; n = n->next) {
                if (!n->name)
                    continue;

                if (isElem (n, "time-layout")) {
                    if (!time_layout && hasAttr (n, "summarization", "24hourly")) {
                        xmlNode *c;
                        int count = 0;

                        for (c = n->children; c && (count < 7 || !time_layout); c = c->next) {
                            if (c->name && !time_layout && isElem (c, "layout-key")) {
                                xmlChar *val = xmlNodeGetContent (c);

                                if (val) {
                                    time_layout = g_strdup ((const char *) val);
                                    xmlFree (val);
                                }
                            } else if (c->name && isElem (c, "start-valid-time")) {
                                xmlChar *val = xmlNodeGetContent (c);

                                if (val) {
                                    GDateTime *dt;

                                    dt = g_date_time_new_from_iso8601 ((const char *) val, NULL);
                                    if (dt != NULL) {
                                        update_times[count] = g_date_time_to_unix (dt);
                                        g_date_time_unref (dt);
                                    } else {
                                        update_times[count] = 0;
                                    }

                                    count++;

                                    xmlFree (val);
                                }
                            }
                        }

                        if (count != 7) {
                            /* There can be more than one time-layout element, the other
                               with only few children, which is not the one to use. */
                            g_free (time_layout);
                            time_layout = NULL;
                        }
                    }
                } else if (isElem (n, "parameters")) {
                    xmlNode *p;

                    /* time-layout should be always before parameters */
                    if (!time_layout)
                        break;

                    if (!res) {
                        int i;

                        for (i = 0; i < 7; i++) {
                            GWeatherInfo *nfo = _gweather_info_new_clone (original_info);
                            nfo->current_time = nfo->update = update_times[i];

                            if (nfo)
                                res = g_slist_append (res, nfo);
                        }
                    }

                    for (p = n->children; p; p = p->next) {
                        if (p->name && isElem (p, "temperature") && hasAttr (p, "time-layout", time_layout)) {
                            xmlNode *c;
                            GSList *at = res;
                            gboolean is_max = hasAttr (p, "type", "maximum");

                            if (!is_max && !hasAttr (p, "type", "minimum"))
                                break;

                            for (c = p->children; c && at; c = c->next) {
                                if (isElem (c, "value")) {
                                    GWeatherInfo *nfo = (GWeatherInfo *) at->data;
                                    xmlChar *val = xmlNodeGetContent (c);

                                    /* can pass some values as <value xsi:nil="true"/> */
                                    if (!val || !*val) {
                                        if (is_max)
                                            nfo->temp_max = nfo->temp_min;
                                        else
                                            nfo->temp_min = nfo->temp_max;
                                    } else {
                                        if (is_max)
                                            nfo->temp_max = atof ((const char *) val);
                                        else
                                            nfo->temp_min = atof ((const char *) val);
                                    }

                                    if (val)
                                        xmlFree (val);

                                    nfo->tempMinMaxValid = nfo->tempMinMaxValid || (nfo->temp_max > -999.0 && nfo->temp_min > -999.0);
                                    nfo->valid = nfo->tempMinMaxValid;

                                    at = at->next;
                                }
                            }
                        } else if (p->name && isElem (p, "weather") && hasAttr (p, "time-layout", time_layout)) {
                            xmlNode *c;
                            GSList *at = res;

                            for (c = p->children; c && at; c = c->next) {
                                if (c->name && isElem (c, "weather-conditions")) {
                                    GWeatherInfo *nfo = at->data;
                                    xmlChar *val = xmlGetProp (c, XC "weather-summary");

                                    if (val && nfo) {
                                        /* Checking from top to bottom, if 'value' contains 'name', then that win,
                                           thus put longer (more precise) values to the top. */
                                        unsigned int i;
                                        struct _ph_list
                                        {
                                            const char *name;
                                            GWeatherConditionPhenomenon ph;
                                        } ph_list[] = {
                                            { "Ice Crystals", GWEATHER_PHENOMENON_ICE_CRYSTALS },
                                            { "Volcanic Ash", GWEATHER_PHENOMENON_VOLCANIC_ASH },
                                            { "Blowing Sand", GWEATHER_PHENOMENON_SANDSTORM },
                                            { "Blowing Dust", GWEATHER_PHENOMENON_DUSTSTORM },
                                            { "Blowing Snow", GWEATHER_PHENOMENON_FUNNEL_CLOUD },
                                            { "Drizzle", GWEATHER_PHENOMENON_DRIZZLE },
                                            { "Rain", GWEATHER_PHENOMENON_RAIN },
                                            { "Snow", GWEATHER_PHENOMENON_SNOW },
                                            { "Fog", GWEATHER_PHENOMENON_FOG },
                                            { "Smoke", GWEATHER_PHENOMENON_SMOKE },
                                            { "Sand", GWEATHER_PHENOMENON_SAND },
                                            { "Haze", GWEATHER_PHENOMENON_HAZE },
                                            { "Dust", GWEATHER_PHENOMENON_DUST } /*,
                                            { "", GWEATHER_PHENOMENON_SNOW_GRAINS } ,
                                            { "", GWEATHER_PHENOMENON_ICE_PELLETS } ,
                                            { "", GWEATHER_PHENOMENON_HAIL } ,
                                            { "", GWEATHER_PHENOMENON_SMALL_HAIL } ,
                                            { "", GWEATHER_PHENOMENON_UNKNOWN_PRECIPITATION } ,
                                            { "", GWEATHER_PHENOMENON_MIST } ,
                                            { "", GWEATHER_PHENOMENON_SPRAY } ,
                                            { "", GWEATHER_PHENOMENON_SQUALL } ,
                                            { "", GWEATHER_PHENOMENON_TORNADO } ,
                                            { "", GWEATHER_PHENOMENON_DUST_WHIRLS } */
                                        };
                                        struct _sky_list
                                        {
                                            const char *name;
                                            GWeatherSky sky;
                                        } sky_list[] = {
                                            { "Mostly Sunny", GWEATHER_SKY_BROKEN },
                                            { "Mostly Clear", GWEATHER_SKY_BROKEN },
                                            { "Partly Cloudy", GWEATHER_SKY_SCATTERED },
                                            { "Mostly Cloudy", GWEATHER_SKY_FEW },
                                            { "Sunny", GWEATHER_SKY_CLEAR },
                                            { "Clear", GWEATHER_SKY_CLEAR },
                                            { "Cloudy", GWEATHER_SKY_OVERCAST },
                                            { "Clouds", GWEATHER_SKY_SCATTERED },
                                            { "Rain", GWEATHER_SKY_SCATTERED },
                                            { "Snow", GWEATHER_SKY_SCATTERED }
                                        };

                                        nfo->valid = TRUE;

                                        for (i = 0; i < G_N_ELEMENTS (ph_list); i++) {
                                            if (strstr ((const char *) val, ph_list[i].name)) {
                                                nfo->cond.significant = TRUE;
                                                nfo->cond.phenomenon = ph_list[i].ph;
                                                break;
                                            }
                                        }

                                        for (i = 0; i < G_N_ELEMENTS (sky_list); i++) {
                                            if (strstr ((const char *) val, sky_list[i].name)) {
                                                nfo->sky = sky_list[i].sky;
                                                break;
                                            }
                                        }
                                    }

                                    if (val)
                                        xmlFree (val);

                                    at = at->next;
                                }
                            }
                        }
                    }

                    if (res) {
                        gboolean have_any = FALSE;
                        GSList *r;

                        /* Remove invalid forecast data from the list.
                           They should be all valid or all invalid. */
                        for (r = res; r; r = r->next) {
                            GWeatherInfo *nfo = r->data;

                            if (!nfo || !nfo->valid) {
                                if (r->data)
                                    g_object_unref (r->data);

                                r->data = NULL;
                            } else {
                                have_any = TRUE;

                                if (nfo->tempMinMaxValid)
                                    nfo->temp = (nfo->temp_min + nfo->temp_max) / 2.0;
                            }
                        }

                        if (!have_any) {
                            /* data members are freed already */
                            g_slist_free (res);
                            res = NULL;
                        }
                    }

                    break;
                }
            }

            g_free (time_layout);

            /* stop seeking XML */
            break;
        }
    }
    xmlFreeDoc (doc);

#undef XC
#undef isElem

    return res;
}

static void
iwin_finish (GObject *source, GAsyncResult *result, gpointer data)
{
    GWeatherInfo *info;
    WeatherLocation *loc;
    SoupSession *session = SOUP_SESSION (source);
    SoupMessage *msg = soup_session_get_async_result_message (session, result);
    GBytes *body;
    GError *error = NULL;
    const char *content;

    body = soup_session_send_and_read_finish (session, result, &error);

    if (!body) {
        /* forecast data is not really interesting anyway ;) */
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_debug ("Failed to get IWIN forecast data: %s", error->message);
            return;
        }
        g_warning ("Failed to get IWIN forecast data: %s", error->message);
        g_clear_error (&error);
        _gweather_info_request_done (data, msg);
        return;
    } else if (!SOUP_STATUS_IS_SUCCESSFUL (soup_message_get_status (msg))) {
        g_bytes_unref (body);
        g_warning ("Failed to get IWIN forecast data: [status: %d]: %s",
                   soup_message_get_status (msg),
                   soup_message_get_reason_phrase (msg));
        _gweather_info_request_done (data, msg);
        return;
    }

    info = data;
    loc = &info->location;
    content = g_bytes_get_data (body, NULL);

    g_debug ("iwin data for %s", loc->zone);
    g_debug ("%s", content);

    info->forecast_list = parseForecastXml (content, info);
    g_bytes_unref (body);

    _gweather_info_request_done (info, msg);
}

/* Get forecast into newly alloc'ed string */
gboolean
iwin_start_open (GWeatherInfo *info)
{
    gchar *url;
    WeatherLocation *loc;
    SoupMessage *msg;
    struct tm tm;
    time_t now;
    g_autofree char *latstr = NULL;
    g_autofree char *lonstr = NULL;

    g_assert (info != NULL);

    loc = &info->location;

    /* No zone (or -) means no weather information from national offices.
       We don't actually use zone, but it's a good indicator of a US location.
       (@ and : prefixes were used in the past for Australia and UK) */
    if (!loc->zone || loc->zone[0] == '-' || loc->zone[0] == '@' || loc->zone[0] == ':') {
        g_debug ("iwin_start_open, ignoring location %s because zone '%s' has no weather info",
                 loc->name,
                 loc->zone ? loc->zone : "(empty)");
        return FALSE;
    }

    if (!loc->latlon_valid)
        return FALSE;

    /* see the description here: http://www.weather.gov/forecasts/xml/ */
    now = time (NULL);
    localtime_r (&now, &tm);

    latstr = _radians_to_degrees_str (loc->latitude);
    lonstr = _radians_to_degrees_str (loc->longitude);
    url = g_strdup_printf ("https://www.weather.gov/forecasts/xml/sample_products/browser_interface/ndfdBrowserClientByDay.php?&lat=%s&lon=%s&format=24+hourly&startDate=%04d-%02d-%02d&numDays=7",
                           latstr,
                           lonstr,
                           1900 + tm.tm_year,
                           1 + tm.tm_mon,
                           tm.tm_mday);
    g_debug ("iwin_start_open, requesting: %s", url);
    msg = soup_message_new ("GET", url);
    _gweather_info_begin_request (info, msg);
    _gweather_info_queue_request (info, msg, iwin_finish);

    g_free (url);

    return TRUE;
}
