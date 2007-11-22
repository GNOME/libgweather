/* $Id: test_metar.c 9270 2006-01-15 10:15:19Z davyd $ */

/*
 * Simple program to reproduce METAR parsing results from command line
 */

#include <glib.h>
#include <string.h>
#include <stdio.h>
#include "weather-priv.h"

#ifndef BUFLEN
#define BUFLEN 4096
#endif /* BUFLEN */

int
main (int argc, char **argv)
{
  FILE*  stream = stdin;
  gchar* filename = NULL;
  GOptionEntry entries[] =
  {
    { "file", 'f', 0, G_OPTION_ARG_FILENAME, &filename,
      "file constaining metar observations", NULL },
    { NULL }
  };
  GOptionContext* context;
  GError* error = NULL;
  char buf[BUFLEN];
  int len;
  WeatherInfo info;


  context = g_option_context_new ("- test libgweather metar parser");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_parse (context, &argc, &argv, &error);

  if (error) {
    perror(error->message);
    return error->code;
  }
  if (filename) {
    stream = fopen(filename, "r");
    if (!stream) {
      perror("fopen");
      return -1;
    }
  }
  else
  {
    fprintf (stderr, "Enter a METAR string...\n");
  }

  while (fgets(buf, sizeof(buf), stream)) {
      len = strlen(buf);
      if (buf[len - 1] == '\n') {
	  buf[--len] = '\0';
      }
      printf("\n%s\n", buf);

      memset (&info, 0, sizeof(info));
      info.valid = 1;
      metar_parse(buf, &info);
      printf("Returned info:\n");
      printf("  update:   %s", ctime(&info.update));
      printf("  sky:      %s\n", weather_info_get_sky(&info));
      printf("  cond:     %s\n", weather_info_get_conditions(&info));
      printf("  temp:     %g F\n", info.temp);
      printf("  dewp:     %g F\n", info.dew);
      printf("  winddir:  %s\n", weather_wind_direction_string(info.wind));
      printf("  windsp:   %d knots\n", info.windspeed);
      printf("  pressure: %g\" Hg\n", info.pressure);
      printf("  vis:      %g miles\n", info.visibility);

      // TODO: retrieve location's lat/lon to display sunrise/set times
  }
  return 0;
}
