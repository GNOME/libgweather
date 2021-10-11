#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2017 Giovanni Campagna
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import subprocess

prefix = os.environ['MESON_INSTALL_PREFIX']
schemadir = os.path.join(prefix, 'share', 'glib-2.0', 'schemas')

if not os.environ.get('DESTDIR'):
    print('Compiling gsettings schemas...')
    subprocess.call(['glib-compile-schemas', schemadir])
