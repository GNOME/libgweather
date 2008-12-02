/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* weather-sun.c - Astronomy calculations for gweather
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

/*
 * Formulas from:
 * "Practical Astronomy With Your Calculator" (3e), Peter Duffett-Smith
 * Cambridge University Press 1988
 *
 * Planetary Mean Orbit parameters from http://ssd.jpl.nasa.gov/elem_planets.html,
 * converting longitudes from heliocentric to geocentric coordinates
 * epoch J2000 (2000 Jan 1 12:00:00 UTC)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <time.h>
#include <glib.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include "weather-priv.h"

#ifdef G_OS_WIN32
#include "gweather-win32.h"
#endif

#define EPOCH_TO_J2000(t)       (t - 946728000)
#define MEAN_ECLIPTIC_LONGITUDE 280.46435
#define PERIGEE_LONGITUDE       282.94719
#define ECCENTRICITY            0.01671002
#define SOL_PROGRESSION         (360./365.242191)

/*
 * Convert ecliptic longitude (radians) to equitorial coordinates,
 * expressed as right ascension (hours) and declination (radians)
 * Assume ecliptic latitude is 0.0
 */
static void
ecl2equ (gdouble time, gdouble eclipLon,
	 gdouble *ra, gdouble *decl)
{
    gdouble jc = EPOCH_TO_J2000 (time) / (36525. * 86400.);
    gdouble mEclipObliq = DEGREES_TO_RADIANS (23.439291667
					      - .013004166 * jc
					      - 1.666667e-7 * jc * jc
					      + 5.027777e-7 * jc * jc * jc);
    *ra = RADIANS_TO_HOURS (atan2 (sin (eclipLon) * cos (mEclipObliq),
				   cos (eclipLon)));
    if (*ra < 0.)
        *ra += 24.;
    *decl = asin (sin (mEclipObliq) * sin (eclipLon));
}

/*
 * Calculate rising and setting times of an object
 * based on it equitorial coordinates
 */
static void
gstObsv (gdouble ra, gdouble decl,
	 gdouble obsLat, gdouble obsLon,
	 gdouble *rise, gdouble *set)
{
    double a = acos (-tan (obsLat) * tan (decl));
    double b;

    if (isnan (a) != 0) {
	*set = *rise = a;
	return;
    }
    a = RADIANS_TO_HOURS (a);
    b = 24. - a + ra;
    a += ra;
    a -= RADIANS_TO_HOURS (obsLon);
    b -= RADIANS_TO_HOURS (obsLon);
    if ((a = fmod (a, 24.)) < 0)
	a += 24.;
    if ((b = fmod (b, 24.)) < 0)
	b += 24.;

    *set = a;
    *rise = b;
}


static gdouble
t0 (time_t date)
{
    gdouble t = ((gdouble)(EPOCH_TO_J2000 (date) / 86400)) / 36525.0;
    gdouble t0 = fmod (6.697374558 + 2400.051366 * t + 2.5862e-5 * t * t, 24.);
    if (t0 < 0.)
        t0 += 24.;
    return t0;
}


static gboolean
calc_sun2 (time_t t, gdouble obsLat, gdouble obsLon,
	   time_t *rise, time_t *set)
{
    time_t gm_midn;
    time_t lcl_midn;
    gdouble gm_hoff, ndays, meanAnom, eccenAnom, delta, lambda;
    gdouble ra1, ra2, decl1, decl2;
    gdouble rise1, rise2, set1, set2;
    gdouble tt, t00;
    gdouble x, u, dt;

    /* Approximate preceding local midnight at observer's longitude */
    gm_midn = t - (t % 86400);
    gm_hoff = floor ((RADIANS_TO_DEGREES (obsLon) + 7.5) / 15.);
    lcl_midn = gm_midn - 3600. * gm_hoff;
    if (t - lcl_midn >= 86400)
        lcl_midn += 86400;
    else if (lcl_midn > t)
        lcl_midn -= 86400;

    /*
     * Ecliptic longitude of the sun at midnight local time
     * Start with an estimate based on a fixed daily rate
     */
    ndays = EPOCH_TO_J2000 (lcl_midn) / 86400.;
    meanAnom = DEGREES_TO_RADIANS (ndays * SOL_PROGRESSION
				   + MEAN_ECLIPTIC_LONGITUDE - PERIGEE_LONGITUDE);

    /*
     * Approximate solution of Kepler's equation:
     * Find E which satisfies  E - e sin(E) = M (mean anomaly)
     */
    eccenAnom = meanAnom;

    while (1e-12 < fabs (delta = eccenAnom - ECCENTRICITY * sin (eccenAnom) - meanAnom))
	eccenAnom -= delta / (1. - ECCENTRICITY * cos (eccenAnom));

    /* Sun's longitude on the ecliptic */
    lambda = fmod (DEGREES_TO_RADIANS (PERIGEE_LONGITUDE)
		   + 2. * atan (sqrt ((1. + ECCENTRICITY) / (1. - ECCENTRICITY))
				* tan (eccenAnom / 2.)),
		   2. * M_PI);

    /* Calculate equitorial coordinates of sun at previous and next local midnights */

    ecl2equ (lcl_midn, lambda, &ra1, &decl1);
    ecl2equ (lcl_midn + 86400., lambda + DEGREES_TO_RADIANS (SOL_PROGRESSION), &ra2, &decl2);

    /* Convert to rise and set times assuming the earth were to stay in its position
     * in its orbit around the sun.  This will soon be followed by interpolating
     * between the two
     */

    gstObsv (ra1, decl1, obsLat, obsLon - (gm_hoff * M_PI / 12.), &rise1, &set1);
    gstObsv (ra2, decl2, obsLat, obsLon - (gm_hoff * M_PI / 12.), &rise2, &set2);

    /* TODO: include calculations for regions near the poles. */
    if (isnan (rise1) || isnan (rise2))
        return FALSE;

    if (rise2 < rise1)
        rise2 += 24.;
    if (set2 < set1)
        set2 += 24.;

    tt = t0 (lcl_midn);
    t00 = tt - (gm_hoff + RADIANS_TO_HOURS (obsLon)) * 1.002737909;

    if (t00 < 0.)
	t00 += 24.;

    if (rise1 < t00) {
        rise1 += 24.;
        rise2 += 24.;
    }
    if (set1 < t00) {
        set1  += 24.;
        set2  += 24.;
    }

    /*
     * Interpolate between the two
     */
    rise1 = (24.07 * rise1 - t00 * (rise2 - rise1)) / (24.07 + rise1 - rise2);
    set1 = (24.07 * set1 - t00 * (set2 - set1)) / (24.07 + set1 - set2);

    decl2 = (decl1 + decl2) / 2.;
    x = DEGREES_TO_RADIANS (0.830725);
    u = acos ( sin (obsLat) / cos (decl2) );
    dt =  RADIANS_TO_HOURS (asin (sin (x) / sin (u)) / cos (decl2));

    rise1 = (rise1 - dt - tt) * 0.9972695661;
    set1  = (set1 + dt - tt) * 0.9972695661;
    if (rise1 < 0.)
	rise1 += 24;
    else if (rise1 >= 24.)
	rise1 -= 24.;
    if (set1 < 0.)
	set1 += 24;
    else if (set1 >= 24.)
	set1 -= 24.;

    *rise = (rise1 * 3600.) + lcl_midn;
    *set = (set1 * 3600.) + lcl_midn;
    return TRUE;
}


gboolean
calc_sun (WeatherInfo *info)
{
    time_t now = time (NULL);

    return info->location->latlon_valid
	&& calc_sun2 (now, info->location->latitude, info->location->longitude,
		      &info->sunrise, &info->sunset);
}


/*
 * Returns the interval, in seconds, until the next "sun event":
 *  - local midnight, when rise and set times are recomputed
 *  - next sunrise, when icon changes to daytime version
 *  - next sunset, when icon changes to nighttime version
 */
gint
weather_info_next_sun_event (WeatherInfo *info)
{
    time_t    now = time (NULL);
    struct tm ltm;
    time_t    nxtEvent;

    g_return_val_if_fail (info != NULL, -1);

    if (!calc_sun (info))
	return -1;

    /* Determine when the next local midnight occurs */
    (void) localtime_r (&now, &ltm);
    ltm.tm_sec = 0;
    ltm.tm_min = 0;
    ltm.tm_hour = 0;
    ltm.tm_mday++;
    nxtEvent = mktime (&ltm);

    if (info->sunset > now && info->sunset < nxtEvent)
	nxtEvent = info->sunset;
    if (info->sunrise > now && info->sunrise < nxtEvent)
	nxtEvent = info->sunrise;
    return (gint)(nxtEvent - now);
}
