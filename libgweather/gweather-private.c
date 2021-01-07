/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* weather.c - Overall weather server functions
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
 * <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
