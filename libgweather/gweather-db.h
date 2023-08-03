#ifndef __DB___GWEATHER_GV__H__
#define __DB___GWEATHER_GV__H__
/* generated code for gweather.gv */
#include <string.h>
#include <glib.h>

/********** Basic types *****************/

typedef struct {
 gconstpointer base;
 gsize size;
} DbRef;

/* Make sure generated code works with older glib versions without g_memdup2 */
static inline gpointer
_Db_memdup2(gconstpointer mem, gsize byte_size)
{
#if GLIB_CHECK_VERSION(2, 68, 0)
    return g_memdup2(mem, byte_size);
#else
    gpointer new_mem;

    if (mem && byte_size != 0) {
        new_mem = g_malloc(byte_size);
        memcpy(new_mem, mem, byte_size);
    } else {
        new_mem = NULL;
    }

    return new_mem;
#endif
}

#define DB_REF_READ_FRAME_OFFSET(_v, _index) Db_ref_read_unaligned_le ((guchar*)((_v).base) + (_v).size - (offset_size * ((_index) + 1)), offset_size)
#define DB_REF_ALIGN(_offset, _align_to) ((_offset + _align_to - 1) & ~(gsize)(_align_to - 1))

/* Note: clz is undefinded for 0, so never call this size == 0 */
G_GNUC_CONST static inline guint
Db_ref_get_offset_size (gsize size)
{
#if defined(__GNUC__) && (__GNUC__ >= 4) && defined(__OPTIMIZE__) && defined(__LP64__)
  /* Instead of using a lookup table we use nibbles in a lookup word */
  guint32 v = (guint32)0x88884421;
  return (v >> (((__builtin_clzl(size) ^ 63) / 8) * 4)) & 0xf;
#else
  if (size > G_MAXUINT16)
    {
      if (size > G_MAXUINT32)
        return 8;
      else
        return 4;
    }
  else
    {
      if (size > G_MAXUINT8)
         return 2;
      else
         return 1;
    }
#endif
}

G_GNUC_PURE static inline guint64
Db_ref_read_unaligned_le (guchar *bytes, guint   size)
{
  union
  {
    guchar bytes[8];
    guint64 integer;
  } tmpvalue;

  tmpvalue.integer = 0;
  /* we unroll the size checks here so that memcpy gets constant args */
  if (size >= 4)
    {
      if (size == 8)
        memcpy (&tmpvalue.bytes, bytes, 8);
      else
        memcpy (&tmpvalue.bytes, bytes, 4);
    }
  else
    {
      if (size == 2)
        memcpy (&tmpvalue.bytes, bytes, 2);
      else
        memcpy (&tmpvalue.bytes, bytes, 1);
    }

  return GUINT64_FROM_LE (tmpvalue.integer);
}

static inline void
__Db_gstring_append_double (GString *string, double d)
{
  gchar buffer[100];
  gint i;

  g_ascii_dtostr (buffer, sizeof buffer, d);
  for (i = 0; buffer[i]; i++)
    if (buffer[i] == '.' || buffer[i] == 'e' ||
        buffer[i] == 'n' || buffer[i] == 'N')
      break;

  /* if there is no '.' or 'e' in the float then add one */
  if (buffer[i] == '\0')
    {
      buffer[i++] = '.';
      buffer[i++] = '0';
      buffer[i++] = '\0';
    }
   g_string_append (string, buffer);
}

static inline void
__Db_gstring_append_string (GString *string, const char *str)
{
  gunichar quote = strchr (str, '\'') ? '"' : '\'';

  g_string_append_c (string, quote);
  while (*str)
    {
      gunichar c = g_utf8_get_char (str);

      if (c == quote || c == '\\')
        g_string_append_c (string, '\\');

      if (g_unichar_isprint (c))
        g_string_append_unichar (string, c);
      else
        {
          g_string_append_c (string, '\\');
          if (c < 0x10000)
            switch (c)
              {
              case '\a':
                g_string_append_c (string, 'a');
                break;

              case '\b':
                g_string_append_c (string, 'b');
                break;

              case '\f':
                g_string_append_c (string, 'f');
                break;

              case '\n':
                g_string_append_c (string, 'n');
                break;

              case '\r':
                g_string_append_c (string, 'r');
                break;

              case '\t':
                g_string_append_c (string, 't');
                break;

              case '\v':
                g_string_append_c (string, 'v');
                break;

              default:
                g_string_append_printf (string, "u%04x", c);
                break;
              }
           else
             g_string_append_printf (string, "U%08x", c);
        }

      str = g_utf8_next_char (str);
    }

  g_string_append_c (string, quote);
}

/************** DbVariantRef *******************/

typedef struct {
 gconstpointer base;
 gsize size;
} DbVariantRef;

static inline DbRef
Db_variant_get_child (DbVariantRef v, const GVariantType **out_type)
{
  if (v.size)
    {
      guchar *base = (guchar *)v.base;
      gsize size = v.size - 1;

      /* find '\0' character */
      while (size > 0 && base[size] != 0)
        size--;

      /* ensure we didn't just hit the start of the string */
      if (base[size] == 0)
       {
          const char *type_string = (char *) base + size + 1;
          const char *limit = (char *)base + v.size;
          const char *end;

          if (g_variant_type_string_scan (type_string, limit, &end) && end == limit)
            {
              if (out_type)
                *out_type = (const GVariantType *)type_string;
              return (DbRef) { v.base, size };
            }
       }
    }
  if (out_type)
    *out_type = G_VARIANT_TYPE_UNIT;
  return  (DbRef) { "\0", 1 };
}

static inline const GVariantType *
Db_variant_get_type (DbVariantRef v)
{
  if (v.size)
    {
      guchar *base = (guchar *)v.base;
      gsize size = v.size - 1;

      /* find '\0' character */
      while (size > 0 && base[size] != 0)
        size--;

      /* ensure we didn't just hit the start of the string */
      if (base[size] == 0)
       {
          const char *type_string = (char *) base + size + 1;
          const char *limit = (char *)base + v.size;
          const char *end;

          if (g_variant_type_string_scan (type_string, limit, &end) && end == limit)
             return (const GVariantType *)type_string;
       }
    }
  return  G_VARIANT_TYPE_UNIT;
}

static inline gboolean
Db_variant_is_type (DbVariantRef v, const GVariantType *type)
{
   return g_variant_type_equal (Db_variant_get_type (v), type);
}

static inline DbVariantRef
Db_variant_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), G_VARIANT_TYPE_VARIANT));
  return (DbVariantRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbVariantRef
Db_variant_from_bytes (GBytes *b)
{
  return (DbVariantRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbVariantRef
Db_variant_from_data (gconstpointer data, gsize size)
{
  return (DbVariantRef) { data, size };
}

static inline GVariant *
Db_variant_dup_to_gvariant (DbVariantRef v)
{
  guint8 *duped = _Db_memdup2 (v.base, v.size);
  return g_variant_new_from_data (G_VARIANT_TYPE_VARIANT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
Db_variant_to_gvariant (DbVariantRef v,
                              GDestroyNotify      notify,
                              gpointer            user_data)
{
  return g_variant_new_from_data (G_VARIANT_TYPE_VARIANT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
Db_variant_to_owned_gvariant (DbVariantRef v,
                                     GVariant *base)
{
  return Db_variant_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
Db_variant_peek_as_variant (DbVariantRef v)
{
  return g_variant_new_from_data (G_VARIANT_TYPE_VARIANT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbVariantRef
Db_variant_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, G_VARIANT_TYPE_VARIANT));
  return Db_variant_from_data (child.base, child.size);
}

static inline GVariant *
Db_variant_dup_child_to_gvariant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  guint8 *duped = _Db_memdup2 (child.base, child.size);
  return g_variant_new_from_data (type, duped, child.size, TRUE, g_free, duped);
}

static inline GVariant *
Db_variant_peek_child_as_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  return g_variant_new_from_data (type, child.base, child.size, TRUE, NULL, NULL);
}

static inline GString *
Db_variant_format (DbVariantRef v, GString *s, gboolean type_annotate)
{
#ifdef DB_DEEP_VARIANT_FORMAT
  GVariant *gv = Db_variant_peek_as_variant (v);
  return g_variant_print_string (gv, s, TRUE);
#else
  const GVariantType  *type = Db_variant_get_type (v);
  g_string_append_printf (s, "<@%.*s>", (int)g_variant_type_get_string_length (type), (const char *)type);
  return s;
#endif
}

static inline char *
Db_variant_print (DbVariantRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  Db_variant_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}
static inline gboolean
Db_variant_get_boolean (DbVariantRef v)
{
  return (gboolean)*((guint8 *)v.base);
}
static inline guint8
Db_variant_get_byte (DbVariantRef v)
{
  return (guint8)*((guint8 *)v.base);
}
static inline gint16
Db_variant_get_int16 (DbVariantRef v)
{
  return (gint16)*((gint16 *)v.base);
}
static inline guint16
Db_variant_get_uint16 (DbVariantRef v)
{
  return (guint16)*((guint16 *)v.base);
}
static inline gint32
Db_variant_get_int32 (DbVariantRef v)
{
  return (gint32)*((gint32 *)v.base);
}
static inline guint32
Db_variant_get_uint32 (DbVariantRef v)
{
  return (guint32)*((guint32 *)v.base);
}
static inline gint64
Db_variant_get_int64 (DbVariantRef v)
{
  return (gint64)*((gint64 *)v.base);
}
static inline guint64
Db_variant_get_uint64 (DbVariantRef v)
{
  return (guint64)*((guint64 *)v.base);
}
static inline guint32
Db_variant_get_handle (DbVariantRef v)
{
  return (guint32)*((guint32 *)v.base);
}
static inline double
Db_variant_get_double (DbVariantRef v)
{
  return (double)*((double *)v.base);
}
static inline const char *
Db_variant_get_string (DbVariantRef v)
{
  return (const char *)v.base;
}
static inline const char *
Db_variant_get_objectpath (DbVariantRef v)
{
  return (const char *)v.base;
}
static inline const char *
Db_variant_get_signature (DbVariantRef v)
{
  return (const char *)v.base;
}

/************** DbI18n *******************/
#define DB_I18N_TYPESTRING "(ss)"
#define DB_I18N_TYPEFORMAT ((const GVariantType *) DB_I18N_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbI18nRef;


static inline DbI18nRef
db_i18n_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_I18N_TYPESTRING));
  return (DbI18nRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbI18nRef
db_i18n_from_bytes (GBytes *b)
{
  return (DbI18nRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbI18nRef
db_i18n_from_data (gconstpointer data, gsize size)
{
  return (DbI18nRef) { data, size };
}

static inline GVariant *
db_i18n_dup_to_gvariant (DbI18nRef v)
{
  guint8 *duped = _Db_memdup2 (v.base, v.size);
  return g_variant_new_from_data (DB_I18N_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
db_i18n_to_gvariant (DbI18nRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_I18N_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_i18n_to_owned_gvariant (DbI18nRef v, GVariant *base)
{
  return db_i18n_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_i18n_peek_as_gvariant (DbI18nRef v)
{
  return g_variant_new_from_data (DB_I18N_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbI18nRef
db_i18n_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_I18N_TYPESTRING));
  return db_i18n_from_data (child.base, child.size);
}

#define DB_I18N_INDEXOF_STR 0

static inline const char *
db_i18n_get_str (DbI18nRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  guint offset = ((0) & (~(gsize)0)) + 0;
  G_GNUC_UNUSED const char *base = (const char *)v.base;
  gsize start = offset;
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  g_assert (start <= end);
  g_assert (end <= v.size);
  g_assert (base[end-1] == 0);
  return &G_STRUCT_MEMBER(const char, v.base, start);
}

#define DB_I18N_INDEXOF_MSGCTXT 1

static inline const char *
db_i18n_get_msgctxt (DbI18nRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  guint offset = ((last_end + 0) & (~(gsize)0)) + 0;
  G_GNUC_UNUSED const char *base = (const char *)v.base;
  gsize start = offset;
  G_GNUC_UNUSED gsize end = v.size - offset_size * 1;
  g_assert (start <= end);
  g_assert (end <= v.size);
  g_assert (base[end-1] == 0);
  return &G_STRUCT_MEMBER(const char, v.base, start);
}

static inline GString *
db_i18n_format (DbI18nRef v, GString *s, gboolean type_annotate)
{
  g_string_append (s, "(");
  __Db_gstring_append_string (s, db_i18n_get_str (v));
  g_string_append (s, ", ");
  __Db_gstring_append_string (s, db_i18n_get_msgctxt (v));
  g_string_append (s, ")");
  return s;
}

static inline char *
db_i18n_print (DbI18nRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_i18n_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** DbArrayofstring *******************/
#define DB_ARRAYOFSTRING_TYPESTRING "as"
#define DB_ARRAYOFSTRING_TYPEFORMAT ((const GVariantType *) DB_ARRAYOFSTRING_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbArrayofstringRef;


static inline DbArrayofstringRef
db_arrayofstring_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_ARRAYOFSTRING_TYPESTRING));
  return (DbArrayofstringRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbArrayofstringRef
db_arrayofstring_from_bytes (GBytes *b)
{
  return (DbArrayofstringRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbArrayofstringRef
db_arrayofstring_from_data (gconstpointer data, gsize size)
{
  return (DbArrayofstringRef) { data, size };
}

static inline GVariant *
db_arrayofstring_dup_to_gvariant (DbArrayofstringRef v)
{
  guint8 *duped = _Db_memdup2 (v.base, v.size);
  return g_variant_new_from_data (DB_ARRAYOFSTRING_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
db_arrayofstring_to_gvariant (DbArrayofstringRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_ARRAYOFSTRING_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_arrayofstring_to_owned_gvariant (DbArrayofstringRef v, GVariant *base)
{
  return db_arrayofstring_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_arrayofstring_peek_as_gvariant (DbArrayofstringRef v)
{
  return g_variant_new_from_data (DB_ARRAYOFSTRING_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbArrayofstringRef
db_arrayofstring_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_ARRAYOFSTRING_TYPESTRING));
  return db_arrayofstring_from_data (child.base, child.size);
}

static inline gsize
db_arrayofstring_get_length (DbArrayofstringRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize offsets_array_size;
  if (last_end > v.size)
    return 0;
  offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return 0;
  gsize length  = offsets_array_size / offset_size;
  return length;
}

static inline const char *
db_arrayofstring_get_at (DbArrayofstringRef v, gsize index)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize len = (v.size - last_end) / offset_size;
  gsize start = (index > 0) ? DB_REF_ALIGN(DB_REF_READ_FRAME_OFFSET(v, len - index), 1) : 0;
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, len - index - 1);
  g_assert (start <= end);
  g_assert (end <= last_end);
  const char *base = (const char *)v.base;
  g_assert (base[end-1] == 0);
  return base + start;
}

static inline const char **
db_arrayofstring_to_strv (DbArrayofstringRef v, gsize *length_out)
{
  gsize length = db_arrayofstring_get_length (v);
  gsize i;
  const char **resv = g_new (const char *, length + 1);

  for (i = 0; i < length; i++)
    resv[i] = db_arrayofstring_get_at (v, i);
  resv[i] = NULL;

  if (length_out)
    *length_out = length;

  return resv;
}

static inline GString *
db_arrayofstring_format (DbArrayofstringRef v, GString *s, gboolean type_annotate)
{
  gsize len = db_arrayofstring_get_length (v);
  gsize i;
  if (len == 0 && type_annotate)
    g_string_append_printf (s, "@%s ", DB_ARRAYOFSTRING_TYPESTRING);
  g_string_append_c (s, '[');
  for (i = 0; i < len; i++)
    {
      if (i != 0)
        g_string_append (s, ", ");
      __Db_gstring_append_string (s, db_arrayofstring_get_at (v, i));
    }
  g_string_append_c (s, ']');
  return s;
}

static inline char *
db_arrayofstring_print (DbArrayofstringRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_arrayofstring_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** DbTimezone *******************/
#define DB_TIMEZONE_TYPESTRING "((ss)as)"
#define DB_TIMEZONE_TYPEFORMAT ((const GVariantType *) DB_TIMEZONE_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbTimezoneRef;


static inline DbTimezoneRef
db_timezone_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_TIMEZONE_TYPESTRING));
  return (DbTimezoneRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbTimezoneRef
db_timezone_from_bytes (GBytes *b)
{
  return (DbTimezoneRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbTimezoneRef
db_timezone_from_data (gconstpointer data, gsize size)
{
  return (DbTimezoneRef) { data, size };
}

static inline GVariant *
db_timezone_dup_to_gvariant (DbTimezoneRef v)
{
  guint8 *duped = _Db_memdup2 (v.base, v.size);
  return g_variant_new_from_data (DB_TIMEZONE_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
db_timezone_to_gvariant (DbTimezoneRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_TIMEZONE_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_timezone_to_owned_gvariant (DbTimezoneRef v, GVariant *base)
{
  return db_timezone_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_timezone_peek_as_gvariant (DbTimezoneRef v)
{
  return g_variant_new_from_data (DB_TIMEZONE_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbTimezoneRef
db_timezone_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_TIMEZONE_TYPESTRING));
  return db_timezone_from_data (child.base, child.size);
}

#define DB_TIMEZONE_INDEXOF_NAME 0

static inline DbI18nRef
db_timezone_get_name (DbTimezoneRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  guint offset = ((0) & (~(gsize)0)) + 0;
  gsize start = offset;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbI18nRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

#define DB_TIMEZONE_INDEXOF_OBSOLETES 1

static inline DbArrayofstringRef
db_timezone_get_obsoletes (DbTimezoneRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  guint offset = ((last_end + 0) & (~(gsize)0)) + 0;
  gsize start = offset;
  gsize end = v.size - offset_size * 1;
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbArrayofstringRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

static inline GString *
db_timezone_format (DbTimezoneRef v, GString *s, gboolean type_annotate)
{
  g_string_append (s, "(");
  db_i18n_format (db_timezone_get_name (v), s, type_annotate);
  g_string_append (s, ", ");
  db_arrayofstring_format (db_timezone_get_obsoletes (v), s, type_annotate);
  g_string_append (s, ")");
  return s;
}

static inline char *
db_timezone_print (DbTimezoneRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_timezone_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** DbCoordinate *******************/
#define DB_COORDINATE_TYPESTRING "(dd)"
#define DB_COORDINATE_TYPEFORMAT ((const GVariantType *) DB_COORDINATE_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbCoordinateRef;

typedef struct {
  double lat;
  double lon;
} DbCoordinate;

static inline DbCoordinateRef
db_coordinate_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_COORDINATE_TYPESTRING));
  return (DbCoordinateRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbCoordinateRef
db_coordinate_from_bytes (GBytes *b)
{
  g_assert (g_bytes_get_size (b) == 16);

  return (DbCoordinateRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbCoordinateRef
db_coordinate_from_data (gconstpointer data, gsize size)
{
  g_assert (size == 16);

  return (DbCoordinateRef) { data, size };
}

static inline GVariant *
db_coordinate_dup_to_gvariant (DbCoordinateRef v)
{
  guint8 *duped = _Db_memdup2 (v.base, v.size);
  return g_variant_new_from_data (DB_COORDINATE_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
db_coordinate_to_gvariant (DbCoordinateRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_COORDINATE_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_coordinate_to_owned_gvariant (DbCoordinateRef v, GVariant *base)
{
  return db_coordinate_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_coordinate_peek_as_gvariant (DbCoordinateRef v)
{
  return g_variant_new_from_data (DB_COORDINATE_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbCoordinateRef
db_coordinate_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_COORDINATE_TYPESTRING));
  return db_coordinate_from_data (child.base, child.size);
}

static inline const DbCoordinate *
db_coordinate_peek (DbCoordinateRef v) {
  return (const DbCoordinate *)v.base;
}

#define DB_COORDINATE_INDEXOF_LAT 0

static inline double
db_coordinate_get_lat (DbCoordinateRef v)
{
  guint offset = ((7) & (~(gsize)7)) + 0;
  return (double)G_STRUCT_MEMBER(double, v.base, offset);
}

#define DB_COORDINATE_INDEXOF_LON 1

static inline double
db_coordinate_get_lon (DbCoordinateRef v)
{
  guint offset = ((7) & (~(gsize)7)) + 8;
  return (double)G_STRUCT_MEMBER(double, v.base, offset);
}

static inline GString *
db_coordinate_format (DbCoordinateRef v, GString *s, gboolean type_annotate)
{
  g_string_append (s, "(");
  __Db_gstring_append_double (s, db_coordinate_get_lat (v));
  g_string_append (s, ", ");
  __Db_gstring_append_double (s, db_coordinate_get_lon (v));
  g_string_append (s, ")");
  return s;
}

static inline char *
db_coordinate_print (DbCoordinateRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_coordinate_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** DbArrayofuint16 *******************/
#define DB_ARRAYOFUINT16_TYPESTRING "aq"
#define DB_ARRAYOFUINT16_TYPEFORMAT ((const GVariantType *) DB_ARRAYOFUINT16_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbArrayofuint16Ref;


static inline DbArrayofuint16Ref
db_arrayofuint16_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_ARRAYOFUINT16_TYPESTRING));
  return (DbArrayofuint16Ref) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbArrayofuint16Ref
db_arrayofuint16_from_bytes (GBytes *b)
{
  return (DbArrayofuint16Ref) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbArrayofuint16Ref
db_arrayofuint16_from_data (gconstpointer data, gsize size)
{
  return (DbArrayofuint16Ref) { data, size };
}

static inline GVariant *
db_arrayofuint16_dup_to_gvariant (DbArrayofuint16Ref v)
{
  guint8 *duped = _Db_memdup2 (v.base, v.size);
  return g_variant_new_from_data (DB_ARRAYOFUINT16_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
db_arrayofuint16_to_gvariant (DbArrayofuint16Ref v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_ARRAYOFUINT16_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_arrayofuint16_to_owned_gvariant (DbArrayofuint16Ref v, GVariant *base)
{
  return db_arrayofuint16_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_arrayofuint16_peek_as_gvariant (DbArrayofuint16Ref v)
{
  return g_variant_new_from_data (DB_ARRAYOFUINT16_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbArrayofuint16Ref
db_arrayofuint16_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_ARRAYOFUINT16_TYPESTRING));
  return db_arrayofuint16_from_data (child.base, child.size);
}

static inline gsize
db_arrayofuint16_get_length (DbArrayofuint16Ref v)
{
  gsize length = v.size / 2;
  return length;
}

static inline guint16
db_arrayofuint16_get_at (DbArrayofuint16Ref v, gsize index)
{
  return (guint16)G_STRUCT_MEMBER(guint16, v.base, index * 2);
}

static inline const guint16 *
db_arrayofuint16_peek (DbArrayofuint16Ref v)
{
  return (const guint16 *)v.base;
}

static inline GString *
db_arrayofuint16_format (DbArrayofuint16Ref v, GString *s, gboolean type_annotate)
{
  gsize len = db_arrayofuint16_get_length (v);
  gsize i;
  if (len == 0 && type_annotate)
    g_string_append_printf (s, "@%s ", DB_ARRAYOFUINT16_TYPESTRING);
  g_string_append_c (s, '[');
  for (i = 0; i < len; i++)
    {
      if (i != 0)
        g_string_append (s, ", ");
      g_string_append_printf (s, "%s%"G_GUINT16_FORMAT"", ((i == 0) ? type_annotate : FALSE) ? "uint16 " : "", db_arrayofuint16_get_at (v, i));
    }
  g_string_append_c (s, ']');
  return s;
}

static inline char *
db_arrayofuint16_print (DbArrayofuint16Ref v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_arrayofuint16_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** DbLocation *******************/
#define DB_LOCATION_TYPESTRING "((ss)ss(dd)ssqyqqaqaq)"
#define DB_LOCATION_TYPEFORMAT ((const GVariantType *) DB_LOCATION_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbLocationRef;


static inline DbLocationRef
db_location_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_LOCATION_TYPESTRING));
  return (DbLocationRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbLocationRef
db_location_from_bytes (GBytes *b)
{
  return (DbLocationRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbLocationRef
db_location_from_data (gconstpointer data, gsize size)
{
  return (DbLocationRef) { data, size };
}

static inline GVariant *
db_location_dup_to_gvariant (DbLocationRef v)
{
  guint8 *duped = _Db_memdup2 (v.base, v.size);
  return g_variant_new_from_data (DB_LOCATION_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
db_location_to_gvariant (DbLocationRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_LOCATION_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_location_to_owned_gvariant (DbLocationRef v, GVariant *base)
{
  return db_location_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_location_peek_as_gvariant (DbLocationRef v)
{
  return g_variant_new_from_data (DB_LOCATION_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbLocationRef
db_location_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_LOCATION_TYPESTRING));
  return db_location_from_data (child.base, child.size);
}

#define DB_LOCATION_INDEXOF_NAME 0

static inline DbI18nRef
db_location_get_name (DbLocationRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  guint offset = ((0) & (~(gsize)0)) + 0;
  gsize start = offset;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbI18nRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

#define DB_LOCATION_INDEXOF_FORECAST_ZONE 1

static inline const char *
db_location_get_forecast_zone (DbLocationRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  guint offset = ((last_end + 0) & (~(gsize)0)) + 0;
  G_GNUC_UNUSED const char *base = (const char *)v.base;
  gsize start = offset;
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 1);
  g_assert (start <= end);
  g_assert (end <= v.size);
  g_assert (base[end-1] == 0);
  return &G_STRUCT_MEMBER(const char, v.base, start);
}

#define DB_LOCATION_INDEXOF_RADAR 2

static inline const char *
db_location_get_radar (DbLocationRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 1);
  guint offset = ((last_end + 0) & (~(gsize)0)) + 0;
  G_GNUC_UNUSED const char *base = (const char *)v.base;
  gsize start = offset;
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 2);
  g_assert (start <= end);
  g_assert (end <= v.size);
  g_assert (base[end-1] == 0);
  return &G_STRUCT_MEMBER(const char, v.base, start);
}

#define DB_LOCATION_INDEXOF_COORDINATES 3

static inline DbCoordinateRef
db_location_get_coordinates (DbLocationRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 2);
  guint offset = ((last_end + 7) & (~(gsize)7)) + 0;
  g_assert (offset + 16 <= v.size);
  return (DbCoordinateRef) { G_STRUCT_MEMBER_P(v.base, offset), 16 };
}

static inline const DbCoordinate *
db_location_peek_coordinates (DbLocationRef v) {
  return (DbCoordinate *)db_location_get_coordinates (v).base;
}

#define DB_LOCATION_INDEXOF_COUNTRY_CODE 4

static inline const char *
db_location_get_country_code (DbLocationRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 2);
  guint offset = ((last_end + 7) & (~(gsize)7)) + 16;
  G_GNUC_UNUSED const char *base = (const char *)v.base;
  gsize start = offset;
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 3);
  g_assert (start <= end);
  g_assert (end <= v.size);
  g_assert (base[end-1] == 0);
  return &G_STRUCT_MEMBER(const char, v.base, start);
}

#define DB_LOCATION_INDEXOF_METAR_CODE 5

static inline const char *
db_location_get_metar_code (DbLocationRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 3);
  guint offset = ((last_end + 0) & (~(gsize)0)) + 0;
  G_GNUC_UNUSED const char *base = (const char *)v.base;
  gsize start = offset;
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 4);
  g_assert (start <= end);
  g_assert (end <= v.size);
  g_assert (base[end-1] == 0);
  return &G_STRUCT_MEMBER(const char, v.base, start);
}

#define DB_LOCATION_INDEXOF_TZ_HINT 6

static inline guint16
db_location_get_tz_hint (DbLocationRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 4);
  guint offset = ((last_end + 1) & (~(gsize)1)) + 0;
  g_assert (offset + 2 <= v.size);
  return (guint16)G_STRUCT_MEMBER(guint16, v.base, offset);
}

#define DB_LOCATION_INDEXOF_LEVEL 7

static inline guint8
db_location_get_level (DbLocationRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 4);
  guint offset = ((last_end + 1) & (~(gsize)1)) + 2;
  g_assert (offset + 1 <= v.size);
  return (guint8)G_STRUCT_MEMBER(guint8, v.base, offset);
}

#define DB_LOCATION_INDEXOF_NEAREST 8

static inline guint16
db_location_get_nearest (DbLocationRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 4);
  guint offset = ((last_end + 1) & (~(gsize)1)) + 4;
  g_assert (offset + 2 <= v.size);
  return (guint16)G_STRUCT_MEMBER(guint16, v.base, offset);
}

#define DB_LOCATION_INDEXOF_PARENT 9

static inline guint16
db_location_get_parent (DbLocationRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 4);
  guint offset = ((last_end + 1) & (~(gsize)1)) + 6;
  g_assert (offset + 2 <= v.size);
  return (guint16)G_STRUCT_MEMBER(guint16, v.base, offset);
}

#define DB_LOCATION_INDEXOF_CHILDREN 10

static inline DbArrayofuint16Ref
db_location_get_children (DbLocationRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 4);
  guint offset = ((last_end + 1) & (~(gsize)1)) + 8;
  gsize start = offset;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 5);
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbArrayofuint16Ref) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

static inline const guint16 *
db_location_peek_children (DbLocationRef v, gsize *len) {
  DbArrayofuint16Ref a = db_location_get_children (v);
  if (len != NULL)
    *len = db_arrayofuint16_get_length (a);
  return (const guint16 *)a.base;
}

#define DB_LOCATION_INDEXOF_TIMEZONES 11

static inline DbArrayofuint16Ref
db_location_get_timezones (DbLocationRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 5);
  guint offset = ((last_end + 1) & (~(gsize)1)) + 0;
  gsize start = offset;
  gsize end = v.size - offset_size * 6;
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbArrayofuint16Ref) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

static inline const guint16 *
db_location_peek_timezones (DbLocationRef v, gsize *len) {
  DbArrayofuint16Ref a = db_location_get_timezones (v);
  if (len != NULL)
    *len = db_arrayofuint16_get_length (a);
  return (const guint16 *)a.base;
}

static inline GString *
db_location_format (DbLocationRef v, GString *s, gboolean type_annotate)
{
  g_string_append (s, "(");
  db_i18n_format (db_location_get_name (v), s, type_annotate);
  g_string_append (s, ", ");
  __Db_gstring_append_string (s, db_location_get_forecast_zone (v));
  g_string_append (s, ", ");
  __Db_gstring_append_string (s, db_location_get_radar (v));
  g_string_append (s, ", ");
  db_coordinate_format (db_location_get_coordinates (v), s, type_annotate);
  g_string_append (s, ", ");
  __Db_gstring_append_string (s, db_location_get_country_code (v));
  g_string_append (s, ", ");
  __Db_gstring_append_string (s, db_location_get_metar_code (v));
  g_string_append (s, ", ");
  g_string_append_printf (s, "%s%"G_GUINT16_FORMAT", %s0x%02x, %s%"G_GUINT16_FORMAT", %s%"G_GUINT16_FORMAT", ",
                   type_annotate ? "uint16 " : "",
                   db_location_get_tz_hint (v),
                   type_annotate ? "byte " : "",
                   db_location_get_level (v),
                   type_annotate ? "uint16 " : "",
                   db_location_get_nearest (v),
                   type_annotate ? "uint16 " : "",
                   db_location_get_parent (v));
  db_arrayofuint16_format (db_location_get_children (v), s, type_annotate);
  g_string_append (s, ", ");
  db_arrayofuint16_format (db_location_get_timezones (v), s, type_annotate);
  g_string_append (s, ")");
  return s;
}

static inline char *
db_location_print (DbLocationRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_location_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** DbWorldLocByCountry *******************/
#define DB_WORLD_LOC_BY_COUNTRY_TYPESTRING "a{sq}"
#define DB_WORLD_LOC_BY_COUNTRY_TYPEFORMAT ((const GVariantType *) DB_WORLD_LOC_BY_COUNTRY_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbWorldLocByCountryRef;

typedef struct {
 gconstpointer base;
 gsize size;
} DbWorldLocByCountryEntryRef;


static inline DbWorldLocByCountryRef
db_world_loc_by_country_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_WORLD_LOC_BY_COUNTRY_TYPESTRING));
  return (DbWorldLocByCountryRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbWorldLocByCountryRef
db_world_loc_by_country_from_bytes (GBytes *b)
{
  return (DbWorldLocByCountryRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbWorldLocByCountryRef
db_world_loc_by_country_from_data (gconstpointer data, gsize size)
{
  return (DbWorldLocByCountryRef) { data, size };
}

static inline GVariant *
db_world_loc_by_country_dup_to_gvariant (DbWorldLocByCountryRef v)
{
  guint8 *duped = _Db_memdup2 (v.base, v.size);
  return g_variant_new_from_data (DB_WORLD_LOC_BY_COUNTRY_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
db_world_loc_by_country_to_gvariant (DbWorldLocByCountryRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_WORLD_LOC_BY_COUNTRY_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_world_loc_by_country_to_owned_gvariant (DbWorldLocByCountryRef v, GVariant *base)
{
  return db_world_loc_by_country_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_world_loc_by_country_peek_as_gvariant (DbWorldLocByCountryRef v)
{
  return g_variant_new_from_data (DB_WORLD_LOC_BY_COUNTRY_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbWorldLocByCountryRef
db_world_loc_by_country_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_WORLD_LOC_BY_COUNTRY_TYPESTRING));
  return db_world_loc_by_country_from_data (child.base, child.size);
}


static inline gsize
db_world_loc_by_country_get_length (DbWorldLocByCountryRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize offsets_array_size;
  if (last_end > v.size)
    return 0;
  offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return 0;
  gsize length = offsets_array_size / offset_size;
  return length;
}

static inline DbWorldLocByCountryEntryRef
db_world_loc_by_country_get_at (DbWorldLocByCountryRef v, gsize index)
{
  DbWorldLocByCountryEntryRef res;
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize len = (v.size - last_end) / offset_size;
  gsize start = (index > 0) ? DB_REF_ALIGN(DB_REF_READ_FRAME_OFFSET(v, len - index), 2) : 0;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, len - index - 1);
  g_assert (start <= end);
  g_assert (end <= last_end);
  res = (DbWorldLocByCountryEntryRef) { ((const char *)v.base) + start, end - start };
  return res;
}

static inline const char *
db_world_loc_by_country_entry_get_key (DbWorldLocByCountryEntryRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  G_GNUC_UNUSED const char *base = (const char *)v.base;
  g_assert (end < v.size);
  g_assert (base[end-1] == 0);
  return base;
}

static inline guint16
db_world_loc_by_country_entry_get_value (DbWorldLocByCountryEntryRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize offset = DB_REF_ALIGN(end, 2);
  g_assert (offset == v.size - offset_size - 2);
  return (guint16)*((guint16 *)((char *)v.base + offset));
}

static inline gboolean
db_world_loc_by_country_lookup (DbWorldLocByCountryRef v, const char * key, gsize *index_out, guint16 *out)
{
  const char * canonical_key = key;
  if (v.size == 0)
    return FALSE;
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  if (last_end > v.size)
    return FALSE;
  gsize offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return FALSE;
  gsize len = offsets_array_size / offset_size;
  gsize start = 0;
  gsize end = len;

  while (start < end)
    {
      gsize mid = (end + start) / 2;
      gsize mid_end = DB_REF_READ_FRAME_OFFSET(v, len - mid - 1);
      gsize mid_start = mid == 0 ? 0 : DB_REF_ALIGN(DB_REF_READ_FRAME_OFFSET(v, len - mid), 2);
      g_assert (mid_start <= mid_end);
      g_assert (mid_end <= last_end);
      DbWorldLocByCountryEntryRef e = { ((const char *)v.base) + mid_start, mid_end - mid_start };
      const char * e_key = db_world_loc_by_country_entry_get_key (e);
      gint32 cmp = strcmp(canonical_key, e_key);
      if (cmp == 0)
        {
           if (index_out)
             *index_out = mid;
           if (out)
             *out = db_world_loc_by_country_entry_get_value (e);
           return TRUE;
        }
      if (cmp < 0)
        end = mid; /* canonical_key < e_key */
      else
        start = mid + 1; /* canonical_key > e_key */
    }
    return FALSE;
}

static inline GString *
db_world_loc_by_country_format (DbWorldLocByCountryRef v, GString *s, gboolean type_annotate)
{
  gsize len = db_world_loc_by_country_get_length (v);
  gsize i;

  if (len == 0 && type_annotate)
    g_string_append_printf (s, "@%s ", DB_WORLD_LOC_BY_COUNTRY_TYPESTRING);

  g_string_append_c (s, '{');
  for (i = 0; i < len; i++)
    {
      DbWorldLocByCountryEntryRef entry = db_world_loc_by_country_get_at (v, i);
      if (i != 0)
        g_string_append (s, ", ");
      __Db_gstring_append_string (s, db_world_loc_by_country_entry_get_key (entry));
      g_string_append (s, ": ");
      g_string_append_printf (s, "%s%"G_GUINT16_FORMAT"", type_annotate ? "uint16 " : "", db_world_loc_by_country_entry_get_value (entry));
    }
  g_string_append_c (s, '}');
  return s;
}

static inline char *
db_world_loc_by_country_print (DbWorldLocByCountryRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_world_loc_by_country_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** DbWorldLocByMetar *******************/
#define DB_WORLD_LOC_BY_METAR_TYPESTRING "a{sq}"
#define DB_WORLD_LOC_BY_METAR_TYPEFORMAT ((const GVariantType *) DB_WORLD_LOC_BY_METAR_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbWorldLocByMetarRef;

typedef struct {
 gconstpointer base;
 gsize size;
} DbWorldLocByMetarEntryRef;


static inline DbWorldLocByMetarRef
db_world_loc_by_metar_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_WORLD_LOC_BY_METAR_TYPESTRING));
  return (DbWorldLocByMetarRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbWorldLocByMetarRef
db_world_loc_by_metar_from_bytes (GBytes *b)
{
  return (DbWorldLocByMetarRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbWorldLocByMetarRef
db_world_loc_by_metar_from_data (gconstpointer data, gsize size)
{
  return (DbWorldLocByMetarRef) { data, size };
}

static inline GVariant *
db_world_loc_by_metar_dup_to_gvariant (DbWorldLocByMetarRef v)
{
  guint8 *duped = _Db_memdup2 (v.base, v.size);
  return g_variant_new_from_data (DB_WORLD_LOC_BY_METAR_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
db_world_loc_by_metar_to_gvariant (DbWorldLocByMetarRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_WORLD_LOC_BY_METAR_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_world_loc_by_metar_to_owned_gvariant (DbWorldLocByMetarRef v, GVariant *base)
{
  return db_world_loc_by_metar_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_world_loc_by_metar_peek_as_gvariant (DbWorldLocByMetarRef v)
{
  return g_variant_new_from_data (DB_WORLD_LOC_BY_METAR_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbWorldLocByMetarRef
db_world_loc_by_metar_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_WORLD_LOC_BY_METAR_TYPESTRING));
  return db_world_loc_by_metar_from_data (child.base, child.size);
}


static inline gsize
db_world_loc_by_metar_get_length (DbWorldLocByMetarRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize offsets_array_size;
  if (last_end > v.size)
    return 0;
  offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return 0;
  gsize length = offsets_array_size / offset_size;
  return length;
}

static inline DbWorldLocByMetarEntryRef
db_world_loc_by_metar_get_at (DbWorldLocByMetarRef v, gsize index)
{
  DbWorldLocByMetarEntryRef res;
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize len = (v.size - last_end) / offset_size;
  gsize start = (index > 0) ? DB_REF_ALIGN(DB_REF_READ_FRAME_OFFSET(v, len - index), 2) : 0;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, len - index - 1);
  g_assert (start <= end);
  g_assert (end <= last_end);
  res = (DbWorldLocByMetarEntryRef) { ((const char *)v.base) + start, end - start };
  return res;
}

static inline const char *
db_world_loc_by_metar_entry_get_key (DbWorldLocByMetarEntryRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  G_GNUC_UNUSED const char *base = (const char *)v.base;
  g_assert (end < v.size);
  g_assert (base[end-1] == 0);
  return base;
}

static inline guint16
db_world_loc_by_metar_entry_get_value (DbWorldLocByMetarEntryRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize offset = DB_REF_ALIGN(end, 2);
  g_assert (offset == v.size - offset_size - 2);
  return (guint16)*((guint16 *)((char *)v.base + offset));
}

static inline gboolean
db_world_loc_by_metar_lookup (DbWorldLocByMetarRef v, const char * key, gsize *index_out, guint16 *out)
{
  const char * canonical_key = key;
  if (v.size == 0)
    return FALSE;
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  if (last_end > v.size)
    return FALSE;
  gsize offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return FALSE;
  gsize len = offsets_array_size / offset_size;
  gsize start = 0;
  gsize end = len;

  while (start < end)
    {
      gsize mid = (end + start) / 2;
      gsize mid_end = DB_REF_READ_FRAME_OFFSET(v, len - mid - 1);
      gsize mid_start = mid == 0 ? 0 : DB_REF_ALIGN(DB_REF_READ_FRAME_OFFSET(v, len - mid), 2);
      g_assert (mid_start <= mid_end);
      g_assert (mid_end <= last_end);
      DbWorldLocByMetarEntryRef e = { ((const char *)v.base) + mid_start, mid_end - mid_start };
      const char * e_key = db_world_loc_by_metar_entry_get_key (e);
      gint32 cmp = strcmp(canonical_key, e_key);
      if (cmp == 0)
        {
           if (index_out)
             *index_out = mid;
           if (out)
             *out = db_world_loc_by_metar_entry_get_value (e);
           return TRUE;
        }
      if (cmp < 0)
        end = mid; /* canonical_key < e_key */
      else
        start = mid + 1; /* canonical_key > e_key */
    }
    return FALSE;
}

static inline GString *
db_world_loc_by_metar_format (DbWorldLocByMetarRef v, GString *s, gboolean type_annotate)
{
  gsize len = db_world_loc_by_metar_get_length (v);
  gsize i;

  if (len == 0 && type_annotate)
    g_string_append_printf (s, "@%s ", DB_WORLD_LOC_BY_METAR_TYPESTRING);

  g_string_append_c (s, '{');
  for (i = 0; i < len; i++)
    {
      DbWorldLocByMetarEntryRef entry = db_world_loc_by_metar_get_at (v, i);
      if (i != 0)
        g_string_append (s, ", ");
      __Db_gstring_append_string (s, db_world_loc_by_metar_entry_get_key (entry));
      g_string_append (s, ": ");
      g_string_append_printf (s, "%s%"G_GUINT16_FORMAT"", type_annotate ? "uint16 " : "", db_world_loc_by_metar_entry_get_value (entry));
    }
  g_string_append_c (s, '}');
  return s;
}

static inline char *
db_world_loc_by_metar_print (DbWorldLocByMetarRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_world_loc_by_metar_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** DbWorldTimezones *******************/
#define DB_WORLD_TIMEZONES_TYPESTRING "a{s((ss)as)}"
#define DB_WORLD_TIMEZONES_TYPEFORMAT ((const GVariantType *) DB_WORLD_TIMEZONES_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbWorldTimezonesRef;

typedef struct {
 gconstpointer base;
 gsize size;
} DbWorldTimezonesEntryRef;


static inline DbWorldTimezonesRef
db_world_timezones_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_WORLD_TIMEZONES_TYPESTRING));
  return (DbWorldTimezonesRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbWorldTimezonesRef
db_world_timezones_from_bytes (GBytes *b)
{
  return (DbWorldTimezonesRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbWorldTimezonesRef
db_world_timezones_from_data (gconstpointer data, gsize size)
{
  return (DbWorldTimezonesRef) { data, size };
}

static inline GVariant *
db_world_timezones_dup_to_gvariant (DbWorldTimezonesRef v)
{
  guint8 *duped = _Db_memdup2 (v.base, v.size);
  return g_variant_new_from_data (DB_WORLD_TIMEZONES_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
db_world_timezones_to_gvariant (DbWorldTimezonesRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_WORLD_TIMEZONES_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_world_timezones_to_owned_gvariant (DbWorldTimezonesRef v, GVariant *base)
{
  return db_world_timezones_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_world_timezones_peek_as_gvariant (DbWorldTimezonesRef v)
{
  return g_variant_new_from_data (DB_WORLD_TIMEZONES_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbWorldTimezonesRef
db_world_timezones_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_WORLD_TIMEZONES_TYPESTRING));
  return db_world_timezones_from_data (child.base, child.size);
}


static inline gsize
db_world_timezones_get_length (DbWorldTimezonesRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize offsets_array_size;
  if (last_end > v.size)
    return 0;
  offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return 0;
  gsize length = offsets_array_size / offset_size;
  return length;
}

static inline DbWorldTimezonesEntryRef
db_world_timezones_get_at (DbWorldTimezonesRef v, gsize index)
{
  DbWorldTimezonesEntryRef res;
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize len = (v.size - last_end) / offset_size;
  gsize start = (index > 0) ? DB_REF_ALIGN(DB_REF_READ_FRAME_OFFSET(v, len - index), 1) : 0;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, len - index - 1);
  g_assert (start <= end);
  g_assert (end <= last_end);
  res = (DbWorldTimezonesEntryRef) { ((const char *)v.base) + start, end - start };
  return res;
}

static inline const char *
db_world_timezones_entry_get_key (DbWorldTimezonesEntryRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  G_GNUC_UNUSED const char *base = (const char *)v.base;
  g_assert (end < v.size);
  g_assert (base[end-1] == 0);
  return base;
}

static inline DbTimezoneRef
db_world_timezones_entry_get_value (DbWorldTimezonesEntryRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize offset = DB_REF_ALIGN(end, 1);
  g_assert (offset <= v.size);
  return (DbTimezoneRef) { (char *)v.base + offset, (v.size - offset_size) - offset };
}

static inline gboolean
db_world_timezones_lookup (DbWorldTimezonesRef v, const char * key, gsize *index_out, DbTimezoneRef *out)
{
  const char * canonical_key = key;
  if (v.size == 0)
    return FALSE;
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  if (last_end > v.size)
    return FALSE;
  gsize offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return FALSE;
  gsize len = offsets_array_size / offset_size;
  gsize start = 0;
  gsize end = len;

  while (start < end)
    {
      gsize mid = (end + start) / 2;
      gsize mid_end = DB_REF_READ_FRAME_OFFSET(v, len - mid - 1);
      gsize mid_start = mid == 0 ? 0 : DB_REF_ALIGN(DB_REF_READ_FRAME_OFFSET(v, len - mid), 1);
      g_assert (mid_start <= mid_end);
      g_assert (mid_end <= last_end);
      DbWorldTimezonesEntryRef e = { ((const char *)v.base) + mid_start, mid_end - mid_start };
      const char * e_key = db_world_timezones_entry_get_key (e);
      gint32 cmp = strcmp(canonical_key, e_key);
      if (cmp == 0)
        {
           if (index_out)
             *index_out = mid;
           if (out)
             *out = db_world_timezones_entry_get_value (e);
           return TRUE;
        }
      if (cmp < 0)
        end = mid; /* canonical_key < e_key */
      else
        start = mid + 1; /* canonical_key > e_key */
    }
    return FALSE;
}

static inline GString *
db_world_timezones_format (DbWorldTimezonesRef v, GString *s, gboolean type_annotate)
{
  gsize len = db_world_timezones_get_length (v);
  gsize i;

  if (len == 0 && type_annotate)
    g_string_append_printf (s, "@%s ", DB_WORLD_TIMEZONES_TYPESTRING);

  g_string_append_c (s, '{');
  for (i = 0; i < len; i++)
    {
      DbWorldTimezonesEntryRef entry = db_world_timezones_get_at (v, i);
      if (i != 0)
        g_string_append (s, ", ");
      __Db_gstring_append_string (s, db_world_timezones_entry_get_key (entry));
      g_string_append (s, ": ");
      db_timezone_format (db_world_timezones_entry_get_value (entry), s, type_annotate);
    }
  g_string_append_c (s, '}');
  return s;
}

static inline char *
db_world_timezones_print (DbWorldTimezonesRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_world_timezones_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** DbArrayofLocation *******************/
#define DB_ARRAYOF_LOCATION_TYPESTRING "a((ss)ss(dd)ssqyqqaqaq)"
#define DB_ARRAYOF_LOCATION_TYPEFORMAT ((const GVariantType *) DB_ARRAYOF_LOCATION_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbArrayofLocationRef;


static inline DbArrayofLocationRef
db_arrayof_location_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_ARRAYOF_LOCATION_TYPESTRING));
  return (DbArrayofLocationRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbArrayofLocationRef
db_arrayof_location_from_bytes (GBytes *b)
{
  return (DbArrayofLocationRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbArrayofLocationRef
db_arrayof_location_from_data (gconstpointer data, gsize size)
{
  return (DbArrayofLocationRef) { data, size };
}

static inline GVariant *
db_arrayof_location_dup_to_gvariant (DbArrayofLocationRef v)
{
  guint8 *duped = _Db_memdup2 (v.base, v.size);
  return g_variant_new_from_data (DB_ARRAYOF_LOCATION_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
db_arrayof_location_to_gvariant (DbArrayofLocationRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_ARRAYOF_LOCATION_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_arrayof_location_to_owned_gvariant (DbArrayofLocationRef v, GVariant *base)
{
  return db_arrayof_location_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_arrayof_location_peek_as_gvariant (DbArrayofLocationRef v)
{
  return g_variant_new_from_data (DB_ARRAYOF_LOCATION_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbArrayofLocationRef
db_arrayof_location_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_ARRAYOF_LOCATION_TYPESTRING));
  return db_arrayof_location_from_data (child.base, child.size);
}

static inline gsize
db_arrayof_location_get_length (DbArrayofLocationRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize offsets_array_size;
  if (last_end > v.size)
    return 0;
  offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return 0;
  gsize length  = offsets_array_size / offset_size;
  return length;
}

static inline DbLocationRef
db_arrayof_location_get_at (DbArrayofLocationRef v, gsize index)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize len = (v.size - last_end) / offset_size;
  gsize start = (index > 0) ? DB_REF_ALIGN(DB_REF_READ_FRAME_OFFSET(v, len - index), 8) : 0;
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, len - index - 1);
  g_assert (start <= end);
  g_assert (end <= last_end);
  return (DbLocationRef) { ((const char *)v.base) + start, end - start };
}

static inline GString *
db_arrayof_location_format (DbArrayofLocationRef v, GString *s, gboolean type_annotate)
{
  gsize len = db_arrayof_location_get_length (v);
  gsize i;
  if (len == 0 && type_annotate)
    g_string_append_printf (s, "@%s ", DB_ARRAYOF_LOCATION_TYPESTRING);
  g_string_append_c (s, '[');
  for (i = 0; i < len; i++)
    {
      if (i != 0)
        g_string_append (s, ", ");
      db_location_format (db_arrayof_location_get_at (v, i), s, ((i == 0) ? type_annotate : FALSE));
    }
  g_string_append_c (s, ']');
  return s;
}

static inline char *
db_arrayof_location_print (DbArrayofLocationRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_arrayof_location_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** DbWorld *******************/
#define DB_WORLD_TYPESTRING "(ta{sq}a{sq}a{s((ss)as)}a((ss)ss(dd)ssqyqqaqaq))"
#define DB_WORLD_TYPEFORMAT ((const GVariantType *) DB_WORLD_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbWorldRef;


static inline DbWorldRef
db_world_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_WORLD_TYPESTRING));
  return (DbWorldRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbWorldRef
db_world_from_bytes (GBytes *b)
{
  return (DbWorldRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbWorldRef
db_world_from_data (gconstpointer data, gsize size)
{
  return (DbWorldRef) { data, size };
}

static inline GVariant *
db_world_dup_to_gvariant (DbWorldRef v)
{
  guint8 *duped = _Db_memdup2 (v.base, v.size);
  return g_variant_new_from_data (DB_WORLD_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
db_world_to_gvariant (DbWorldRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_WORLD_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_world_to_owned_gvariant (DbWorldRef v, GVariant *base)
{
  return db_world_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_world_peek_as_gvariant (DbWorldRef v)
{
  return g_variant_new_from_data (DB_WORLD_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbWorldRef
db_world_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = Db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_WORLD_TYPESTRING));
  return db_world_from_data (child.base, child.size);
}

#define DB_WORLD_INDEXOF_MAGIC 0

static inline guint64
db_world_get_magic (DbWorldRef v)
{
  guint offset = ((7) & (~(gsize)7)) + 0;
  g_assert (offset + 8 <= v.size);
  return (guint64)G_STRUCT_MEMBER(guint64, v.base, offset);
}

#define DB_WORLD_INDEXOF_LOC_BY_COUNTRY 1

static inline DbWorldLocByCountryRef
db_world_get_loc_by_country (DbWorldRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  guint offset = ((7) & (~(gsize)7)) + 8;
  gsize start = offset;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbWorldLocByCountryRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

#define DB_WORLD_INDEXOF_LOC_BY_METAR 2

static inline DbWorldLocByMetarRef
db_world_get_loc_by_metar (DbWorldRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  guint offset = ((last_end + 1) & (~(gsize)1)) + 0;
  gsize start = offset;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 1);
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbWorldLocByMetarRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

#define DB_WORLD_INDEXOF_TIMEZONES 3

static inline DbWorldTimezonesRef
db_world_get_timezones (DbWorldRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 1);
  guint offset = ((last_end + 0) & (~(gsize)0)) + 0;
  gsize start = offset;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 2);
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbWorldTimezonesRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

#define DB_WORLD_INDEXOF_LOCATIONS 4

static inline DbArrayofLocationRef
db_world_get_locations (DbWorldRef v)
{
  guint offset_size = Db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 2);
  guint offset = ((last_end + 7) & (~(gsize)7)) + 0;
  gsize start = offset;
  gsize end = v.size - offset_size * 3;
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbArrayofLocationRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

static inline GString *
db_world_format (DbWorldRef v, GString *s, gboolean type_annotate)
{
  g_string_append_printf (s, "(%s%"G_GUINT64_FORMAT", ",
                   type_annotate ? "uint64 " : "",
                   db_world_get_magic (v));
  db_world_loc_by_country_format (db_world_get_loc_by_country (v), s, type_annotate);
  g_string_append (s, ", ");
  db_world_loc_by_metar_format (db_world_get_loc_by_metar (v), s, type_annotate);
  g_string_append (s, ", ");
  db_world_timezones_format (db_world_get_timezones (v), s, type_annotate);
  g_string_append (s, ", ");
  db_arrayof_location_format (db_world_get_locations (v), s, type_annotate);
  g_string_append (s, ")");
  return s;
}

static inline char *
db_world_print (DbWorldRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_world_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}
#endif
