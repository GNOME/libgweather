add_project_arguments([
  '-DHAVE_CONFIG_H',
  '-DGWEATHER_COMPILATION',
  '-D_XOPEN_SOURCE=700',
  '-D_DEFAULT_SOURCE',
], language: 'c')

c_compiler = meson.get_compiler('c')

header_subdir = 'libgweather-3.0/libgweather'
libgweather_map = meson.current_source_dir() / 'libgweather.map'
link_depends = []
link_args = []

# convert to soname
current = libgweather_lt_c - libgweather_lt_a
interface_age = libgweather_lt_r
libgweather_so_version = '@0@.@1@.@2@'.format(
  current,
  libgweather_lt_a,
  interface_age,
)

libgweather_darwin_versions = [
  current + 1,
  '@0@.@1@'.format(current + 1, interface_age),
]

if c_compiler.has_link_argument('-Wl,--version-script,' + libgweather_map)
  link_depends += libgweather_map
  link_args += ['-Wl,--version-script,' + libgweather_map]
endif

config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', GETTEXT_PACKAGE)
config_h.set_quoted('LOCALEDIR', datadir / 'locale')
config_h.set_quoted('GNOMELOCALEDIR', datadir / 'locale')
config_h.set_quoted('G_LOG_DOMAIN', 'GWeather')
config_h.set_quoted('GWEATHER_BIN_LOCATION_DIR', pkglibdir)
config_h.set_quoted('LIBGWEATHER_VERSION', meson.project_version())

if c_compiler.has_member('struct tm', 'tm_gmtoff', prefix: '#include <time.h>')
  config_h.set('HAVE_TM_TM_GMOFF', 1)
endif

if c_compiler.has_header_symbol('time.h', 'timezone')
  config_h.set('HAVE_TIMEZONE', 1)
endif

if c_compiler.has_header_symbol('langinfo.h', '_NL_MEASUREMENT_MEASUREMENT')
  config_h.set('HAVE__NL_MEASUREMENT_MEASUREMENT', 1)
endif

config_h.set_quoted('ZONEINFO_DIR', get_option('zoneinfo_dir'))
config_h.set_quoted('OWM_APIKEY', get_option('owm_apikey'))

configure_file(
  output: 'config.h',
  configuration: config_h,
)

versionconf = configuration_data()
versionconf.set('GWEATHER_MAJOR_VERSION', libgweather_version[0])
versionconf.set('GWEATHER_MINOR_VERSION', libgweather_version[1])
versionconf.set('GWEATHER_MICRO_VERSION', libgweather_version[2])
versionconf.set_quoted('GWEATHER_VERSION', meson.project_version())
configure_file(
  input: 'gweather-version.h.in',
  output: 'gweather-version.h',
  configuration: versionconf,
  install: true,
  install_dir: includedir / header_subdir,
)

deps_libgweather = [
  dependency('gio-2.0', version: glib_req_version),
  dependency('gtk+-3.0', version: gtk_req_version),
  dependency('libsoup-2.4', version: libsoup_req_version),
  dependency('libxml-2.0', version: libxml_req_version),
  dependency('geocode-glib-1.0'),

  c_compiler.find_library('m', required: false),
]

gweather_headers = [
  'gweather-location.h',
  'gweather-location-entry.h',
  'gweather-timezone.h',
  'gweather-timezone-menu.h',
  'gweather-weather.h',
  'gweather-enums.h'
]

gweather_enum_types = gnome.mkenums('gweather-enum-types',
  sources: gweather_headers,
  c_template: 'gweather-enum-types.c.tmpl',
  h_template: 'gweather-enum-types.h.tmpl',
  install_header: true,
  install_dir: includedir / header_subdir,
)

install_headers(gweather_headers + ['gweather.h'], subdir: header_subdir)

gweather_c_sources = [
  gweather_enum_types,
  'gweather.c',
  'gweather-weather.c',
  'gweather-location.c',
  'gweather-timezone.c',
  'gweather-location-entry.c',
  'gweather-timezone-menu.c',
]

gweather_priv_sources = [
  'gweather-private.c',
  'weather-metar.c',
  'weather-iwin.c',
  'weather-metno.c',
  'weather-owm.c',
  'weather-sun.c',
  'weather-moon.c',
]

introspection_sources = gweather_c_sources + gweather_headers

lib_libgweather = shared_library('gweather-3',
  sources: [
    gweather_c_sources,
    gweather_priv_sources,
  ],
  dependencies: deps_libgweather,
  link_depends: link_depends,
  link_args: link_args,
  version: libgweather_so_version,
  darwin_versions: libgweather_darwin_versions,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
  ],
  install: true,
)

gweather_gir = gnome.generate_gir(lib_libgweather,
  sources: introspection_sources,
  dependencies: deps_libgweather,
  nsversion: '3.0',
  namespace: 'GWeather',
  includes: ['GObject-2.0', 'Gtk-3.0'],
  symbol_prefix: 'gweather',
  identifier_prefix: 'GWeather',
  export_packages: 'gweather-3.0',
  header: 'libgweather/gweather.h',
  extra_args: [
    '--warn-all',
    '--quiet',
    '-DGWEATHER_COMPILATION',
  ],
  fatal_warnings: get_option('werror'),
  install: true,
)

libgweather_dep = declare_dependency(
  sources: [gweather_enum_types[1], gweather_gir],
  dependencies: deps_libgweather,
  link_with: lib_libgweather,
  include_directories: include_directories('..'),
)

if enable_vala
  gnome.generate_vapi('gweather-3.0',
    sources: gweather_gir[0],
    packages: ['gobject-2.0', 'gtk+-3.0'],
    metadata_dirs: '.',
    install: true,
  )
endif

test_cargs = [
  '-DTEST_LOCATIONS="@0@"'.format(locations_bin.full_path()),
  '-DSCHEMASDIR="@0@/schemas"'.format(meson.source_root()),
  '-DSCHEMAS_BUILDDIR="@0@/schemas"'.format(meson.build_root()),
]

executable('test_locations',
  sources: ['test_locations.c'],
  c_args: test_cargs,
  dependencies: libgweather_dep,
  install: false,
)

executable('test_locations_utc',
  sources: ['test_locations_utc.c'],
  c_args: test_cargs,
  dependencies: libgweather_dep,
  install: false,
)

test('test_libgweather',
  executable('test_libgweather',
    sources: ['test_libgweather.c'],
    c_args: test_cargs,
    dependencies: libgweather_dep,
    install: false,
  ),
  depends: [locations_bin],
)

executable('test_metar',
  sources: ['test_metar.c', gweather_c_sources, gweather_priv_sources],
  c_args: test_cargs,
  dependencies: libgweather_dep,
  install: false,
)

executable('test_sun_moon',
  sources: ['test_sun_moon.c', 'weather-sun.c', 'weather-moon.c'],
  c_args: test_cargs,
  dependencies: libgweather_dep,
  install: false,
)

executable('test_weather',
  sources: ['test_weather.c'],
  c_args: test_cargs,
  dependencies: libgweather_dep,
  install: false,
)
