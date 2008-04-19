/* gweather-xml.c - Locations.xml parsing code
 *
 * Copyright (C) 2005 Ryan Lortie, 2004 Gareth Owen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


/* There is very little error checking in the parsing code below, it relies 
 * heavily on the locations file being in the correct format.  If you have
 * <name> elements within a parent element, they must come first and be
 * grouped together.
 * The format is as follows: 
 * 
 * <gweather format="1.0">
 * <region>
 *  <name>Name of the region</name>
 *  <name xml:lang="xx">Translated Name</name>
 *  <name xml:lang="zz">Another Translated Name</name>
 *  <country>
 *   <name>Name of the country</name>
 *   <iso-code>2-letter ISO 3166 code for the country</iso-code>
 *   <tz-hint>default timezone</tz-hint>
 *   <location>
 *    <name>Name of the location</name>
 *    <code>IWIN code</code>
 *    <zone>Forecast code (North America, Australia, UK only)</zone>
 *    <radar>Weather.com radar map code (North America only)</radar>
 *    <coordinates>Latitude and longitude as DD-MM[-SS][H] pair</coordinates>
 *   </location>
 *   <state>
 *     <location>
 *       ....
 *     </location>
 *     <city>
 *      <name>Name of city with multiple locations</city>
 *      <zone>Forecast code</zone>
 *      <radar>Radar Map code</radar>
 *      <location>
 *        ...
 *      </location>
 *     </city>
 *   </state>
 *  </country>
 * </region>
 * <gweather>
 *
 * The thing to note is that each country can either contain different locations
 * or be split into "states" which in turn contain a list of locations.
 *
 * <iso-code> can appear at the country or location level. <tz-hint>
 * can appear at country, state, or location.
 */

#include <string.h>
#include <math.h>
#include <locale.h>
#include <gtk/gtk.h>
#include <libxml/xmlreader.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include "gweather-xml.h"

static gint
gweather_xml_location_sort_func( GtkTreeModel *model, GtkTreeIter *a,
                                 GtkTreeIter *b, gpointer user_data )
{
    gint res;
    gchar *name_a, *name_b;
    gchar *fold_a, *fold_b;
        
    gtk_tree_model_get (model, a, GWEATHER_XML_COL_LOC, &name_a, -1);
    gtk_tree_model_get (model, b, GWEATHER_XML_COL_LOC, &name_b, -1);
        
    fold_a = g_utf8_casefold(name_a, -1);
    fold_b = g_utf8_casefold(name_b, -1);
        
    res = g_utf8_collate(fold_a, fold_b);
    
    g_free(name_a);
    g_free(name_b);
    g_free(fold_a);
    g_free(fold_b);
    
    return res;
}
 
static char*
gweather_xml_get_value( xmlTextReaderPtr xml )
{
  char* value;

  /* check for null node */
  if ( xmlTextReaderIsEmptyElement( xml ) )
    return NULL;
    
  /* the next "node" is the text node containing the value we want to get */
  if( xmlTextReaderRead( xml ) != 1 )
    return NULL;

  value = (char *) xmlTextReaderValue( xml );

  /* move on to the end of this node */
  while( xmlTextReaderNodeType( xml ) != XML_READER_TYPE_END_ELEMENT )
    if( xmlTextReaderRead( xml ) != 1 )
    {
      xmlFree( value );
      return NULL;
    }

  /* consume the end element too */
  if( xmlTextReaderRead( xml ) != 1 )
  {
    xmlFree( value );
    return NULL;
  }
    
  return value;
}

static char *
gweather_xml_parse_name( xmlTextReaderPtr xml )
{
  const char * const *locales;
  const char *this_language;
  int best_match = INT_MAX;
  char *lang, *tagname;
  gboolean keep_going;
  char *name = NULL;
  int i;

  locales = g_get_language_names();

  do
  {
    /* First let's get the language */
    lang = (char *) xmlTextReaderXmlLang( xml );

    if( lang == NULL )
      this_language = "C";
    else
      this_language = lang;

    /* the next "node" is text node containing the actual name */
    if( xmlTextReaderRead( xml ) != 1 )
    {
      xmlFree( lang );
      return NULL;
    }

    for( i = 0; locales[i] && i < best_match; i++ )
      if( !strcmp( locales[i], this_language ) )
      {
        /* if we've already encounted a less accurate
           translation, then free it */
        xmlFree( name );

        name = (char *) xmlTextReaderValue( xml );
        best_match = i;

        break;
      }

    xmlFree( lang );

    while( xmlTextReaderNodeType( xml ) != XML_READER_TYPE_ELEMENT )
      if( xmlTextReaderRead( xml ) != 1 )
      {
        xmlFree( name );
        return NULL;
      }

    /* if the next tag is another <name> then keep going */
    tagname = (char *) xmlTextReaderName( xml );
    keep_going = !strcmp( tagname, "name" );
    xmlFree( tagname );

  } while( keep_going );

  return name;
}

static gboolean
gweather_xml_parse_node (xmlTextReaderPtr xml,
			 GtkTreeStore *store, GtkTreeIter *parent,
                         const char *dflt_radar, const char *dflt_zone,
                         const char *dflt_country_code,
			 const char *dflt_tz_hint, const char *cityname)
{
  char *name, *code, *zone, *radar, *coordinates;
  char *country_code, *tz_hint;
  char **city, *nocity = NULL;
  GtkTreeIter iter, *self;
  gboolean is_location;
  char *tagname;
  gboolean ret = FALSE;
  int tagtype;

  if( (tagname = (char *) xmlTextReaderName( xml )) == NULL )
    return FALSE;

  if( !strcmp( tagname, "city" ) )
    city = &name;
  else
    city = &nocity;

  is_location = !strcmp( tagname, "location" );

  /* if we're processing the top-level, then don't create a new iter */
  if( !strcmp( tagname, "gweather" ) )
    self = NULL;
  else
  {
    self = &iter;
    /* insert this node into the tree */
    gtk_tree_store_append( store, self, parent );
  }

  xmlFree( tagname );

  coordinates = NULL;
  radar = NULL;
  zone = NULL;
  code = NULL;
  name = NULL;

  country_code = dflt_country_code ? (char *) xmlStrdup( (xmlChar *) dflt_country_code ) : NULL;
  tz_hint = dflt_tz_hint ? (char *) xmlStrdup( (xmlChar *) dflt_tz_hint ) : NULL;

  /* absorb the start tag */
  if( xmlTextReaderRead( xml ) != 1 )
    goto error_out;

  /* start parsing the actual contents of the node */
  while( (tagtype = xmlTextReaderNodeType( xml )) !=
         XML_READER_TYPE_END_ELEMENT )
  {

    /* skip non-element types */
    if( tagtype != XML_READER_TYPE_ELEMENT )
    {
      if( xmlTextReaderRead( xml ) != 1 )
        goto error_out;

      continue;
    }
    
    tagname = (char *) xmlTextReaderName( xml );

    if( !strcmp( tagname, "region" ) || !strcmp( tagname, "country" ) ||
        !strcmp( tagname, "state" ) || !strcmp( tagname, "city" ) ||
        !strcmp( tagname, "location" ) )
    {
      /* recursively handle sub-sections */
      if( !gweather_xml_parse_node( xml, store, self, 
				    radar, zone, country_code, tz_hint,
				    *city ) )
        goto error_out;
    }
    else if ( !strcmp( tagname, "name" ) )
    {
      xmlFree( name );
      if( (name = gweather_xml_parse_name( xml )) == NULL )
        goto error_out;
    }
    else if ( !strcmp( tagname, "iso-code" ) )
    {
      xmlFree( country_code );
      if( (country_code = gweather_xml_get_value( xml )) == NULL )
        goto error_out;
    }
    else if ( !strcmp( tagname, "tz-hint" ) )
    {
      xmlFree( tz_hint );
      if( (tz_hint = gweather_xml_get_value( xml )) == NULL )
        goto error_out;
    }
    else if ( !strcmp( tagname, "code" ) )
    {
      xmlFree( code );
      if( (code = gweather_xml_get_value( xml )) == NULL )
        goto error_out;
    }
    else if ( !strcmp( tagname, "zone" ) )
    {
      xmlFree( zone );
      if( (zone = gweather_xml_get_value( xml )) == NULL )
        goto error_out;
    }
    else if ( !strcmp( tagname, "radar" ) )
    {
      xmlFree( radar );
      if( (radar = gweather_xml_get_value( xml )) == NULL )
        goto error_out;
    }
    else if ( !strcmp( tagname, "coordinates" ) )
    {
      xmlFree( coordinates );
      if( (coordinates = gweather_xml_get_value( xml )) == NULL )
        goto error_out;
    }
    else /* some strange tag */
    {
      /* skip past it */
      char *junk;
      junk = gweather_xml_get_value( xml );
      if( junk )
	xmlFree( junk );
    }

    xmlFree( tagname );
  }

  if( self )
    gtk_tree_store_set( store, self, GWEATHER_XML_COL_LOC, name, -1 );

  /* if this is an actual location, setup the WeatherLocation for it */
  if( is_location )
  {
    WeatherLocation *new_loc;

    if( cityname == NULL )
      cityname = name;

    if( radar != NULL )
      dflt_radar = radar;

    if( zone != NULL )
      dflt_zone = zone;

    new_loc =  weather_location_new( cityname, code, dflt_zone,
                                     dflt_radar, coordinates,
				     country_code, tz_hint );

    gtk_tree_store_set( store, &iter, GWEATHER_XML_COL_POINTER, new_loc, -1 );
  }
  /* if this is not a location and there's no child, then it's useless */
  else if ( !gtk_tree_model_iter_has_child( GTK_TREE_MODEL (store), &iter ) )
  {
    gtk_tree_store_remove( store, &iter );
  }

  /* if this is a city with only one location, then we merge the location and
   * the city */
  if (*city)
  {
    int n_children;

    n_children = gtk_tree_model_iter_n_children( GTK_TREE_MODEL (store),
                                                 &iter );
    if ( n_children == 1 )
    {
      GtkTreeIter child;
      WeatherLocation *loc;

      gtk_tree_model_iter_children( GTK_TREE_MODEL (store), &child, &iter );
      gtk_tree_model_get( GTK_TREE_MODEL (store), &child,
                          GWEATHER_XML_COL_POINTER, &loc, -1 );
      gtk_tree_store_remove( store, &child );
      gtk_tree_store_set( store, &iter, GWEATHER_XML_COL_POINTER, loc, -1 );
    }
  }

  /* absorb the end tag.  in the case of processing a <gweather> then 'self'
     is NULL.  In this case, we let this fail since we might be at EOF */
  if( xmlTextReaderRead( xml ) == 1 || !self )
    ret = TRUE;

error_out:
  xmlFree( name );
  xmlFree( code );
  xmlFree( zone );
  xmlFree( radar );
  xmlFree( coordinates );
  xmlFree( country_code );
  xmlFree( tz_hint );

  return ret;
}

GtkTreeModel *
gweather_xml_load_locations( void )
{
  const char * const *languages;
  int i;
  char *filename;
  char *tagname, *format;
  GtkTreeSortable *sortable;
  GtkTreeStore *store = NULL;
  xmlTextReaderPtr xml;
  int keep_going;

  /* First try to load a locale-specific XML. It's much faster. */
  languages = g_get_language_names ();
  filename = NULL;

  for (i = 0; languages[i] != NULL; i++)
    {
      filename = g_strdup_printf ("%s/Locations.%s.xml",
                                  GWEATHER_XML_LOCATION_DIR, languages[i]);

      if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
        break;

      g_free (filename);
      filename = NULL;
    }

  /* Fallback on the file containing either all translations, or only
   * the english names (depending on the configure flags). Note that it's
   * also the file that is advertised in our pkg-config file, so it's
   * part of the API. */
  if (!filename)
    filename = g_strdup (GWEATHER_XML_LOCATION_DIR"/Locations.xml");

  /* Open the xml file containing the different locations */
  xml = xmlNewTextReaderFilename (filename);
  g_free (filename);

  if( xml == NULL )
    return NULL;

  /* fast forward to the first element */
  do
  {
    /* if we encounter a problem here, exit right away */
    if( xmlTextReaderRead( xml ) != 1 )
      goto error_out;
  } while( xmlTextReaderNodeType( xml ) != XML_READER_TYPE_ELEMENT );

  /* check the name and format */
  tagname = (char *) xmlTextReaderName( xml );
  keep_going = tagname && !strcmp( tagname, "gweather" );
  xmlFree( tagname );

  if( !keep_going )
    goto error_out;

  format = (char *) xmlTextReaderGetAttribute( xml, (xmlChar *) "format" );
  keep_going = format && !strcmp( format, "1.0" );
  xmlFree( format );

  if( !keep_going )
    goto error_out;

  store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);

  if (!gweather_xml_parse_node( xml, store, NULL, NULL, NULL, NULL, NULL, NULL ))
  {
    g_object_unref( store );
    store = NULL;
    goto error_out;
  }

  /* Sort the tree */
  sortable = GTK_TREE_SORTABLE( store );
  gtk_tree_sortable_set_default_sort_func( sortable,
                                           &gweather_xml_location_sort_func,
                                           NULL, NULL);
  gtk_tree_sortable_set_sort_column_id( sortable, 
					GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING );
error_out:
  xmlFreeTextReader( xml );

  return (GtkTreeModel *)store;
}
