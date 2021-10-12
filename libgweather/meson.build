add_project_arguments([
  '-DGWEATHER_COMPILATION',
  '-D_XOPEN_SOURCE=700',
  '-D_DEFAULT_SOURCE',
], language: 'c')

c_compiler = meson.get_compiler('c')

add_project_arguments(c_compiler.get_supported_arguments([
  # Warnings we care about
  '-Wimplicit-fallthrough',
  '-Wmisleading-indentation',
  '-Wstrict-prototypes',
  '-Wunused',

  # We have legacy code and callbacks
  '-Wno-unused-parameter',

  '-Werror=format=2',
  '-Werror=implicit-function-declaration',
  '-Werror=init-self',
  '-Werror=missing-include-dirs',
]), language: 'c')

header_subdir = 'libgweather-3.0/libgweather'

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

if host_machine.system() == 'windows'
  visibility_define = '__declspec(dllexport) extern'
else
  visibility_define = '__attribute__((visibility("default"))) extern'
endif

if get_option('default_library') != 'static'
  config_h.set('_GWEATHER_EXTERN', visibility_define)
  if host_machine.system() == 'windows'
    config_h.set('DLL_EXPORT', true)
  endif
endif

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

libgweather_static = static_library('gweather',
  sources: [
    gweather_c_sources,
    gweather_priv_sources,
  ],
  dependencies: deps_libgweather,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
  ],
)

libgweather_static_dep = declare_dependency(sources: gweather_c_sources,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
  ],
  dependencies: deps_libgweather,
  link_with: libgweather_static,
)

lib_libgweather = shared_library('gweather-3',
  dependencies: deps_libgweather,
  link_whole: libgweather_static,
  version: libgweather_so_version,
  gnu_symbol_visibility: 'hidden',
  darwin_versions: libgweather_darwin_versions,
  include_directories: [
    include_directories('..'),
  ],
  install: true,
)

g_ir_scanner = find_program('g-ir-scanner', required: get_option('introspection'))
build_gir = get_option('introspection') and g_ir_scanner.found() and not meson.is_cross_build()

enable_vala = get_option('enable_vala')
vapigen = find_program('vapigen', required: enable_vala == 'true')
if enable_vala == 'auto' or enable_vala == 'true'
  build_vapi = vapigen.found() and get_option('introspection')
else
  build_vapi = false
endif

if build_gir
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
else
  gweather_gir = []
endif

libgweather_dep = declare_dependency(
  sources: [gweather_enum_types[1], gweather_gir],
  dependencies: deps_libgweather,
  link_with: lib_libgweather,
  include_directories: [
    include_directories('..'),
  ],
)

if build_vapi and build_gir
  gnome.generate_vapi('gweather-3.0',
    sources: gweather_gir[0],
    packages: ['gobject-2.0', 'gtk+-3.0'],
    metadata_dirs: '.',
    install: true,
  )
endif

subdir('tests')
subdir('tools')
