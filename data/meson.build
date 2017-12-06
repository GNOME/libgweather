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

if enable_glade_catalog
  install_data('glade/libgweather.xml',
    install_dir: glade_catalogdir,
  )
endif

xmllint = find_program('xmllint', required: false)
if xmllint.found()
  test('Valid Locations file',
       xmllint,
       args: [
         '--path', '@0@'.format(meson.current_source_dir()),
         '--noout',
         '--valid',
         'Locations.xml',
       ])
endif

install_data('Locations.xml',
  install_dir: pkgdatadir,
)

install_data('locations.dtd',
  install_dir: pkgdatadir,
)
