tools = [
  {
    'name': 'metar',
  },
  {
    'name': 'sun_moon',
    'sources': [
      'test_sun_moon.c',
      '../weather-sun.c',
      '../weather-moon.c',
    ],
  },
  {
    'name': 'weather',
  }
]

tools_cargs = [
  '-DTEST_LOCATIONS="@0@"'.format(locations_bin),
  '-DSCHEMASDIR="@0@"'.format(schemas_srcdir),
  '-DSCHEMAS_BUILDDIR="@0@"'.format(schemas_builddir),
]

foreach tool: tools
  tool_name = tool.get('name')
  tool_sources = tool.get('sources', ['test_' + tool_name + '.c'])

  executable('test_' + tool_name,
    sources: tool_sources,
    c_args: tools_cargs,
    dependencies: libgweather_static_dep,
    install: false,
  )
endforeach
