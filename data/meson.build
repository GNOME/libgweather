xmllint = find_program('xmllint', required: false)
if xmllint.found()
  test('Valid Locations file',
    xmllint,
    args: [
      '--path', '@0@'.format(meson.current_source_dir()),
      '--noout',
      '--valid',
      'Locations.xml',
    ],
    suite: ['lint'],
  )
endif

if pylint.found()
  test('pylint-gen-locations-variant',
    pylint,
    args: [
      pylint_flags,
      gen_locations_variant.full_path(),
    ],
    env: nomalloc,
    suite: ['lint'],
  )
endif

# The generated file is architecture dependent (i.e. host endianness)
locations_bin = custom_target('locations-db',
  command: [gen_locations_variant, '@INPUT@', '@OUTPUT@' ],
  input: files('Locations.xml'),
  output: '@BASENAME@.bin',
  install: true,
  install_dir: pkglibdir,
)

install_data('Locations.xml',
  install_dir: pkgdatadir,
)

install_data('locations.dtd',
  install_dir: pkgdatadir,
)
