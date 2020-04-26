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
