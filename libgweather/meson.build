add_project_arguments([
  '-DGWEATHER_COMPILATION',
  '-D_XOPEN_SOURCE=700',
  '-D_DEFAULT_SOURCE',
], language: 'c')

c_compiler = meson.get_compiler('c')

add_project_arguments(c_compiler.get_supported_arguments([
  # We use G_DECLARE_* macros
  '-Wno-typedef-redefinition',

  # We have legacy code and callbacks
  '-Wno-unused-parameter',

  # We are using GObject
  '-fno-strict-aliasing',

  # Warnings we care about
  '-Wimplicit-fallthrough',
  '-Wmisleading-indentation',
  '-Wstrict-prototypes',
  '-Wunused',

  # We guarantee that our code base does not fail these
  '-Werror=format=2',
  '-Werror=implicit-function-declaration',
  '-Werror=init-self',
  '-Werror=missing-include-dirs',
]), language: 'c')

header_subdir = libgweather_full_version / 'libgweather'

libgweather_major = libgweather_version[0].to_int()
libgweather_minor = libgweather_version[1].to_int()
libgweather_micro = libgweather_version[2].to_int()
interface_age = libgweather_minor.is_odd() ? 0 : libgweather_micro
binary_age = libgweather_minor * 100 + libgweather_micro
current = binary_age - interface_age

libgweather_soversion = 0
libgweather_library_version = '@0@.@1@.@2@'.format(libgweather_soversion, current, interface_age)

libgweather_darwin_versions = [
  current + 1,
  '@0@.@1@'.format(current + 1, interface_age),
]

gweather_pc = 'gweather@0@'.format(libgweather_api)

config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', GETTEXT_PACKAGE)
config_h.set_quoted('LOCATIONS_GETTEXT_PACKAGE', LOCATIONS_GETTEXT_PACKAGE)
config_h.set_quoted('LOCALEDIR', datadir / 'locale')
config_h.set_quoted('GNOMELOCALEDIR', datadir / 'locale')
config_h.set_quoted('G_LOG_DOMAIN', 'GWeather')
config_h.set_quoted('GWEATHER_BIN_LOCATION_DIR', pkglibdir)
config_h.set_quoted('LIBGWEATHER_VERSION', meson.project_version())

if c_compiler.has_header('alloca.h')
  config_h.set('HAVE_ALLOCA_H', 1)
endif

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

config_h.set_quoted('GWEATHER_LOCATIONS_DB', locations_bin)

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
versionconf.set('GWEATHER_MAJOR_VERSION', libgweather_major)
versionconf.set('GWEATHER_MINOR_VERSION', libgweather_minor)
versionconf.set('GWEATHER_MICRO_VERSION', libgweather_micro)
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
  dependency('libsoup-3.0', version: libsoup_req_version),
  dependency('libxml-2.0', version: libxml_req_version),
  dependency('geocode-glib-2.0'),
  dependency('json-glib-1.0'),

  c_compiler.find_library('m', required: false),
]

gweather_headers = [
  'gweather-info.h',
  'gweather-location.h',
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
  'gweather.c',
  'gweather-info.c',
  'gweather-location.c',
]

gweather_priv_sources = [
  'third-party/kdtree.c',
  'gweather-private.c',
  'weather-metar.c',
  'weather-iwin.c',
  'weather-metno.c',
  'weather-owm.c',
  'weather-nws.c',
  'weather-sun.c',
  'weather-moon.c',
]

introspection_sources = [
  gweather_enum_types,
  gweather_c_sources,
  gweather_headers,
]

libgweather_static = static_library('gweather',
  sources: [
    gweather_enum_types,
    gweather_c_sources,
    gweather_priv_sources,
  ],
  dependencies: deps_libgweather,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
    include_directories('third-party'),
  ],
)

libgweather_static_dep = declare_dependency(
  sources: gweather_enum_types,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
    include_directories('third-party'),
  ],
  dependencies: deps_libgweather,
  link_with: libgweather_static,
)

lib_libgweather = library('gweather-4',
  dependencies: deps_libgweather,
  link_whole: libgweather_static,
  soversion: libgweather_soversion,
  version: libgweather_library_version,
  gnu_symbol_visibility: 'hidden',
  darwin_versions: libgweather_darwin_versions,
  include_directories: [
    include_directories('..'),
  ],
  install: true,
)

enable_vala = get_option('enable_vala')
vapigen = dependency('vapigen', required: enable_vala == 'true')
if enable_vala == 'auto' or enable_vala == 'true'
  build_vapi = vapigen.found() and get_option('introspection')
  if enable_vala == 'true' and not build_vapi
    if not vapigen.found()
      msg = 'vapigen not found'
    elif not get_option('introspection')
      msg = 'introspection disabled'
    endif
    error('Vala bindings were enabled but cannot be generated: @0@'.format(msg))
  endif
else
  build_vapi = false
endif

if build_gir
  gweather_gir = gnome.generate_gir(lib_libgweather,
    sources: introspection_sources,
    dependencies: deps_libgweather,
    nsversion: libgweather_api_version,
    namespace: 'GWeather',
    includes: ['Gio-2.0'],
    symbol_prefix: 'gweather',
    identifier_prefix: 'GWeather',
    export_packages: gweather_pc,
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
  gnome.generate_vapi(gweather_pc,
    sources: gweather_gir[0],
    packages: ['gio-2.0'],
    metadata_dirs: '.',
    install: true,
  )
endif

pkgconfig.generate(
  lib_libgweather,
  filebase: gweather_pc,
  name: 'GWeather',
  description: 'Gather weather information from online services',
  version: meson.project_version(),
  subdirs: libgweather_full_version,
  variables: [
    'soupapiversion=3.0'
  ]
)

if get_option('tests')
  subdir('tests')
  subdir('tools')

  clang_format = find_program('clang-format', required: false)
  if clang_format.found()
    test('style-format', clang_format,
      args: ['--dry-run', '--Werror', gweather_c_sources],
      workdir: meson.current_source_dir(),
      suite: ['style'],
    )
  endif
endif
