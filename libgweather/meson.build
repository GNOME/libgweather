add_project_arguments([
  '-DHAVE_CONFIG_H',
  '-DGWEATHER_COMPILATION',
  '-D_XOPEN_SOURCE=700',
  '-D_DEFAULT_SOURCE',
], language: 'c')

header_subdir = 'libgweather-3.0/libgweather'
libgweather_map = join_paths(meson.current_source_dir(), 'libgweather.map')
link_depends = []
link_args = []

if c_compiler.has_link_argument('-Wl,--version-script,' + libgweather_map)
        link_depends += libgweather_map
        link_args += ['-Wl,--version-script,' + libgweather_map]
endif

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
  install_dir: join_paths(includedir, header_subdir),
)

gweather_new_headers = [
  'gweather.h',
  'gweather-location.h',
  'gweather-location-entry.h',
  'gweather-timezone.h',
  'gweather-timezone-menu.h',
  'gweather-weather.h',
  'gweather-enums.h'
]
gweather_enum_types = gnome.mkenums('gweather-enum-types',
  sources: gweather_new_headers,
  c_template: 'gweather-enum-types.c.tmpl',
  h_template: 'gweather-enum-types.h.tmpl',
  install_header: true,
  install_dir: join_paths(includedir, header_subdir))
install_headers(gweather_new_headers,
  subdir: header_subdir)

gweather_c_sources = [
  gweather_enum_types,
  'gweather.c',
  'gweather-private.c',
  'gweather-weather.c',
  'weather-metar.c',
  'weather-iwin.c',
  'weather-metno.c',
  'weather-owm.c',
  'weather-sun.c',
  'weather-moon.c',
  'gweather-location.c',
  'gweather-timezone.c',
  'gweather-location-entry.c',
  'gweather-timezone-menu.c']
introspection_sources = gweather_c_sources + gweather_new_headers

lib_libgweather = shared_library('gweather-3',
  gweather_c_sources,
  include_directories: root_inc,
  dependencies: deps_libgweather,
  link_depends: link_depends,
  link_args: link_args,
  version: libgweather_so_version,
  darwin_versions: libgweather_darwin_versions,
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
    '-DGWEATHER_COMPILATION',
  ],
  install: true,
)

libgweather_dep = declare_dependency(
  sources: [gweather_enum_types[1], gweather_gir],
  dependencies: deps_libgweather,
  link_with: lib_libgweather,
  include_directories: root_inc,
)

if enable_vala
  gnome.generate_vapi('gweather-3.0',
    sources: gweather_gir[0],
    packages: ['gobject-2.0', 'gtk+-3.0'],
    metadata_dirs: '.',
    install: true
  )
endif

test_cargs = ['-DTEST_LOCATIONS="@0@"'.format(locations_bin.full_path()),
              '-DSCHEMASDIR="@0@/schemas"'.format(meson.source_root()),
              '-DSCHEMAS_BUILDDIR="@0@/schemas"'.format(meson.build_root())]

executable('test_locations',
  ['test_locations.c'],
  c_args: test_cargs,
  dependencies: libgweather_dep,
  install: false)
executable('test_locations_utc',
  ['test_locations_utc.c'],
  c_args: test_cargs,
  dependencies: libgweather_dep,
  install: false)

exe = executable('test_libgweather',
  ['test_libgweather.c'],
  c_args: test_cargs,
  dependencies: libgweather_dep,
  install: false)
test('test_libgweather', exe, depends: [locations_bin])

executable('test_metar',
  ['test_metar.c', gweather_c_sources],
  c_args: test_cargs,
  dependencies: libgweather_dep,
  install: false)
executable('test_sun_moon',
  ['test_sun_moon.c', 'weather-sun.c', 'weather-moon.c'],
  c_args: test_cargs,
  dependencies: libgweather_dep,
  install: false)
executable('test_weather',
  ['test_weather.c'],
  c_args: test_cargs,
  dependencies: libgweather_dep,
  install: false)
