gsettingsdir = join_paths(datadir, 'glib-2.0', 'schemas')
install_data('org.gnome.GWeather.gschema.xml',
             install_dir : gsettingsdir)

gnome.mkenums('org.gnome.GWeather.enums.xml',
              comments: '<!-- @comment@ -->',
              fhead: '<schemalist>',
              vhead: '<@type@ id="org.gnome.GWeather.@EnumName@">',
              vprod: '<value nick="@valuenick@" value="@valuenum@"/>',
              vtail: '</@type@>',
              ftail: '</schemalist>',
              sources: ['../libgweather/gweather-enums.h'],
              install_header: true,
              install_dir: gsettingsdir)
