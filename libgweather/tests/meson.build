gweather_test_cargs = [
  '-DSCHEMASDIR="@0@"'.format(schemas_srcdir),
  '-DSCHEMAS_BUILDDIR="@0@"'.format(schemas_builddir),
]

gweather_test_env = environment()
gweather_test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
gweather_test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
gweather_test_env.set('GIO_USE_VFS', 'local')
gweather_test_env.set('GSETTINGS_BACKED', 'memory')
gweather_test_env.set('G_ENABLE_DIAGNOSTIC', '0')
gweather_test_env.set('LIBGWEATHER_LOCATIONS_PATH', locations_bin)

gweather_tests = [
  { 'name': 'test_libgweather' },
  {
    'name': 'duplicates',
    'env': {
      'LIBGWEATHER_LOCATIONS_NO_NEAREST': '1',
    },
  },
  { 'name': 'timezones' },
  { 'name': 'metar' },
]

foreach t: gweather_tests
  test_name = t['name']
  test_sources = [test_name + '.c', 'gweather-test-utils.c'] + t.get('sources', [])
  test_c_args = gweather_test_cargs + t.get('c_args', [])
  test_env = gweather_test_env

  extra_env = t.get('env', {})
  foreach var, value: extra_env
    test_env.set(var, value)
  endforeach

  test(test_name,
    executable(test_name,
      sources: test_sources,
      c_args: test_c_args,
      dependencies: libgweather_static_dep,
      install: false,
    ),
    args: ['--tap', '-k'],
    protocol: 'tap',
    env: test_env,
    depends: [schemas_enums],
  )
endforeach
