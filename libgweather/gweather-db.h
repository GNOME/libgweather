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

#define DB_REF_READ_FRAME_OFFSET(_v, _index) db_ref_read_unaligned_le ((guchar*)((_v).base) + (_v).size - (offset_size * ((_index) + 1)), offset_size)
#define DB_REF_ALIGN(_offset, _align_to) ((_offset + _align_to - 1) & ~(gsize)(_align_to - 1))

/* Note: clz is undefinded for 0, so never call this size == 0 */
G_GNUC_CONST static inline guint
db_ref_get_offset_size (gsize size)
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
db_ref_read_unaligned_le (guchar *bytes, guint   size)
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
__db_gstring_append_double (GString *string, double d)
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
__db_gstring_append_string (GString *string, const char *str)
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
db_variant_get_child (DbVariantRef v, const GVariantType **out_type)
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
db_variant_get_type (DbVariantRef v)
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
db_variant_is_type (DbVariantRef v, const GVariantType *type)
{
   return g_variant_type_equal (db_variant_get_type (v), type);
}

static inline DbVariantRef
db_variant_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), G_VARIANT_TYPE_VARIANT));
  return (DbVariantRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbVariantRef
db_variant_from_bytes (GBytes *b)
{
  return (DbVariantRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbVariantRef
db_variant_from_data (gconstpointer data, gsize size)
{
  return (DbVariantRef) { data, size };
}

static inline GVariant *
db_variant_dup_to_gvariant (DbVariantRef v)
{
  return g_variant_new_from_data (G_VARIANT_TYPE_VARIANT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
}

static inline GVariant *
db_variant_to_gvariant (DbVariantRef v,
                              GDestroyNotify      notify,
                              gpointer            user_data)
{
  return g_variant_new_from_data (G_VARIANT_TYPE_VARIANT, g_memdup (v.base, v.size), v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_variant_to_owned_gvariant (DbVariantRef v,
                                     GVariant *base)
{
  return db_variant_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_variant_peek_as_variant (DbVariantRef v)
{
  return g_variant_new_from_data (G_VARIANT_TYPE_VARIANT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbVariantRef
db_variant_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, G_VARIANT_TYPE_VARIANT));
  return db_variant_from_data (child.base, child.size);
}

static inline GVariant *
db_variant_dup_child_to_gvariant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = db_variant_get_child (v, &type);
  return g_variant_new_from_data (type, g_memdup (child.base, child.size), child.size, TRUE, g_free, NULL);
}

static inline GVariant *
db_variant_peek_child_as_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = db_variant_get_child (v, &type);
  return g_variant_new_from_data (type, child.base, child.size, TRUE, NULL, NULL);
}

static inline GString *
db_variant_format (DbVariantRef v, GString *s, gboolean type_annotate)
{
#ifdef DB_DEEP_VARIANT_FORMAT
  GVariant *gv = db_variant_peek_as_variant (v);
  return g_variant_print_string (gv, s, TRUE);
#else
  const GVariantType  *type = db_variant_get_type (v);
  g_string_append_printf (s, "<@%.*s>", (int)g_variant_type_get_string_length (type), (const char *)type);
  return s;
#endif
}

static inline char *
db_variant_print (DbVariantRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_variant_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}
static inline gboolean
db_variant_get_boolean (DbVariantRef v)
{
  return (gboolean)*((guint8 *)v.base);
}
static inline guint8
db_variant_get_byte (DbVariantRef v)
{
  return (guint8)*((guint8 *)v.base);
}
static inline gint16
db_variant_get_int16 (DbVariantRef v)
{
  return (gint16)*((gint16 *)v.base);
}
static inline guint16
db_variant_get_uint16 (DbVariantRef v)
{
  return (guint16)*((guint16 *)v.base);
}
static inline gint32
db_variant_get_int32 (DbVariantRef v)
{
  return (gint32)*((gint32 *)v.base);
}
static inline guint32
db_variant_get_uint32 (DbVariantRef v)
{
  return (guint32)*((guint32 *)v.base);
}
static inline gint64
db_variant_get_int64 (DbVariantRef v)
{
  return (gint64)*((gint64 *)v.base);
}
static inline guint64
db_variant_get_uint64 (DbVariantRef v)
{
  return (guint64)*((guint64 *)v.base);
}
static inline guint32
db_variant_get_handle (DbVariantRef v)
{
  return (guint32)*((guint32 *)v.base);
}
static inline double
db_variant_get_double (DbVariantRef v)
{
  return (double)*((double *)v.base);
}
static inline const char *
db_variant_get_string (DbVariantRef v)
{
  return (const char *)v.base;
}
static inline const char *
db_variant_get_objectpath (DbVariantRef v)
{
  return (const char *)v.base;
}
static inline const char *
db_variant_get_signature (DbVariantRef v)
{
  return (const char *)v.base;
}

/************** DbIdx *******************/
#define DB_IDX_TYPESTRING "(q)"
#define DB_IDX_TYPEFORMAT ((const GVariantType *) DB_IDX_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbIdxRef;

typedef struct {
  guint16 idx;/* big endian */
} DbIdx;

static inline DbIdxRef
db_idx_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_IDX_TYPESTRING));
  return (DbIdxRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbIdxRef
db_idx_from_bytes (GBytes *b)
{
  g_assert (g_bytes_get_size (b) == 2);

  return (DbIdxRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbIdxRef
db_idx_from_data (gconstpointer data, gsize size)
{
  g_assert (size == 2);

  return (DbIdxRef) { data, size };
}

static inline GVariant *
db_idx_dup_to_gvariant (DbIdxRef v)
{
  return g_variant_new_from_data (DB_IDX_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
}

static inline GVariant *
db_idx_to_gvariant (DbIdxRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_IDX_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_idx_to_owned_gvariant (DbIdxRef v, GVariant *base)
{
  return db_idx_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_idx_peek_as_gvariant (DbIdxRef v)
{
  return g_variant_new_from_data (DB_IDX_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbIdxRef
db_idx_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_IDX_TYPESTRING));
  return db_idx_from_data (child.base, child.size);
}

static inline const DbIdx *
db_idx_peek (DbIdxRef v) {
  return (const DbIdx *)v.base;
}

#define DB_IDX_INDEXOF_IDX 0

static inline guint16
db_idx_get_idx (DbIdxRef v)
{
  guint offset = ((1) & (~(gsize)1)) + 0;
  return GUINT16_FROM_BE((guint16)G_STRUCT_MEMBER(guint16, v.base, offset));
}

static inline GString *
db_idx_format (DbIdxRef v, GString *s, gboolean type_annotate)
{
  g_string_append_printf (s, "(%s%"G_GUINT16_FORMAT",)",
                   type_annotate ? "uint16 " : "",
                   db_idx_get_idx (v));
  return s;
}

static inline char *
db_idx_print (DbIdxRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_idx_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
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
  return g_variant_new_from_data (DB_I18N_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
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
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_I18N_TYPESTRING));
  return db_i18n_from_data (child.base, child.size);
}

#define DB_I18N_INDEXOF_STR 0

static inline const char *
db_i18n_get_str (DbI18nRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  guint offset = ((0) & (~(gsize)0)) + 0;
  const char *base = (const char *)v.base;
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
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  guint offset = ((last_end + 0) & (~(gsize)0)) + 0;
  const char *base = (const char *)v.base;
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
  __db_gstring_append_string (s, db_i18n_get_str (v));
  g_string_append (s, ", ");
  __db_gstring_append_string (s, db_i18n_get_msgctxt (v));
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
  return g_variant_new_from_data (DB_ARRAYOFSTRING_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
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
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_ARRAYOFSTRING_TYPESTRING));
  return db_arrayofstring_from_data (child.base, child.size);
}

static inline gsize
db_arrayofstring_get_length (DbArrayofstringRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = db_ref_get_offset_size (v.size);
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
  guint offset_size = db_ref_get_offset_size (v.size);
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
      __db_gstring_append_string (s, db_arrayofstring_get_at (v, i));
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
  return g_variant_new_from_data (DB_TIMEZONE_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
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
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_TIMEZONE_TYPESTRING));
  return db_timezone_from_data (child.base, child.size);
}

#define DB_TIMEZONE_INDEXOF_NAME 0

static inline DbI18nRef
db_timezone_get_name (DbTimezoneRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
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
  guint offset_size = db_ref_get_offset_size (v.size);
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
  double lat;/* big endian */
  double lon;/* big endian */
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
  return g_variant_new_from_data (DB_COORDINATE_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
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
  DbRef child = db_variant_get_child (v, &type);
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
  return DOUBLE_FROM_BE((double)G_STRUCT_MEMBER(double, v.base, offset));
}

#define DB_COORDINATE_INDEXOF_LON 1

static inline double
db_coordinate_get_lon (DbCoordinateRef v)
{
  guint offset = ((7) & (~(gsize)7)) + 8;
  return DOUBLE_FROM_BE((double)G_STRUCT_MEMBER(double, v.base, offset));
}

static inline GString *
db_coordinate_format (DbCoordinateRef v, GString *s, gboolean type_annotate)
{
  g_string_append (s, "(");
  __db_gstring_append_double (s, db_coordinate_get_lat (v));
  g_string_append (s, ", ");
  __db_gstring_append_double (s, db_coordinate_get_lon (v));
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

/************** DbMaybeCoordinate *******************/
#define DB_MAYBE_COORDINATE_TYPESTRING "m(dd)"
#define DB_MAYBE_COORDINATE_TYPEFORMAT ((const GVariantType *) DB_MAYBE_COORDINATE_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbMaybeCoordinateRef;


static inline DbMaybeCoordinateRef
db_maybe_coordinate_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_MAYBE_COORDINATE_TYPESTRING));
  return (DbMaybeCoordinateRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbMaybeCoordinateRef
db_maybe_coordinate_from_bytes (GBytes *b)
{
  return (DbMaybeCoordinateRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbMaybeCoordinateRef
db_maybe_coordinate_from_data (gconstpointer data, gsize size)
{
  return (DbMaybeCoordinateRef) { data, size };
}

static inline GVariant *
db_maybe_coordinate_dup_to_gvariant (DbMaybeCoordinateRef v)
{
  return g_variant_new_from_data (DB_MAYBE_COORDINATE_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
}

static inline GVariant *
db_maybe_coordinate_to_gvariant (DbMaybeCoordinateRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_MAYBE_COORDINATE_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_maybe_coordinate_to_owned_gvariant (DbMaybeCoordinateRef v, GVariant *base)
{
  return db_maybe_coordinate_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_maybe_coordinate_peek_as_gvariant (DbMaybeCoordinateRef v)
{
  return g_variant_new_from_data (DB_MAYBE_COORDINATE_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbMaybeCoordinateRef
db_maybe_coordinate_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_MAYBE_COORDINATE_TYPESTRING));
  return db_maybe_coordinate_from_data (child.base, child.size);
}

static inline gboolean
db_maybe_coordinate_has_value(DbMaybeCoordinateRef v)
{
  return v.size != 0;
}
static inline DbCoordinateRef
db_maybe_coordinate_get_value (DbMaybeCoordinateRef v)
{
  g_assert (v.size == 16);
  return (DbCoordinateRef) { v.base, v.size };
}
static inline GString *
db_maybe_coordinate_format (DbMaybeCoordinateRef v, GString *s, gboolean type_annotate)
{
  if (type_annotate)
    g_string_append_printf (s, "@%s ", DB_MAYBE_COORDINATE_TYPESTRING);
  if (v.size != 0)
    {
      db_coordinate_format (db_maybe_coordinate_get_value (v), s, FALSE);
    }
  else
    {
      g_string_append (s, "nothing");
    }
  return s;
}

static inline char *
db_maybe_coordinate_print (DbMaybeCoordinateRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_maybe_coordinate_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** DbMaybeIdx *******************/
#define DB_MAYBE_IDX_TYPESTRING "m(q)"
#define DB_MAYBE_IDX_TYPEFORMAT ((const GVariantType *) DB_MAYBE_IDX_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbMaybeIdxRef;


static inline DbMaybeIdxRef
db_maybe_idx_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_MAYBE_IDX_TYPESTRING));
  return (DbMaybeIdxRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbMaybeIdxRef
db_maybe_idx_from_bytes (GBytes *b)
{
  return (DbMaybeIdxRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbMaybeIdxRef
db_maybe_idx_from_data (gconstpointer data, gsize size)
{
  return (DbMaybeIdxRef) { data, size };
}

static inline GVariant *
db_maybe_idx_dup_to_gvariant (DbMaybeIdxRef v)
{
  return g_variant_new_from_data (DB_MAYBE_IDX_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
}

static inline GVariant *
db_maybe_idx_to_gvariant (DbMaybeIdxRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_MAYBE_IDX_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_maybe_idx_to_owned_gvariant (DbMaybeIdxRef v, GVariant *base)
{
  return db_maybe_idx_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_maybe_idx_peek_as_gvariant (DbMaybeIdxRef v)
{
  return g_variant_new_from_data (DB_MAYBE_IDX_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbMaybeIdxRef
db_maybe_idx_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_MAYBE_IDX_TYPESTRING));
  return db_maybe_idx_from_data (child.base, child.size);
}

static inline gboolean
db_maybe_idx_has_value(DbMaybeIdxRef v)
{
  return v.size != 0;
}
static inline DbIdxRef
db_maybe_idx_get_value (DbMaybeIdxRef v)
{
  g_assert (v.size == 2);
  return (DbIdxRef) { v.base, v.size };
}
static inline GString *
db_maybe_idx_format (DbMaybeIdxRef v, GString *s, gboolean type_annotate)
{
  if (type_annotate)
    g_string_append_printf (s, "@%s ", DB_MAYBE_IDX_TYPESTRING);
  if (v.size != 0)
    {
      db_idx_format (db_maybe_idx_get_value (v), s, FALSE);
    }
  else
    {
      g_string_append (s, "nothing");
    }
  return s;
}

static inline char *
db_maybe_idx_print (DbMaybeIdxRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_maybe_idx_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** DbArrayofIdx *******************/
#define DB_ARRAYOF_IDX_TYPESTRING "a(q)"
#define DB_ARRAYOF_IDX_TYPEFORMAT ((const GVariantType *) DB_ARRAYOF_IDX_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} DbArrayofIdxRef;


static inline DbArrayofIdxRef
db_arrayof_idx_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), DB_ARRAYOF_IDX_TYPESTRING));
  return (DbArrayofIdxRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline DbArrayofIdxRef
db_arrayof_idx_from_bytes (GBytes *b)
{
  return (DbArrayofIdxRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline DbArrayofIdxRef
db_arrayof_idx_from_data (gconstpointer data, gsize size)
{
  return (DbArrayofIdxRef) { data, size };
}

static inline GVariant *
db_arrayof_idx_dup_to_gvariant (DbArrayofIdxRef v)
{
  return g_variant_new_from_data (DB_ARRAYOF_IDX_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
}

static inline GVariant *
db_arrayof_idx_to_gvariant (DbArrayofIdxRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (DB_ARRAYOF_IDX_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
db_arrayof_idx_to_owned_gvariant (DbArrayofIdxRef v, GVariant *base)
{
  return db_arrayof_idx_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
db_arrayof_idx_peek_as_gvariant (DbArrayofIdxRef v)
{
  return g_variant_new_from_data (DB_ARRAYOF_IDX_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline DbArrayofIdxRef
db_arrayof_idx_from_variant (DbVariantRef v)
{
  const GVariantType  *type;
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_ARRAYOF_IDX_TYPESTRING));
  return db_arrayof_idx_from_data (child.base, child.size);
}

static inline gsize
db_arrayof_idx_get_length (DbArrayofIdxRef v)
{
  gsize length = v.size / 2;
  return length;
}

static inline DbIdxRef
db_arrayof_idx_get_at (DbArrayofIdxRef v, gsize index)
{
  return (DbIdxRef) { G_STRUCT_MEMBER_P(v.base, index * 2), 2};
}

static inline const DbIdx *
db_arrayof_idx_peek (DbArrayofIdxRef v)
{
  return (const DbIdx *)v.base;
}

static inline GString *
db_arrayof_idx_format (DbArrayofIdxRef v, GString *s, gboolean type_annotate)
{
  gsize len = db_arrayof_idx_get_length (v);
  gsize i;
  if (len == 0 && type_annotate)
    g_string_append_printf (s, "@%s ", DB_ARRAYOF_IDX_TYPESTRING);
  g_string_append_c (s, '[');
  for (i = 0; i < len; i++)
    {
      if (i != 0)
        g_string_append (s, ", ");
      db_idx_format (db_arrayof_idx_get_at (v, i), s, ((i == 0) ? type_annotate : FALSE));
    }
  g_string_append_c (s, ']');
  return s;
}

static inline char *
db_arrayof_idx_print (DbArrayofIdxRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  db_arrayof_idx_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** DbLocation *******************/
#define DB_LOCATION_TYPESTRING "((ss)ssm(dd)ssm(q)ym(q)(q)a(q)a(q))"
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
  return g_variant_new_from_data (DB_LOCATION_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
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
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_LOCATION_TYPESTRING));
  return db_location_from_data (child.base, child.size);
}

#define DB_LOCATION_INDEXOF_NAME 0

static inline DbI18nRef
db_location_get_name (DbLocationRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
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
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  guint offset = ((last_end + 0) & (~(gsize)0)) + 0;
  const char *base = (const char *)v.base;
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
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 1);
  guint offset = ((last_end + 0) & (~(gsize)0)) + 0;
  const char *base = (const char *)v.base;
  gsize start = offset;
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 2);
  g_assert (start <= end);
  g_assert (end <= v.size);
  g_assert (base[end-1] == 0);
  return &G_STRUCT_MEMBER(const char, v.base, start);
}

#define DB_LOCATION_INDEXOF_COORDINATES 3

static inline DbMaybeCoordinateRef
db_location_get_coordinates (DbLocationRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 2);
  guint offset = ((last_end + 7) & (~(gsize)7)) + 0;
  gsize start = offset;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 3);
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbMaybeCoordinateRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

#define DB_LOCATION_INDEXOF_COUNTRY_CODE 4

static inline const char *
db_location_get_country_code (DbLocationRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 3);
  guint offset = ((last_end + 0) & (~(gsize)0)) + 0;
  const char *base = (const char *)v.base;
  gsize start = offset;
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 4);
  g_assert (start <= end);
  g_assert (end <= v.size);
  g_assert (base[end-1] == 0);
  return &G_STRUCT_MEMBER(const char, v.base, start);
}

#define DB_LOCATION_INDEXOF_METAR_CODE 5

static inline const char *
db_location_get_metar_code (DbLocationRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 4);
  guint offset = ((last_end + 0) & (~(gsize)0)) + 0;
  const char *base = (const char *)v.base;
  gsize start = offset;
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 5);
  g_assert (start <= end);
  g_assert (end <= v.size);
  g_assert (base[end-1] == 0);
  return &G_STRUCT_MEMBER(const char, v.base, start);
}

#define DB_LOCATION_INDEXOF_TZ_HINT 6

static inline DbMaybeIdxRef
db_location_get_tz_hint (DbLocationRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 5);
  guint offset = ((last_end + 1) & (~(gsize)1)) + 0;
  gsize start = offset;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 6);
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbMaybeIdxRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

#define DB_LOCATION_INDEXOF_LEVEL 7

static inline guint8
db_location_get_level (DbLocationRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 6);
  guint offset = ((last_end + 0) & (~(gsize)0)) + 0;
  g_assert (offset + 1 < v.size);
  return (guint8)G_STRUCT_MEMBER(guint8, v.base, offset);
}

#define DB_LOCATION_INDEXOF_NEAREST 8

static inline DbMaybeIdxRef
db_location_get_nearest (DbLocationRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 6);
  guint offset = ((last_end + 2) & (~(gsize)1)) + 0;
  gsize start = offset;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 7);
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbMaybeIdxRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

#define DB_LOCATION_INDEXOF_PARENT 9

static inline DbIdxRef
db_location_get_parent (DbLocationRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 7);
  guint offset = ((last_end + 1) & (~(gsize)1)) + 0;
  g_assert (offset + 2 < v.size);
  return (DbIdxRef) { G_STRUCT_MEMBER_P(v.base, offset), 2 };
}

static inline const DbIdx *
db_location_peek_parent (DbLocationRef v) {
  return (DbIdx *)db_location_get_parent (v).base;
}

#define DB_LOCATION_INDEXOF_CHILDREN 10

static inline DbArrayofIdxRef
db_location_get_children (DbLocationRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 7);
  guint offset = ((last_end + 1) & (~(gsize)1)) + 2;
  gsize start = offset;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 8);
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbArrayofIdxRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

static inline const DbIdx *
db_location_peek_children (DbLocationRef v, gsize *len) {
  DbArrayofIdxRef a = db_location_get_children (v);
  if (len != NULL)
    *len = db_arrayof_idx_get_length (a);
  return (const DbIdx *)a.base;
}

#define DB_LOCATION_INDEXOF_TIMEZONES 11

static inline DbArrayofIdxRef
db_location_get_timezones (DbLocationRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 8);
  guint offset = ((last_end + 1) & (~(gsize)1)) + 0;
  gsize start = offset;
  gsize end = v.size - offset_size * 9;
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbArrayofIdxRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

static inline const DbIdx *
db_location_peek_timezones (DbLocationRef v, gsize *len) {
  DbArrayofIdxRef a = db_location_get_timezones (v);
  if (len != NULL)
    *len = db_arrayof_idx_get_length (a);
  return (const DbIdx *)a.base;
}

static inline GString *
db_location_format (DbLocationRef v, GString *s, gboolean type_annotate)
{
  g_string_append (s, "(");
  db_i18n_format (db_location_get_name (v), s, type_annotate);
  g_string_append (s, ", ");
  __db_gstring_append_string (s, db_location_get_forecast_zone (v));
  g_string_append (s, ", ");
  __db_gstring_append_string (s, db_location_get_radar (v));
  g_string_append (s, ", ");
  db_maybe_coordinate_format (db_location_get_coordinates (v), s, type_annotate);
  g_string_append (s, ", ");
  __db_gstring_append_string (s, db_location_get_country_code (v));
  g_string_append (s, ", ");
  __db_gstring_append_string (s, db_location_get_metar_code (v));
  g_string_append (s, ", ");
  db_maybe_idx_format (db_location_get_tz_hint (v), s, type_annotate);
  g_string_append (s, ", ");
  g_string_append_printf (s, "%s0x%02x, ",
                   type_annotate ? "byte " : "",
                   db_location_get_level (v));
  db_maybe_idx_format (db_location_get_nearest (v), s, type_annotate);
  g_string_append (s, ", ");
  db_idx_format (db_location_get_parent (v), s, type_annotate);
  g_string_append (s, ", ");
  db_arrayof_idx_format (db_location_get_children (v), s, type_annotate);
  g_string_append (s, ", ");
  db_arrayof_idx_format (db_location_get_timezones (v), s, type_annotate);
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
#define DB_WORLD_LOC_BY_COUNTRY_TYPESTRING "a{s(q)}"
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
  return g_variant_new_from_data (DB_WORLD_LOC_BY_COUNTRY_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
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
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_WORLD_LOC_BY_COUNTRY_TYPESTRING));
  return db_world_loc_by_country_from_data (child.base, child.size);
}


static inline gsize
db_world_loc_by_country_get_length (DbWorldLocByCountryRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = db_ref_get_offset_size (v.size);
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
  guint offset_size = db_ref_get_offset_size (v.size);
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
  guint offset_size = db_ref_get_offset_size (v.size);
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  const char *base = (const char *)v.base;
  g_assert (end < v.size);
  g_assert (base[end-1] == 0);
  return base;
}

static inline DbIdxRef
db_world_loc_by_country_entry_get_value (DbWorldLocByCountryEntryRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize offset = DB_REF_ALIGN(end, 2);
  g_assert (offset == v.size - offset_size - 2);
  return (DbIdxRef) { (char *)v.base + offset, (v.size - offset_size) - offset };
}

static inline gboolean
db_world_loc_by_country_lookup (DbWorldLocByCountryRef v, const char * key, gsize *index_out, DbIdxRef *out)
{
  const char * canonical_key = key;
  guint offset_size = db_ref_get_offset_size (v.size);
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
      __db_gstring_append_string (s, db_world_loc_by_country_entry_get_key (entry));
      g_string_append (s, ": ");
      db_idx_format (db_world_loc_by_country_entry_get_value (entry), s, type_annotate);
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
#define DB_WORLD_LOC_BY_METAR_TYPESTRING "a{s(q)}"
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
  return g_variant_new_from_data (DB_WORLD_LOC_BY_METAR_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
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
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_WORLD_LOC_BY_METAR_TYPESTRING));
  return db_world_loc_by_metar_from_data (child.base, child.size);
}


static inline gsize
db_world_loc_by_metar_get_length (DbWorldLocByMetarRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = db_ref_get_offset_size (v.size);
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
  guint offset_size = db_ref_get_offset_size (v.size);
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
  guint offset_size = db_ref_get_offset_size (v.size);
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  const char *base = (const char *)v.base;
  g_assert (end < v.size);
  g_assert (base[end-1] == 0);
  return base;
}

static inline DbIdxRef
db_world_loc_by_metar_entry_get_value (DbWorldLocByMetarEntryRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize offset = DB_REF_ALIGN(end, 2);
  g_assert (offset == v.size - offset_size - 2);
  return (DbIdxRef) { (char *)v.base + offset, (v.size - offset_size) - offset };
}

static inline gboolean
db_world_loc_by_metar_lookup (DbWorldLocByMetarRef v, const char * key, gsize *index_out, DbIdxRef *out)
{
  const char * canonical_key = key;
  guint offset_size = db_ref_get_offset_size (v.size);
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
      __db_gstring_append_string (s, db_world_loc_by_metar_entry_get_key (entry));
      g_string_append (s, ": ");
      db_idx_format (db_world_loc_by_metar_entry_get_value (entry), s, type_annotate);
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
  return g_variant_new_from_data (DB_WORLD_TIMEZONES_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
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
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_WORLD_TIMEZONES_TYPESTRING));
  return db_world_timezones_from_data (child.base, child.size);
}


static inline gsize
db_world_timezones_get_length (DbWorldTimezonesRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = db_ref_get_offset_size (v.size);
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
  guint offset_size = db_ref_get_offset_size (v.size);
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
  guint offset_size = db_ref_get_offset_size (v.size);
  G_GNUC_UNUSED gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  const char *base = (const char *)v.base;
  g_assert (end < v.size);
  g_assert (base[end-1] == 0);
  return base;
}

static inline DbTimezoneRef
db_world_timezones_entry_get_value (DbWorldTimezonesEntryRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  gsize offset = DB_REF_ALIGN(end, 1);
  g_assert (offset <= v.size);
  return (DbTimezoneRef) { (char *)v.base + offset, (v.size - offset_size) - offset };
}

static inline gboolean
db_world_timezones_lookup (DbWorldTimezonesRef v, const char * key, gsize *index_out, DbTimezoneRef *out)
{
  const char * canonical_key = key;
  guint offset_size = db_ref_get_offset_size (v.size);
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
      __db_gstring_append_string (s, db_world_timezones_entry_get_key (entry));
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
#define DB_ARRAYOF_LOCATION_TYPESTRING "a((ss)ssm(dd)ssm(q)ym(q)(q)a(q)a(q))"
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
  return g_variant_new_from_data (DB_ARRAYOF_LOCATION_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
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
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_ARRAYOF_LOCATION_TYPESTRING));
  return db_arrayof_location_from_data (child.base, child.size);
}

static inline gsize
db_arrayof_location_get_length (DbArrayofLocationRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = db_ref_get_offset_size (v.size);
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
  guint offset_size = db_ref_get_offset_size (v.size);
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
#define DB_WORLD_TYPESTRING "(a{s(q)}a{s(q)}a{s((ss)as)}a((ss)ssm(dd)ssm(q)ym(q)(q)a(q)a(q)))"
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
  return g_variant_new_from_data (DB_WORLD_TYPEFORMAT, g_memdup (v.base, v.size), v.size, TRUE, g_free, NULL);
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
  DbRef child = db_variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, DB_WORLD_TYPESTRING));
  return db_world_from_data (child.base, child.size);
}

#define DB_WORLD_INDEXOF_LOC_BY_COUNTRY 0

static inline DbWorldLocByCountryRef
db_world_get_loc_by_country (DbWorldRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  guint offset = ((1) & (~(gsize)1)) + 0;
  gsize start = offset;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 0);
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbWorldLocByCountryRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

#define DB_WORLD_INDEXOF_LOC_BY_METAR 1

static inline DbWorldLocByMetarRef
db_world_get_loc_by_metar (DbWorldRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 0);
  guint offset = ((last_end + 1) & (~(gsize)1)) + 0;
  gsize start = offset;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 1);
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbWorldLocByMetarRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

#define DB_WORLD_INDEXOF_TIMEZONES 2

static inline DbWorldTimezonesRef
db_world_get_timezones (DbWorldRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
  gsize last_end = DB_REF_READ_FRAME_OFFSET(v, 1);
  guint offset = ((last_end + 0) & (~(gsize)0)) + 0;
  gsize start = offset;
  gsize end = DB_REF_READ_FRAME_OFFSET(v, 2);
  g_assert (start <= end);
  g_assert (end <= v.size);
  return (DbWorldTimezonesRef) { G_STRUCT_MEMBER_P(v.base, start), end - start };
}

#define DB_WORLD_INDEXOF_LOCATIONS 3

static inline DbArrayofLocationRef
db_world_get_locations (DbWorldRef v)
{
  guint offset_size = db_ref_get_offset_size (v.size);
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
  g_string_append (s, "(");
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
