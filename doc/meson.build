if build_docs
  dependency('gi-docgen', version: '>= 2021.6',
    fallback: ['gi-docgen', 'dummy_dep'],
  )

  gidocgen = find_program('gi-docgen', required: get_option('gtk_doc'))

  gweather_docdir = get_option('datadir') / 'doc'

  toml_conf = configuration_data()
  toml_conf.set('GWEATHER_VERSION', meson.project_version())

  gweather_toml = configure_file(
    input: 'libgweather.toml.in',
    output: 'libgweather.toml',
    configuration: toml_conf,
  )

  gweather_content_files = [
    'adding-locations.md',
    'migrating-3to4.md',
  ]

  custom_target('libgweather-doc',
    input: gweather_gir[0],
    output: libgweather_full_version,
    command: [
      gidocgen,
      'generate',
      '--quiet',
      '--fatal-warnings',
      '--config', gweather_toml,
      '--output-dir=@OUTPUT@',
      '--no-namespace-dir',
      '--content-dir=@0@'.format(meson.current_source_dir()),
      '@INPUT@',
    ],
    depend_files: [ gweather_toml, gweather_content_files ],
    build_by_default: true,
    install: true,
    install_dir: gweather_docdir,
  )
endif
