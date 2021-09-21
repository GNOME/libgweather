project('libgweather', 'c',
  version: '3.90.0',
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

glib_req_version = '>= 2.44.0'
libxml_req_version = '>= 2.6.0'

GETTEXT_PACKAGE = libgweather_full_version
LOCATIONS_GETTEXT_PACKAGE = libgweather_full_version + '-locations'
gtk_req_version = '>= 3.13.5'
libxml_req_version = '>= 2.6.0'

if get_option('soup2')
  libsoup_name = 'libsoup-2.4'
  libsoup_req_version = '>= 2.44.0'
  libsoup_api_version = '2.4'
else
  libsoup_name = 'libsoup-3.0'
  libsoup_req_version = '>= 2.99.2'
  libsoup_api_version = '3.0'
endif

c_compiler = meson.get_compiler('c')
gtk_dep = dependency('gtk+-3.0', version: gtk_req_version)
glib_dep = dependency('gio-2.0', version: glib_req_version)
libsoup_dep = dependency(libsoup_name, version: libsoup_req_version)
libxml_dep = dependency('libxml-2.0', version: libxml_req_version)
geocode_glib_dep = dependency('geocode-glib-1.0')
math_dep = c_compiler.find_library('m', required : false)
deps_libgweather = [math_dep, gtk_dep, glib_dep, libsoup_dep, libxml_dep, geocode_glib_dep]

geocode_soup_version = geocode_glib_dep.get_pkgconfig_variable('soupapiversion')
if geocode_soup_version == ''
  geocode_soup_version = '2.4' # Pre libsoup3 support
endif
if geocode_soup_version != libsoup_api_version
  error('geocode-glib was built against a different API of libsoup. @0@ instead of @1@.'.format(geocode_soup_version, libsoup_api_version))
endif

pylint = find_program('pylint-3', 'pylint3', 'pylint', required: false)
pylint_flags = [
  '-d', 'C0111',
  '-d', 'W0511',
  '-d', 'F0401',
  '-d', 'C0326',
]

gen_locations_variant = find_program('build-aux/meson/gen_locations_variant.py')

subdir('data')
subdir('libgweather')
subdir('schemas')
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
    'Introspection': build_gir,
    'Generate VAPI': build_vapi,
    'API reference': get_option('gtk_doc'),
  },
  section: 'Build',
  bool_yn: true,
)
