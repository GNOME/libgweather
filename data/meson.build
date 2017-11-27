pkgconfig.generate(
  filebase: 'gweather-3.0',
  name: 'GWeather',
  description: 'GWeather shared library',
  version: meson.project_version(),
  libraries: lib_libgweather,
  subdirs: 'libgweather-3.0',
)

if enable_glade_catalog
  install_data('glade/libgweather.xml',
    install_dir: glade_catalogdir,
  )
endif

# FIXME check syntax of Locations.xml
install_data('Locations.xml',
  install_dir: pkgdatadir,
)
install_data('locations.dtd',
  install_dir: pkgdatadir,
)
