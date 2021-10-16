/* gweather-private.c - Overall weather server functions
 *
 * SPDX-FileCopyrightText: The GWeather authors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "gweather-private.h"

/* sign, 3 digits, separator, 4 decimals, nul-char */
#define DEGREES_STR_SIZE (1 + 3 + 1 + 4 + 1)

char *
_radians_to_degrees_str (gdouble radians)
{
    char *str;
    double degrees;

    str = g_malloc0 (DEGREES_STR_SIZE);
    /* Max 4 decimals */
    degrees = (double) ((int) (RADIANS_TO_DEGREES (radians) * 10000)) / 10000;
    /* Too many digits */
    g_return_val_if_fail (degrees <= 1000 || degrees >= -1000, NULL);
    return g_ascii_formatd (str, G_ASCII_DTOSTR_BUF_SIZE, "%g", degrees);
}
