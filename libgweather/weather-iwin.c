/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* weather-iwin.c - US National Weather Service IWIN forecast source
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>

#include "gweather-private.h"

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

    res = g_str_equal ((const char *)attr, attr_value);

    xmlFree (attr);

    return res;
}

static GSList *
parseForecastXml (const char *buff, GWeatherInfo *master_info)
{
    GSList *res = NULL;
    xmlDocPtr doc;
    xmlNode *root, *node;

    g_return_val_if_fail (master_info != NULL, NULL);

    if (!buff || !*buff)
        return NULL;

    #define XC (const xmlChar *)
    #define isElem(_node,_name) g_str_equal ((const char *)_node->name, _name)

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
            time_t update_times[7] = {0};

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
                                    time_layout = g_strdup ((const char *)val);
                                    xmlFree (val);
                                }
                            } else if (c->name && isElem (c, "start-valid-time")) {
                                xmlChar *val = xmlNodeGetContent (c);

                                if (val) {
                                    GTimeVal tv;

                                    if (g_time_val_from_iso8601 ((const char *)val, &tv)) {
                                        update_times[count] = tv.tv_sec;
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

                        for (i = 0; i < 7;  i++) {
                            GWeatherInfo *nfo = _gweather_info_new_clone (master_info);
			    nfo->priv->current_time = nfo->priv->update = update_times[i];

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
                                    GWeatherInfo *nfo = (GWeatherInfo *)at->data;
				    GWeatherInfoPrivate *priv = nfo->priv;
                                    xmlChar *val = xmlNodeGetContent (c);

                                    /* can pass some values as <value xsi:nil="true"/> */
                                    if (!val || !*val) {
                                        if (is_max)
                                            priv->temp_max = priv->temp_min;
                                        else
                                            priv->temp_min = priv->temp_max;
                                    } else {
                                        if (is_max)
                                            priv->temp_max = atof ((const char *)val);
                                        else
                                            priv->temp_min = atof ((const char *)val);
                                    }

                                    if (val)
                                        xmlFree (val);

                                    priv->tempMinMaxValid = priv->tempMinMaxValid || (priv->temp_max > -999.0 && priv->temp_min > -999.0);
                                    priv->valid = priv->tempMinMaxValid;

                                    at = at->next;
                                }
                            }
                        } else if (p->name && isElem (p, "weather") && hasAttr (p, "time-layout", time_layout)) {
                            xmlNode *c;
                            GSList *at = res;

                            for (c = p->children; c && at; c = c->next) {
                                if (c->name && isElem (c, "weather-conditions")) {
                                    GWeatherInfo *nfo = at->data;
				    GWeatherInfoPrivate *priv = nfo->priv;
                                    xmlChar *val = xmlGetProp (c, XC "weather-summary");

                                    if (val && nfo) {
                                        /* Checking from top to bottom, if 'value' contains 'name', then that win,
                                           thus put longer (more precise) values to the top. */
                                        unsigned int i;
                                        struct _ph_list {
                                            const char *name;
                                            GWeatherConditionPhenomenon ph;
                                        } ph_list[] = {
                                            { "Ice Crystals", GWEATHER_PHENOMENON_ICE_CRYSTALS } ,
                                            { "Volcanic Ash", GWEATHER_PHENOMENON_VOLCANIC_ASH } ,
                                            { "Blowing Sand", GWEATHER_PHENOMENON_SANDSTORM } ,
                                            { "Blowing Dust", GWEATHER_PHENOMENON_DUSTSTORM } ,
                                            { "Blowing Snow", GWEATHER_PHENOMENON_FUNNEL_CLOUD } ,
                                            { "Drizzle", GWEATHER_PHENOMENON_DRIZZLE } ,
                                            { "Rain", GWEATHER_PHENOMENON_RAIN } ,
                                            { "Snow", GWEATHER_PHENOMENON_SNOW } ,
                                            { "Fog", GWEATHER_PHENOMENON_FOG } ,
                                            { "Smoke", GWEATHER_PHENOMENON_SMOKE } ,
                                            { "Sand", GWEATHER_PHENOMENON_SAND } ,
                                            { "Haze", GWEATHER_PHENOMENON_HAZE } ,
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
                                        struct _sky_list {
                                            const char *name;
                                            GWeatherSky sky;
                                        } sky_list[] = {
                                            { "Mostly Sunny", GWEATHER_SKY_BROKEN } ,
                                            { "Mostly Clear", GWEATHER_SKY_BROKEN } ,
                                            { "Partly Cloudy", GWEATHER_SKY_SCATTERED } ,
                                            { "Mostly Cloudy", GWEATHER_SKY_FEW } ,
                                            { "Sunny", GWEATHER_SKY_CLEAR } ,
                                            { "Clear", GWEATHER_SKY_CLEAR } ,
                                            { "Cloudy", GWEATHER_SKY_OVERCAST } ,
                                            { "Clouds", GWEATHER_SKY_SCATTERED } ,
                                            { "Rain", GWEATHER_SKY_SCATTERED } ,
                                            { "Snow", GWEATHER_SKY_SCATTERED }
                                        };

                                        priv->valid = TRUE;

                                        for (i = 0; i < G_N_ELEMENTS (ph_list); i++) {
                                            if (strstr ((const char *)val, ph_list [i].name)) {
						priv->cond.significant = TRUE;
                                                priv->cond.phenomenon = ph_list [i].ph;
                                                break;
                                            }
                                        }

                                        for (i = 0; i < G_N_ELEMENTS (sky_list); i++) {
                                            if (strstr ((const char *)val, sky_list [i].name)) {
                                                priv->sky = sky_list [i].sky;
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
			    GWeatherInfoPrivate *priv = nfo->priv;

                            if (!nfo || !priv->valid) {
                                if (r->data)
                                    g_object_unref (r->data);

                                r->data = NULL;
                            } else {
                                have_any = TRUE;

                                if (priv->tempMinMaxValid)
                                    priv->temp = (priv->temp_min + priv->temp_max) / 2.0;
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
iwin_finish (SoupSession *session, SoupMessage *msg, gpointer data)
{
    GWeatherInfo *info;
    GWeatherInfoPrivate *priv;
    WeatherLocation *loc;

    if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
        /* forecast data is not really interesting anyway ;) */
	if (msg->status_code == SOUP_STATUS_CANCELLED) {
	    g_debug ("Failed to get IWIN forecast data: %d %s\n",
		     msg->status_code, msg->reason_phrase);
	    return;
	}
	g_warning ("Failed to get IWIN forecast data: %d %s\n",
		   msg->status_code, msg->reason_phrase);
        _gweather_info_request_done (data, msg);
        return;
    }

    info = data;
    priv = info->priv;
    loc = &priv->location;

    g_debug ("iwin data for %s", loc->zone);
    g_debug ("%s", msg->response_body->data);

    priv->forecast_list = parseForecastXml (msg->response_body->data, info);

    _gweather_info_request_done (info, msg);
}

/* Get forecast into newly alloc'ed string */
gboolean
iwin_start_open (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    gchar *url;
    WeatherLocation *loc;
    SoupMessage *msg;
    struct tm tm;
    time_t now;
    gchar latstr[G_ASCII_DTOSTR_BUF_SIZE], lonstr[G_ASCII_DTOSTR_BUF_SIZE];

    g_assert (info != NULL);

    priv = info->priv;
    loc = &priv->location;

    /* No zone (or -) means no weather information from national offices.
       We don't actually use zone, but it's a good indicator of a US location.
       (@ and : prefixes were used in the past for Australia and UK) */
    if (!loc->zone || loc->zone[0] == '-' || loc->zone[0] == '@' || loc->zone[0] == ':') {
        g_debug ("iwin_start_open, ignoring location %s because zone '%s' has no weather info",
                 loc->name, loc->zone ? loc->zone : "(empty)");
        return FALSE;
    }

    if (!loc->latlon_valid)
	return FALSE;

    /* see the description here: http://www.weather.gov/forecasts/xml/ */
    now = time (NULL);
    localtime_r (&now, &tm);

    g_ascii_dtostr (latstr, sizeof(latstr), RADIANS_TO_DEGREES (loc->latitude));
    g_ascii_dtostr (lonstr, sizeof(lonstr), RADIANS_TO_DEGREES (loc->longitude));
    url = g_strdup_printf ("https://www.weather.gov/forecasts/xml/sample_products/browser_interface/ndfdBrowserClientByDay.php?&lat=%s&lon=%s&format=24+hourly&startDate=%04d-%02d-%02d&numDays=7",
			   latstr, lonstr, 1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday);
    g_debug ("iwin_start_open, requesting: %s", url);
    msg = soup_message_new ("GET", url);
    _gweather_info_begin_request (info, msg);
    soup_session_queue_message (priv->session, msg, iwin_finish, info);

    g_free (url);

    return TRUE;
}
