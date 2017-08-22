
version_conf = configuration_data()
version_conf.set('LIBGWEATHER_VERSION', meson.project_version())
configure_file(input: 'version.xml.in', output: 'version.xml', configuration: version_conf)

gnome.gtkdoc('libgweather',
             main_xml: 'libgweather-docs.xml',
             dependencies: libgweather_dep,
             gobject_typesfile: join_paths(meson.current_source_dir(), 'libgweather.types'),
             ignore_headers: ['gweather-enum-types.h',
                              'gweather-parser.h',
                              'gweather-private.h'],
             install: true,
             mkdb_args: ['--xml-mode', '--output-format=xml'],
             scan_args: ['--deprecated-guards="GWEATHER_DISABLE_DEPRECATED"'],
             src_dir: [join_paths(meson.source_root(), 'libgweather'),
                       join_paths(meson.build_root(), 'libgweather')])
