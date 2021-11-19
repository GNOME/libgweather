/* gweather-test-utils.h: Utility API for GWeather tests
 *
 * SPDX-FileCopyrightText: 2021  Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib.h>
#include <locale.h>
#include <string.h>

#include <libgweather/gweather.h>

G_BEGIN_DECLS

void
gweather_test_reset_world (void);

/* Set up the temporary directory with the GSettings schemas */
char *
gweather_test_setup_gsettings (void);

void
gweather_test_teardown_gsettings (const char *schemas_dir);

G_END_DECLS
