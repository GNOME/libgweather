project('libgweather', 'c',
  version: '3.36.1',
  meson_version: '>= 0.50.0',
)

libgweather_version = meson.project_version().split('.')

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

# convert to soname
libgweather_so_version = '@0@.@1@.@2@'.format((libgweather_lt_c - libgweather_lt_a),
                                            libgweather_lt_a, libgweather_lt_r)

current = libgweather_lt_c - libgweather_lt_a
interface_age = libgweather_lt_r
libgweather_darwin_versions = [current + 1, '@0@.@1@'.format(current + 1, interface_age)]

pkgconfig = import('pkgconfig')
gnome = import('gnome')
i18n = import('i18n')

prefix = get_option('prefix')

bindir = join_paths(prefix, get_option('bindir'))
datadir = join_paths(prefix, get_option('datadir'))
libdir = join_paths(prefix, get_option('libdir'))
includedir = join_paths(prefix, get_option('includedir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))
sysconfdir = join_paths(prefix, get_option('sysconfdir'))
pkgdatadir = join_paths(datadir, 'libgweather')

glib_req_version = '>= 2.44.0'
gtk_req_version = '>= 3.13.5'
libsoup_req_version = '>= 2.44.0'
libxml_req_version = '>= 2.6.0'

c_compiler = meson.get_compiler('c')
gtk_dep = dependency('gtk+-3.0', version: gtk_req_version)
glib_dep = dependency('gio-2.0', version: glib_req_version)
libsoup_dep = dependency('libsoup-2.4', version: libsoup_req_version)
libxml_dep = dependency('libxml-2.0', version: libxml_req_version)
geocode_glib_dep = dependency('geocode-glib-1.0')
math_dep = c_compiler.find_library('m', required : false)
deps_libgweather = [math_dep, gtk_dep, glib_dep, libsoup_dep, libxml_dep, geocode_glib_dep]

config_h = configuration_data()
GETTEXT_PACKAGE = 'libgweather-3.0'
config_h.set_quoted('GETTEXT_PACKAGE', GETTEXT_PACKAGE)
config_h.set_quoted('LOCALEDIR', join_paths(datadir, 'locale'))
config_h.set_quoted('GNOMELOCALEDIR', join_paths(datadir, 'locale'))
config_h.set_quoted('G_LOG_DOMAIN', 'GWeather')
config_h.set_quoted('GWEATHER_XML_LOCATION_DIR', pkgdatadir)
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

if get_option('default_library') != 'static'
  if host_machine.system() == 'windows'
    config_h.set('DLL_EXPORT', true)
    config_h.set('GWEATHER_EXTERN', '__declspec(dllexport) extern')
    if cc.get_id() != 'msvc'
      add_project_arguments(['-fvisibility=hidden'], language: 'c')
    endif
  else
    config_h.set('GWEATHER_EXTERN', '__attribute__((visibility("default"))) extern')
    add_project_arguments(['-fvisibility=hidden'], language: 'c')
  endif
endif


configure_file(
  output: 'config.h',
  configuration: config_h,
)

if get_option('glade_catalog') == 'false'
  enable_glade_catalog = false
else
  glade_dep = dependency('gladeui-2.0', required: (get_option('glade_catalog') == 'true'))
  enable_glade_catalog = glade_dep.found()
  if enable_glade_catalog
    glade_catalogdir = join_paths(datadir, 'glade', 'catalogs')
  endif
endif

enable_vala = get_option('enable_vala')
if enable_vala == 'auto'
  enable_vala = find_program('vapigen', required: false).found()
else
  enable_vala = enable_vala != 'false'
endif

root_inc = include_directories('.')

subdir('libgweather')
subdir('data')
subdir('schemas')
if get_option('gtk_doc')
  subdir('doc')
endif
subdir('po')
subdir('po-locations')
meson.add_install_script('meson/meson_post_install.py')
