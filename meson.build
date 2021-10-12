project('libgweather', 'c',
  version: '40.0',
  license: 'GPL-2.0-or-later',
  meson_version: '>= 0.55.0',
  default_options: [
    'warning_level=2',
    'buildtype=debugoptimized',
    'c_std=gnu99',
  ],
)

# Add another 0, so that we can release without a suffix
libgweather_version = meson.project_version().split('.') + [0]

# We use libtool-version numbers because it's easier to understand.
# Before making a release, the libgweather_so_*
# numbers should be modified. The components are of the form C:R:A.
# a) If binary compatibility has been broken (eg removed or changed interfaces)
#    change to C+1:0:0.
# b) If interfaces have been changed or added, but binary compatibility has
#    been preserved, change to C+1:0:A+1
# c) If the interface is the same as the previous version, change to C:R+1:A
libgweather_lt_c=16
libgweather_lt_r=0
libgweather_lt_a=0

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

pkgdatadir = datadir / 'libgweather'
pkglibdir = libdir / 'libgweather'

glib_req_version = '>= 2.44.0'
gtk_req_version = '>= 3.13.5'
libsoup_req_version = '>= 2.44.0'
libxml_req_version = '>= 2.6.0'

GETTEXT_PACKAGE = 'libgweather-3.0'

pylint = find_program('pylint-3', 'pylint3', 'pylint', required: false)
pylint_flags = [
  '-d', 'C0111',
  '-d', 'W0511',
  '-d', 'F0401',
  '-d', 'C0326',
]

if get_option('glade_catalog') == 'false'
  enable_glade_catalog = false
else
  glade_dep = dependency('gladeui-2.0', required: get_option('glade_catalog') == 'true')
  enable_glade_catalog = glade_dep.found()
  if enable_glade_catalog
    glade_catalogdir = datadir / 'glade/catalogs'
  endif
endif

enable_vala = get_option('enable_vala')
if enable_vala == 'auto'
  enable_vala = find_program('vapigen', required: false).found()
else
  enable_vala = enable_vala != 'false'
endif

gen_locations_variant = find_program('build-aux/meson/gen_locations_variant.py')

subdir('data')
subdir('libgweather')
subdir('schemas')
subdir('doc')
subdir('po')
subdir('po-locations')

meson.add_install_script('build-aux/meson/meson_post_install.py')

pkgconfig.generate(
  filebase: 'gweather-3.0',
  name: 'GWeather',
  description: 'GWeather shared library',
  version: meson.project_version(),
  libraries: lib_libgweather,
  subdirs: 'libgweather-3.0',
  requires: [
    'gtk+-3.0',
  ],
  requires_private: [
    'gio-2.0',
    'libsoup-2.4',
    'libxml-2.0',
    'geocode-glib-1.0',
  ],
)

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
    'Glade catalog': enable_glade_catalog,
    'Generate VAPI': enable_vala,
    'API reference': get_option('gtk_doc'),
  },
  section: 'Build',
  bool_yn: true,
)
