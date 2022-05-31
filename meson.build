project('libgweather', 'c',
  version: '4.1.0',
  license: 'GPL-2.0-or-later',
  meson_version: '>= 0.55.0',
  default_options: [
    'warning_level=2',
    'buildtype=debugoptimized',
    'c_std=gnu99',
  ],
)

libgweather_version = meson.project_version().split('.')
libgweather_api = 4
libgweather_api_version = '@0@.0'.format(libgweather_api)
libgweather_full_version = '@0@-@1@'.format(meson.project_name(), libgweather_api_version)

pkgconfig = import('pkgconfig')
gnome = import('gnome')
i18n = import('i18n')

prefix = get_option('prefix')

bindir = prefix / get_option('bindir')
datadir = prefix / get_option('datadir')
libdir = prefix / get_option('libdir')
includedir = prefix / get_option('includedir')
libexecdir = prefix / get_option('libexecdir')
sysconfdir = prefix / get_option('sysconfdir')

pkgdatadir = datadir / 'libgweather-4'
pkglibdir = libdir / 'libgweather-4'

glib_req_version = '>= 2.68.0'
libxml_req_version = '>= 2.6.0'

GETTEXT_PACKAGE = libgweather_full_version
LOCATIONS_GETTEXT_PACKAGE = libgweather_full_version + '-locations'

c_compiler = meson.get_compiler('c')

pylint = find_program('pylint-3', 'pylint3', 'pylint', required: false)
pylint_flags = [
  '-d', 'C0111',
  '-d', 'W0511',
  '-d', 'F0401',
  '-d', 'C0326',
]

py = import('python').find_installation('python3', modules: ['gi'])
gen_locations_variant = find_program('build-aux/meson/gen_locations_variant.py')

subdir('data')
subdir('schemas')
subdir('libgweather')
subdir('doc')
subdir('po')
subdir('po-locations')

meson.add_install_script('build-aux/meson/meson_post_install.py')

summary({
    'prefix': prefix,
    'libdir': libdir,
    'datadir': datadir,
    'zone info': get_option('zoneinfo_dir'),
  },
  section: 'Directories',
)

summary({
    'Debug': get_option('debug'),
    'Optimization': get_option('optimization'),
    'Use libsoup-2': get_option('soup2'),
    'Introspection': build_gir,
    'Generate VAPI': build_vapi,
    'API reference': get_option('gtk_doc'),
  },
  section: 'Build',
  bool_yn: true,
)
