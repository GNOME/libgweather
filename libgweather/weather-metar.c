/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* weather-metar.c - Weather server functions (METAR)
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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "gweather-private.h"

enum {
    TIME_RE,
    WIND_RE,
    VIS_RE,
    COND_RE,
    CLOUD_RE,
    TEMP_RE,
    PRES_RE,

    RE_NUM
};

/* Return time of weather report as secs since epoch UTC */
static time_t
make_time (gint utcDate, gint utcHour, gint utcMin)
{
    const time_t now = time (NULL);
    struct tm tm;

    localtime_r (&now, &tm);

    /* If last reading took place just before midnight UTC on the
     * first, adjust the date downward to allow for the month
     * change-over.  This ASSUMES that the reading won't be more than
     * 24 hrs old! */
    if ((utcDate > tm.tm_mday) && (tm.tm_mday == 1)) {
        tm.tm_mday = 0; /* mktime knows this is the last day of the previous
			 * month. */
    } else {
        tm.tm_mday = utcDate;
    }
    tm.tm_hour = utcHour;
    tm.tm_min  = utcMin;
    tm.tm_sec  = 0;

    /* mktime() assumes value is local, not UTC.  Use tm_gmtoff to compensate */
#ifdef HAVE_TM_TM_GMOFF
    return tm.tm_gmtoff + mktime (&tm);
#elif defined HAVE_TIMEZONE
    return timezone + mktime (&tm);
#endif
}

static void
metar_tok_time (gchar *tokp, GWeatherInfo *info)
{
    gint day, hr, min;

    g_debug ("metar_tok_time: %s", tokp);

    sscanf (tokp, "%2u%2u%2u", &day, &hr, &min);
    info->priv->update = make_time (day, hr, min);
}

static void
metar_tok_wind (gchar *tokp, GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    gchar sdir[4], sspd[4], sgust[4];
    gint dir, spd = -1;
    gchar *gustp;
    size_t glen;

    g_debug ("metar_tok_wind: %s", tokp);

    priv = info->priv;

    strncpy (sdir, tokp, 3);
    sdir[3] = 0;
    dir = (!strcmp (sdir, "VRB")) ? -1 : atoi (sdir);

    memset (sspd, 0, sizeof (sspd));
    glen = strspn (tokp + 3, CONST_DIGITS);
    strncpy (sspd, tokp + 3, glen);
    spd = atoi (sspd);
    tokp += glen + 3;

    gustp = strchr (tokp, 'G');
    if (gustp) {
        memset (sgust, 0, sizeof (sgust));
	glen = strspn (gustp + 1, CONST_DIGITS);
        strncpy (sgust, gustp + 1, glen);
	tokp = gustp + 1 + glen;
    }

    if (!strcmp (tokp, "MPS"))
	priv->windspeed = WINDSPEED_MS_TO_KNOTS ((GWeatherWindSpeed)spd);
    else
	priv->windspeed = (GWeatherWindSpeed)spd;

    if ((349 <= dir) || (dir <= 11))
        priv->wind = GWEATHER_WIND_N;
    else if ((12 <= dir) && (dir <= 33))
        priv->wind = GWEATHER_WIND_NNE;
    else if ((34 <= dir) && (dir <= 56))
        priv->wind = GWEATHER_WIND_NE;
    else if ((57 <= dir) && (dir <= 78))
        priv->wind = GWEATHER_WIND_ENE;
    else if ((79 <= dir) && (dir <= 101))
        priv->wind = GWEATHER_WIND_E;
    else if ((102 <= dir) && (dir <= 123))
        priv->wind = GWEATHER_WIND_ESE;
    else if ((124 <= dir) && (dir <= 146))
        priv->wind = GWEATHER_WIND_SE;
    else if ((147 <= dir) && (dir <= 168))
        priv->wind = GWEATHER_WIND_SSE;
    else if ((169 <= dir) && (dir <= 191))
        priv->wind = GWEATHER_WIND_S;
    else if ((192 <= dir) && (dir <= 213))
        priv->wind = GWEATHER_WIND_SSW;
    else if ((214 <= dir) && (dir <= 236))
        priv->wind = GWEATHER_WIND_SW;
    else if ((237 <= dir) && (dir <= 258))
        priv->wind = GWEATHER_WIND_WSW;
    else if ((259 <= dir) && (dir <= 281))
        priv->wind = GWEATHER_WIND_W;
    else if ((282 <= dir) && (dir <= 303))
        priv->wind = GWEATHER_WIND_WNW;
    else if ((304 <= dir) && (dir <= 326))
        priv->wind = GWEATHER_WIND_NW;
    else if ((327 <= dir) && (dir <= 348))
        priv->wind = GWEATHER_WIND_NNW;
}

static void
metar_tok_vis (gchar *tokp, GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    gchar *pfrac, *pend, *psp;
    gchar sval[6];
    gint num, den, val;

    g_debug ("metar_tok_vis: %s", tokp);

    priv = info->priv;

    memset (sval, 0, sizeof (sval));

    if (!strcmp (tokp,"CAVOK")) {
        // "Ceiling And Visibility OK": visibility >= 10 KM
        priv->visibility=10000. / VISIBILITY_SM_TO_M (1.);
        priv->sky = GWEATHER_SKY_CLEAR;
    } else if (0 != (pend = strstr (tokp, "SM"))) {
        // US observation: field ends with "SM"
        pfrac = strchr (tokp, '/');
        if (pfrac) {
	    if (*tokp == 'M') {
	        priv->visibility = 0.001;
	    } else {
	        num = (*(pfrac - 1) - '0');
		strncpy (sval, pfrac + 1, pend - pfrac - 1);
		den = atoi (sval);
		priv->visibility =
		    ((GWeatherVisibility)num / ((GWeatherVisibility)den));

		psp = strchr (tokp, ' ');
		if (psp) {
		    *psp = '\0';
		    val = atoi (tokp);
		    priv->visibility += (GWeatherVisibility)val;
		}
	    }
	} else {
	    strncpy (sval, tokp, pend - tokp);
            val = atoi (sval);
            priv->visibility = (GWeatherVisibility)val;
	}
    } else {
        // International observation: NNNN(DD NNNNDD)?
        // For now: use only the minimum visibility and ignore its direction
        strncpy (sval, tokp, strspn (tokp, CONST_DIGITS));
	val = atoi (sval);
	priv->visibility = (GWeatherVisibility)val / VISIBILITY_SM_TO_M (1.);
    }
}

static void
metar_tok_cloud (gchar *tokp, GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    gchar stype[4], salt[4];

    g_debug ("metar_tok_cloud: %s", tokp);

    priv = info->priv;

    strncpy (stype, tokp, 3);
    stype[3] = 0;
    if (strlen (tokp) == 6) {
        strncpy (salt, tokp + 3, 3);
        salt[3] = 0;
    }

    if (!strcmp (stype, "CLR")) {
        priv->sky = GWEATHER_SKY_CLEAR;
    } else if (!strcmp (stype, "SKC")) {
        priv->sky = GWEATHER_SKY_CLEAR;
    } else if (!strcmp (stype, "NSC")) {
        priv->sky = GWEATHER_SKY_CLEAR;
    } else if (!strcmp (stype, "BKN")) {
        priv->sky = GWEATHER_SKY_BROKEN;
    } else if (!strcmp (stype, "SCT")) {
        priv->sky = GWEATHER_SKY_SCATTERED;
    } else if (!strcmp (stype, "FEW")) {
        priv->sky = GWEATHER_SKY_FEW;
    } else if (!strcmp (stype, "OVC")) {
        priv->sky = GWEATHER_SKY_OVERCAST;
    }
}

static void
metar_tok_pres (gchar *tokp, GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv = info->priv;

    g_debug ("metar_tok_pres: %s", tokp);

    if (*tokp == 'A') {
        gchar sintg[3], sfract[3];
        gint intg, fract;

        strncpy (sintg, tokp + 1, 2);
        sintg[2] = 0;
        intg = atoi (sintg);

        strncpy (sfract, tokp + 3, 2);
        sfract[2] = 0;
        fract = atoi (sfract);

        priv->pressure = (GWeatherPressure)intg + (((GWeatherPressure)fract)/100.0);
    } else {  /* *tokp == 'Q' */
        gchar spres[5];
        gint pres;

        strncpy (spres, tokp + 1, 4);
        spres[4] = 0;
        pres = atoi (spres);

        priv->pressure = PRESSURE_MBAR_TO_INCH ((GWeatherPressure)pres);
    }
}

static void
metar_tok_temp (gchar *tokp, GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    gchar *ptemp, *pdew, *psep;

    g_debug ("metar_tok_temp: %s", tokp);

    priv = info->priv;

    psep = strchr (tokp, '/');
    *psep = 0;
    ptemp = tokp;
    pdew = psep + 1;

    priv->temp = (*ptemp == 'M') ? TEMP_C_TO_F (-atoi (ptemp + 1))
	: TEMP_C_TO_F (atoi (ptemp));
    if (*pdew) {
	priv->dew = (*pdew == 'M') ? TEMP_C_TO_F (-atoi (pdew + 1))
	    : TEMP_C_TO_F (atoi (pdew));
    } else {
	priv->dew = -1000.0;
    }
}

/* How "important" are the conditions to be reported to the user.
   Indexed by GWeatherConditionPhenomenon */
static const int importance_scale[] = {
    0, /* invalid */
    0, /* none */
    20, /* drizzle */
    30, /* rain */
    35, /* snow */
    35, /* snow grains */
    35, /* ice crystals */
    35, /* ice pellets */
    35, /* hail */
    35, /* small hail */
    20, /* unknown precipitation */
    10, /* mist */
    15, /* fog */
    15, /* smoke */
    18, /* volcanic ash */
    18, /* sand */
    15, /* haze */
    15, /* spray */
    15, /* dust */
    40, /* squall */
    50, /* sandstorm */
    50, /* duststorm */
    70, /* funnel cloud */
    70, /* tornado */
    50, /* dust whirls */
};

static gboolean
condition_more_important (GWeatherConditions *which,
			  GWeatherConditions *than)
{
    if (!than->significant)
	return TRUE;
    if (!which->significant)
	return FALSE;

    if (importance_scale[than->phenomenon] <
	importance_scale[which->phenomenon])
	return TRUE;

    return FALSE;
}

static void
metar_tok_cond (gchar *tokp, GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    GWeatherConditions new_cond;
    gchar squal[3], sphen[4];
    gchar *pphen;

    g_debug ("metar_tok_cond: %s", tokp);

    priv = info->priv;

    if ((strlen (tokp) > 3) && ((*tokp == '+') || (*tokp == '-')))
        ++tokp;   /* FIX */

    if ((*tokp == '+') || (*tokp == '-'))
        pphen = tokp + 1;
    else if (strlen (tokp) < 4)
        pphen = tokp;
    else
        pphen = tokp + 2;

    memset (squal, 0, sizeof (squal));
    strncpy (squal, tokp, pphen - tokp);
    squal[pphen - tokp] = 0;

    memset (sphen, 0, sizeof (sphen));
    strncpy (sphen, pphen, sizeof (sphen));
    sphen[sizeof (sphen)-1] = '\0';

    /* Defaults */
    new_cond.qualifier = GWEATHER_QUALIFIER_NONE;
    new_cond.phenomenon = GWEATHER_PHENOMENON_NONE;
    new_cond.significant = FALSE;

    if (!strcmp (squal, "")) {
        new_cond.qualifier = GWEATHER_QUALIFIER_MODERATE;
    } else if (!strcmp (squal, "-")) {
        new_cond.qualifier = GWEATHER_QUALIFIER_LIGHT;
    } else if (!strcmp (squal, "+")) {
        new_cond.qualifier = GWEATHER_QUALIFIER_HEAVY;
    } else if (!strcmp (squal, "VC")) {
        new_cond.qualifier = GWEATHER_QUALIFIER_VICINITY;
    } else if (!strcmp (squal, "MI")) {
        new_cond.qualifier = GWEATHER_QUALIFIER_SHALLOW;
    } else if (!strcmp (squal, "BC")) {
        new_cond.qualifier = GWEATHER_QUALIFIER_PATCHES;
    } else if (!strcmp (squal, "PR")) {
        new_cond.qualifier = GWEATHER_QUALIFIER_PARTIAL;
    } else if (!strcmp (squal, "TS")) {
        new_cond.qualifier = GWEATHER_QUALIFIER_THUNDERSTORM;
    } else if (!strcmp (squal, "BL")) {
        new_cond.qualifier = GWEATHER_QUALIFIER_BLOWING;
    } else if (!strcmp (squal, "SH")) {
        new_cond.qualifier = GWEATHER_QUALIFIER_SHOWERS;
    } else if (!strcmp (squal, "DR")) {
        new_cond.qualifier = GWEATHER_QUALIFIER_DRIFTING;
    } else if (!strcmp (squal, "FZ")) {
        new_cond.qualifier = GWEATHER_QUALIFIER_FREEZING;
    } else {
        return;
    }

    if (!strcmp (sphen, "DZ")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_DRIZZLE;
    } else if (!strcmp (sphen, "RA")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_RAIN;
    } else if (!strcmp (sphen, "SN")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_SNOW;
    } else if (!strcmp (sphen, "SG")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_SNOW_GRAINS;
    } else if (!strcmp (sphen, "IC")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_ICE_CRYSTALS;
    } else if (!strcmp (sphen, "PL")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_ICE_PELLETS;
    } else if (!strcmp (sphen, "GR")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_HAIL;
    } else if (!strcmp (sphen, "GS")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_SMALL_HAIL;
    } else if (!strcmp (sphen, "UP")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_UNKNOWN_PRECIPITATION;
    } else if (!strcmp (sphen, "BR")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_MIST;
    } else if (!strcmp (sphen, "FG")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_FOG;
    } else if (!strcmp (sphen, "FU")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_SMOKE;
    } else if (!strcmp (sphen, "VA")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_VOLCANIC_ASH;
    } else if (!strcmp (sphen, "SA")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_SAND;
    } else if (!strcmp (sphen, "HZ")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_HAZE;
    } else if (!strcmp (sphen, "PY")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_SPRAY;
    } else if (!strcmp (sphen, "DU")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_DUST;
    } else if (!strcmp (sphen, "SQ")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_SQUALL;
    } else if (!strcmp (sphen, "SS")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_SANDSTORM;
    } else if (!strcmp (sphen, "DS")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_DUSTSTORM;
    } else if (!strcmp (sphen, "PO")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_DUST_WHIRLS;
    } else if (!strcmp (sphen, "+FC")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_TORNADO;
    } else if (!strcmp (sphen, "FC")) {
        new_cond.phenomenon = GWEATHER_PHENOMENON_FUNNEL_CLOUD;
    } else {
        return;
    }

    if ((new_cond.qualifier != GWEATHER_QUALIFIER_NONE) || (new_cond.phenomenon != GWEATHER_PHENOMENON_NONE))
        new_cond.significant = TRUE;

    if (condition_more_important (&new_cond, &priv->cond))
	priv->cond = new_cond;
}

#define TIME_RE_STR  "([0-9]{6})Z"
#define WIND_RE_STR  "(([0-9]{3})|VRB)([0-9]?[0-9]{2})(G[0-9]?[0-9]{2})?(KT|MPS)"
#define VIS_RE_STR   "((([0-9]?[0-9])|(M?([12] )?([1357]/1?[0-9])))SM)|" \
    "([0-9]{4}(N|NE|E|SE|S|SW|W|NW( [0-9]{4}(N|NE|E|SE|S|SW|W|NW))?)?)|" \
    "CAVOK"
#define COND_RE_STR  "(-|\\+)?(VC|MI|BC|PR|TS|BL|SH|DR|FZ)?(DZ|RA|SN|SG|IC|PE|GR|GS|UP|BR|FG|FU|VA|SA|HZ|PY|DU|SQ|SS|DS|PO|\\+?FC)"
#define CLOUD_RE_STR "((CLR|BKN|SCT|FEW|OVC|SKC|NSC)([0-9]{3}|///)?(CB|TCU|///)?)"
#define TEMP_RE_STR  "(M?[0-9][0-9])/(M?(//|[0-9][0-9])?)"
#define PRES_RE_STR  "(A|Q)([0-9]{4})"

/* POSIX regular expressions do not allow us to express "match whole words
 * only" in a simple way, so we have to wrap them all into
 *   (^| )(...regex...)( |$)
 */
#define RE_PREFIX "(^| )("
#define RE_SUFFIX ")( |$)"

static GRegex *metar_re[RE_NUM];
static void (*metar_f[RE_NUM]) (gchar *tokp, GWeatherInfo *info);

static void
metar_init_re (void)
{
    static gboolean initialized = FALSE;
    if (initialized)
        return;
    initialized = TRUE;

    metar_re[TIME_RE] = g_regex_new (RE_PREFIX TIME_RE_STR RE_SUFFIX, G_REGEX_OPTIMIZE, 0, NULL);
    metar_re[WIND_RE] = g_regex_new (RE_PREFIX WIND_RE_STR RE_SUFFIX, G_REGEX_OPTIMIZE, 0, NULL);
    metar_re[VIS_RE] = g_regex_new (RE_PREFIX VIS_RE_STR RE_SUFFIX, G_REGEX_OPTIMIZE, 0, NULL);
    metar_re[COND_RE] = g_regex_new (RE_PREFIX COND_RE_STR RE_SUFFIX, G_REGEX_OPTIMIZE, 0, NULL);
    metar_re[CLOUD_RE] = g_regex_new (RE_PREFIX CLOUD_RE_STR RE_SUFFIX, G_REGEX_OPTIMIZE, 0, NULL);
    metar_re[TEMP_RE] = g_regex_new (RE_PREFIX TEMP_RE_STR RE_SUFFIX, G_REGEX_OPTIMIZE, 0, NULL);
    metar_re[PRES_RE] = g_regex_new (RE_PREFIX PRES_RE_STR RE_SUFFIX, G_REGEX_OPTIMIZE, 0, NULL);

    metar_f[TIME_RE] = metar_tok_time;
    metar_f[WIND_RE] = metar_tok_wind;
    metar_f[VIS_RE] = metar_tok_vis;
    metar_f[COND_RE] = metar_tok_cond;
    metar_f[CLOUD_RE] = metar_tok_cloud;
    metar_f[TEMP_RE] = metar_tok_temp;
    metar_f[PRES_RE] = metar_tok_pres;
}

gboolean
metar_parse (gchar *metar, GWeatherInfo *info)
{
    gchar *p;
    //gchar *rmk;
    gint i, i2;
    gchar *tokp;

    g_return_val_if_fail (info != NULL, FALSE);
    g_return_val_if_fail (metar != NULL, FALSE);

    g_debug ("About to metar_parse: %s", metar);

    metar_init_re ();

    /*
     * Force parsing to end at "RMK" field.  This prevents a subtle
     * problem when info within the remark happens to match an earlier state
     * and, as a result, throws off all the remaining expression
     */
    if (0 != (p = strstr (metar, " RMK "))) {
        *p = '\0';
        //rmk = p + 5;   // uncomment this if RMK data becomes useful
    }

    p = metar;
    i = TIME_RE;
    while (*p) {
        int token_start, token_end;

        i2 = RE_NUM;
        token_start = strlen(p);
        token_end = token_start;

        for (i = 0; i < RE_NUM; i++) {
            GMatchInfo *match_info;

            if (g_regex_match_full (metar_re[i], p, -1, 0, 0, &match_info, NULL))
            {
                int tmp_token_start, tmp_token_end;
                /* Skip leading and trailing space characters, if present.
                   (the regular expressions include those characters to
                   only get matches limited to whole words). */
                g_match_info_fetch_pos (match_info, 0, &tmp_token_start, &tmp_token_end);
                if (p[tmp_token_start] == ' ') tmp_token_start++;
                if (p[tmp_token_end - 1] == ' ') tmp_token_end--;

                /* choose the regular expression with the earliest match */
                if (tmp_token_start < token_start) {
                    i2 = i;
                    token_start = tmp_token_start;
                    token_end = tmp_token_end;
                }
            }

            g_match_info_unref (match_info);
        }

        if (i2 != RE_NUM) {
            tokp = g_strndup (p + token_start, token_end - token_start);
            metar_f[i2] (tokp, info);
            g_free (tokp);
        }

        p += token_end;
        p += strspn (p, " ");
    }
    return TRUE;
}

static void
metar_finish (SoupSession *session, SoupMessage *msg, gpointer data)
{
    GWeatherInfo *info = (GWeatherInfo *)data;
    GWeatherInfoPrivate *priv;
    WeatherLocation *loc;
    const gchar *p, *eoln;
    gchar *searchkey, *metar;
    gboolean success = FALSE;

    g_return_if_fail (info != NULL);

    priv = info->priv;
   
    if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
	if (SOUP_STATUS_IS_TRANSPORT_ERROR (msg->status_code))
	    priv->network_error = TRUE;
	else {
	    if (msg->status_code != SOUP_STATUS_CANCELLED)
		/* Translators: %d is an error code, and %s the error string */
		g_warning (_("Failed to get METAR data: %d %s.\n"),
			   msg->status_code, msg->reason_phrase);
	}

	_gweather_info_request_done (info, msg);
	return;
    }

    loc = &priv->location;

    g_debug ("METAR data for %s", loc->code);
    g_debug ("%s", msg->response_body->data);

    searchkey = g_strdup_printf ("<raw_text>%s ", loc->code);
    p = strstr (msg->response_body->data, searchkey);

    if (p) {
	p += strlen (searchkey);
	eoln = strstr (p, "</raw_text>");
	if (eoln)
	    metar = g_strndup (p, eoln - p);
	else
	    metar = g_strdup (p);
	success = metar_parse (metar, info);
	g_free (metar);
    } else if (!strstr (msg->response_body->data, "aviationweather.gov")) {
	/* The response doesn't even seem to have come from NOAA...
	 * most likely it is a wifi hotspot login page. Call that a
	 * network error.
	 */
	priv->network_error = TRUE;
    }

    g_free (searchkey);

    priv->valid = success;
    _gweather_info_request_done (info, msg);
}

/* Read current conditions and fill in info structure */
void
metar_start_open (GWeatherInfo *info)
{
    GWeatherInfoPrivate *priv;
    WeatherLocation *loc;
    SoupMessage *msg;

    g_return_if_fail (info != NULL);

    priv = info->priv;

    priv->valid = priv->network_error = FALSE;
    loc = &priv->location;

    if (!loc->latlon_valid)
        return;

    g_debug ("metar_start_open, requesting: https://www.aviationweather.gov/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&hoursBeforeNow=3&mostRecent=true&fields=raw_text&stationString=%s", loc->code);
    msg = soup_form_request_new (
	"GET", "https://www.aviationweather.gov/adds/dataserver_current/httpparam",
	"dataSource", "metars",
	"requestType", "retrieve",
	"format", "xml",
	"hoursBeforeNow", "3",
	"mostRecent", "true",
	"fields", "raw_text",
	"stationString", loc->code,
	NULL);
    _gweather_info_begin_request (info, msg);
    soup_session_queue_message (priv->session, msg, metar_finish, info);
}
