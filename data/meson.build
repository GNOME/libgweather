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

gen_locations_variant = find_program('gen_locations_variant.py')

if pylint.found()
  test('pylint-gen-locations-variant', pylint, args: pylint_flags + files([ 'gen_locations_variant.py' ]))
endif

# The generated file is architecture dependent (i.e. host endianness)
locations_bin = custom_target('locations-db',
  command: [gen_locations_variant, '@INPUT@', '@OUTPUT@' ],
  install: true,
  install_dir: pkglibdir,
  input: files('Locations.xml'),
  output: '@BASENAME@.bin',
)

install_data('Locations.xml',
  install_dir: pkgdatadir,
)

install_data('locations.dtd',
  install_dir: pkgdatadir,
)
