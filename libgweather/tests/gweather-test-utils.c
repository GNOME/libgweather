/* gweather-test-utils.c: Utility API for GWeather tests
 *
 * SPDX-FileCopyrightText: 2021  Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gweather-test-utils.h"

#include <glib/gstdio.h>

/* We use internal API */
#include "gweather-private.h"

extern void
_gweather_location_reset_world (void);

void
gweather_test_reset_world (void)
{
    _gweather_location_reset_world ();
}

/* Set up the temporary directory with the GSettings schemas */
char *
gweather_test_setup_gsettings (void)
{
    char *tmpdir, *schema_text, *dest, *cmdline;
    int result;

    /* Create the installed schemas directory */
    GError *error = NULL;
    tmpdir = g_dir_make_tmp ("libgweather-test-XXXXXX", &error);
    g_assert_no_error (error);

    g_test_message ("Using temporary directory: %s", tmpdir);

    /* Copy the schemas files */
    g_assert_true (g_file_get_contents (SCHEMAS_BUILDDIR "/org.gnome.GWeather4.enums.xml", &schema_text, NULL, NULL));
    dest = g_build_filename (tmpdir, "org.gnome.GWeather4.enums.xml", NULL);
    g_assert_true (g_file_set_contents (dest, schema_text, -1, NULL));
    g_free (dest);
    g_free (schema_text);

    g_assert_true (g_file_get_contents (SCHEMASDIR "/org.gnome.GWeather4.gschema.xml", &schema_text, NULL, NULL));
    dest = g_build_filename (tmpdir, "org.gnome.GWeather4.gschema.xml", NULL);
    g_assert_true (g_file_set_contents (dest, schema_text, -1, NULL));
    g_free (dest);
    g_free (schema_text);

    /* Compile the schemas */
    cmdline = g_strdup_printf ("glib-compile-schemas --targetdir=%s "
                               "--schema-file=%s/org.gnome.GWeather4.enums.xml "
                               "--schema-file=%s/org.gnome.GWeather4.gschema.xml",
                               tmpdir,
                               SCHEMAS_BUILDDIR,
                               SCHEMASDIR);
    g_assert_true (g_spawn_command_line_sync (cmdline, NULL, NULL, &result, NULL));
    g_assert_cmpint (result, ==, 0);
    g_free (cmdline);

    /* Set envvar */
    g_setenv ("GSETTINGS_SCHEMA_DIR", tmpdir, TRUE);

    return tmpdir;
}

/* Tear down the temporary directory with the GSettings schemas */
void
gweather_test_teardown_gsettings (const char *schemas_dir)
{
    char *dest = NULL;

    dest = g_build_filename (schemas_dir, "org.gnome.GWeather4.enums.xml", NULL);
    g_assert_no_errno (g_unlink (dest));
    g_free (dest);

    dest = g_build_filename (schemas_dir, "org.gnome.GWeather4.gschema.xml", NULL);
    g_assert_no_errno (g_unlink (dest));
    g_free (dest);

    dest = g_build_filename (schemas_dir, "gschemas.compiled", NULL);
    g_assert_no_errno (g_unlink (dest));
    g_free (dest);

    g_assert_no_errno (g_rmdir (schemas_dir));
}
