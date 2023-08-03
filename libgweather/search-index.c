/* search-index.c
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "search-index.h"
#include "search-sparse-set.h"

#define SEARCH_INDEX_MAGIC     {0x9F,0x3A,0x74,0xE2}
#define SEARCH_INDEX_ALIGNMENT 8

static inline void
write_uint (GByteArray *bytes,
            guint       value)
{
  do
    {
      guint8 b = ((value > 0x7F) << 7) | (value & 0x7F);
      value >>= 7;
      g_byte_array_append (bytes, &b, 1);
    }
  while (value > 0);
}

static gboolean
_search_trigram_iter_next_char (SearchTrigramIter *iter,
                                gunichar          *ch)
{
  if (iter->pos >= iter->end)
    return FALSE;

  /* Since we're reading files they may not be in modified UTF-8 format.
   * If they're in regular UTF-8 there could be embedded Nil bytes. Handle
   * those specifically because g_utf8_*() will not.
   */

  if G_UNLIKELY (iter->pos[0] == 0)
    {
      *ch = 0;
      iter->pos++;
      return TRUE;
    }

  *ch = g_utf8_get_char_validated (iter->pos, iter->end - iter->pos);

  if (*ch == (gunichar)-1 || *ch == (gunichar)-2)
    {
      iter->pos = iter->end;
      return FALSE;
    }

  iter->pos = g_utf8_next_char (iter->pos);

  return TRUE;
}

void
search_trigram_iter_init (SearchTrigramIter *iter,
                          const char        *text,
                          goffset            len)
{
  if (len < 0)
    len = strlen (text);

  iter->pos = text;
  iter->end = text + len;

  if (_search_trigram_iter_next_char (iter, &iter->trigram.y))
    _search_trigram_iter_next_char (iter, &iter->trigram.z);
}

gboolean
search_trigram_iter_next (SearchTrigramIter *iter,
                          SearchTrigram     *trigram)
{
  if G_UNLIKELY (iter->pos >= iter->end)
    return FALSE;

  iter->trigram.x = iter->trigram.y;
  iter->trigram.y = iter->trigram.z;

  if (!_search_trigram_iter_next_char (iter, &iter->trigram.z))
    return FALSE;

  trigram->x = !g_unichar_isspace (iter->trigram.x) ? iter->trigram.x : '_';
  trigram->y = !g_unichar_isspace (iter->trigram.y) ? iter->trigram.y : '_';
  trigram->z = !g_unichar_isspace (iter->trigram.z) ? iter->trigram.z : '_';

  return TRUE;
}

guint
search_trigram_encode (const SearchTrigram *trigram)
{
  return ((trigram->x & 0xFF) << 16) |
         ((trigram->y & 0xFF) <<  8) |
         ((trigram->z & 0xFF) <<  0);
}

SearchTrigram
search_trigram_decode (guint encoded)
{
  return (SearchTrigram) {
    .x = ((encoded & 0xFF0000) >> 16),
    .y = ((encoded & 0xFF00) >>  8),
    .z = (encoded & 0xFF),
  };
}

typedef struct _SearchIndexBuilderTrigrams
{
  GByteArray *buffer;
  guint32     id;
  guint32     position;
  guint       last_document_id;
} SearchIndexBuilderTrigrams;

typedef struct _SearchIndexBuilderDocument
{
  GBytes     *document;
  guint32     id;
  guint32     position;
} SearchIndexBuilderDocument;

typedef struct _SearchIndexHeader
{
  guint8  magic[4];
  guint32 n_documents;
  guint32 documents;
  guint32 n_documents_bytes;
  guint32 n_trigrams;
  guint32 trigrams;
  guint32 n_trigrams_bytes;
  guint32 trigrams_data;
  guint32 trigrams_data_bytes;
} SearchIndexHeader;

struct _SearchIndexBuilder
{
  GStringChunk    *paths;
  SearchSparseSet  trigrams_set;
  SearchSparseSet  uncommitted_set;
  GArray          *documents;
  GArray          *trigrams;
  GBytes          *current_document;
};

static void
search_index_builder_document_clear (gpointer data)
{
  SearchIndexBuilderDocument *document = data;

  g_clear_pointer (&document->document, g_bytes_unref);
  document->position = 0;
  document->id = 0;
}

static void
search_index_builder_trigrams_clear (gpointer data)
{
  SearchIndexBuilderTrigrams *trigrams = data;

  g_clear_pointer (&trigrams->buffer, g_byte_array_unref);
  trigrams->last_document_id = 0;
}

static void
search_index_builder_finalize (SearchIndexBuilder *builder)
{
  search_sparse_set_clear (&builder->trigrams_set);
  search_sparse_set_clear (&builder->uncommitted_set);
  g_clear_pointer (&builder->paths, g_string_chunk_free);
  g_clear_pointer (&builder->documents, g_array_unref);
  g_clear_pointer (&builder->trigrams, g_array_unref);
}

void
search_index_builder_unref (SearchIndexBuilder *builder)
{
  g_atomic_rc_box_release_full (builder, (GDestroyNotify)search_index_builder_finalize);
}

SearchIndexBuilder *
search_index_builder_new (void)
{
  static const SearchIndexBuilderDocument zero = {0};
  SearchIndexBuilder *builder;

  builder = g_atomic_rc_box_new0 (SearchIndexBuilder);
  builder->documents = g_array_new (FALSE, FALSE, sizeof (SearchIndexBuilderDocument));
  builder->trigrams = g_array_new (FALSE, FALSE, sizeof (SearchIndexBuilderTrigrams));
  builder->paths = g_string_chunk_new (4096*4);
  search_sparse_set_init (&builder->trigrams_set, 1<<24);
  search_sparse_set_init (&builder->uncommitted_set, 1<<24);

  g_array_set_clear_func (builder->documents, search_index_builder_document_clear);
  g_array_set_clear_func (builder->trigrams, search_index_builder_trigrams_clear);

  g_array_append_val (builder->documents, zero);

  return builder;
}

SearchIndexBuilder *
search_index_builder_ref (SearchIndexBuilder *builder)
{
  return g_atomic_rc_box_acquire (builder);
}

guint
search_index_builder_get_n_documents (SearchIndexBuilder *builder)
{
  return builder->documents->len;
}

guint
search_index_builder_get_n_trigrams (SearchIndexBuilder *builder)
{
  return builder->trigrams_set.len;
}

guint
search_index_builder_get_uncommitted (SearchIndexBuilder *builder)
{
  return builder->uncommitted_set.len;
}

void
search_index_builder_add (SearchIndexBuilder  *builder,
                          const SearchTrigram *trigram)
{
  guint trigram_id = search_trigram_encode (trigram);

  search_sparse_set_add (&builder->uncommitted_set, trigram_id);
}

void
search_index_builder_begin (SearchIndexBuilder *builder,
                            GBytes             *document)
{
  g_return_if_fail (builder != NULL);
  g_return_if_fail (document != NULL);
  g_return_if_fail (g_bytes_get_size (document) < G_MAXINT32);

  builder->current_document = g_bytes_ref (document);
}

void
search_index_builder_commit (SearchIndexBuilder *builder)
{
  SearchIndexBuilderDocument document = {
    .document = g_steal_pointer (&builder->current_document),
    .id = builder->documents->len,
    .position = 0,
  };

  g_array_append_val (builder->documents, document);

  for (guint i = 0; i < builder->uncommitted_set.len; i++)
    {
      SearchIndexBuilderTrigrams *trigrams;
      guint trigram_id = builder->uncommitted_set.dense[i].value;
      guint trigrams_index;

      if (!search_sparse_set_get (&builder->trigrams_set, trigram_id, &trigrams_index))
        {
          SearchIndexBuilderTrigrams t;

          t.buffer = g_byte_array_new ();
          t.id = trigram_id;
          t.last_document_id = 0;
          t.position = 0;

          trigrams_index = builder->trigrams->len;
          search_sparse_set_add_with_data (&builder->trigrams_set, trigram_id, trigrams_index);
          g_array_append_val (builder->trigrams, t);
        }

      trigrams = &g_array_index (builder->trigrams, SearchIndexBuilderTrigrams, trigrams_index);
      write_uint (trigrams->buffer, document.id - trigrams->last_document_id);
      trigrams->last_document_id = document.id;
    }

  search_sparse_set_reset (&builder->uncommitted_set);
}

void
search_index_builder_rollback (SearchIndexBuilder *builder)
{
  g_clear_pointer (&builder->current_document, g_bytes_unref);

  search_sparse_set_reset (&builder->uncommitted_set);
}

static int
sort_by_trigram (gconstpointer a,
                 gconstpointer b)
{
  const SearchIndexBuilderTrigrams *atri = a;
  const SearchIndexBuilderTrigrams *btri = b;

  if (atri->id < btri->id)
    return -1;
  else if (atri->id > btri->id)
    return 1;
  else
    return 0;
}

static guint
realign (GByteArray *buffer)
{
  static const guint8 zero[SEARCH_INDEX_ALIGNMENT] = {0};
  gsize rem = buffer->len % SEARCH_INDEX_ALIGNMENT;

  if (rem > 0)
    g_byte_array_append (buffer, zero, SEARCH_INDEX_ALIGNMENT-rem);
  return buffer->len;
}

gboolean
search_index_builder_write (SearchIndexBuilder  *builder,
                            const char          *filename,
                            GError             **error)
{
  GByteArray *buffer;
  gboolean ret;
  guint begin_documents_pos;

  SearchIndexHeader header = {
    .magic = SEARCH_INDEX_MAGIC,
    .n_documents = builder->documents->len,
    .n_trigrams = builder->trigrams->len,
  };

  g_array_sort (builder->trigrams, sort_by_trigram);

  buffer = g_byte_array_new ();
  g_byte_array_append (buffer, (const guint8 *)&header, sizeof header);

  begin_documents_pos = realign (buffer);
  for (guint i = 1; i < builder->documents->len; i++)
    {
      SearchIndexBuilderDocument *document = &g_array_index (builder->documents, SearchIndexBuilderDocument, i);
      const guint8 *data;
      gsize len;

      document->position = buffer->len;

      data = g_bytes_get_data (document->document, &len);
      g_byte_array_append (buffer, data, len);
    }

  header.documents = realign (buffer);
  for (guint i = 0; i < builder->documents->len; i++)
    {
      SearchIndexBuilderDocument *document = &g_array_index (builder->documents, SearchIndexBuilderDocument, i);
      guint32 size = document->document ? g_bytes_get_size (document->document) : 0;

      g_byte_array_append (buffer, (const guint8 *)&document->position, sizeof document->position);
      g_byte_array_append (buffer, (const guint8 *)&size, sizeof size);
    }
  header.n_documents_bytes = buffer->len - begin_documents_pos;

  header.trigrams_data = realign (buffer);
  for (guint i = 0; i < builder->trigrams->len; i++)
    {
      SearchIndexBuilderTrigrams *trigrams = &g_array_index (builder->trigrams, SearchIndexBuilderTrigrams, i);

      g_assert (trigrams->buffer->len > 0);

      trigrams->position = buffer->len;
      g_byte_array_append (buffer,
                           (const guint8 *)trigrams->buffer->data,
                           trigrams->buffer->len);
    }
  header.trigrams_data_bytes = buffer->len - header.trigrams_data;

  header.trigrams = realign (buffer);
  for (guint i = 0; i < builder->trigrams->len; i++)
    {
      SearchIndexBuilderTrigrams *trigrams = &g_array_index (builder->trigrams, SearchIndexBuilderTrigrams, i);
      guint32 end = trigrams->position + trigrams->buffer->len;

      g_byte_array_append (buffer, (const guint8 *)&trigrams->id, sizeof trigrams->id);
      g_byte_array_append (buffer, (const guint8 *)&trigrams->position, sizeof trigrams->position);
      g_byte_array_append (buffer, (const guint8 *)&end, sizeof end);
    }
  header.n_trigrams_bytes = buffer->len - header.trigrams;

  memcpy (buffer->data, &header, sizeof header);

  ret = g_file_set_contents (filename, (const char *)buffer->data, buffer->len, error);
  g_byte_array_unref (buffer);
  return ret;
}

typedef struct _SearchIndexTrigram
{
  guint32 trigram_id;
  guint32 position;
  guint32 end;
} SearchIndexTrigram;

typedef struct _SearchIndexDocument
{
  guint32 position;
  guint32 len;
} SearchIndexDocument;

struct _SearchIndex
{
  GMappedFile         *map;
  SearchIndexTrigram  *trigrams;
  SearchIndexDocument *documents;
  SearchIndexHeader    header;
};

#define SIZE_OVERFLOWS(a,b) (G_UNLIKELY ((b) > 0 && (a) > G_MAXSIZE / (b)))

static inline gboolean
has_space_for (gsize length,
               gsize offset,
               gsize n_items,
               gsize item_size)
{
  gsize avail;
  gsize needed;

  if (offset >= length)
    return FALSE;

  avail = length - offset;

  if (SIZE_OVERFLOWS (n_items, item_size))
    return FALSE;

  needed = n_items * item_size;

  return needed <= avail;
}

SearchIndex *
search_index_new (const char  *filename,
                  GError     **error)
{
  static const guint8 magic[] = SEARCH_INDEX_MAGIC;
  SearchIndex *index;
  GMappedFile *mf;
  const char *data;
  gsize len;

  if (!(mf = g_mapped_file_new (filename, FALSE, error)))
    return NULL;

  if (g_mapped_file_get_length (mf) < sizeof index->header)
    {
      g_set_error_literal (error,
                           G_FILE_ERROR,
                           G_FILE_ERROR_INVAL,
                           "Not a searchindex");
      g_mapped_file_unref (mf);
      return NULL;
    }

  data = g_mapped_file_get_contents (mf);
  len = g_mapped_file_get_length (mf);

  index = g_atomic_rc_box_new0 (SearchIndex);

  memcpy (&index->header, data, sizeof index->header);
  index->map = mf;

  if (memcmp (&index->header.magic, magic, sizeof magic) != 0 ||
      !has_space_for (len, index->header.trigrams, index->header.n_trigrams, sizeof (SearchTrigram)) ||
      !has_space_for (len, index->header.documents, index->header.n_documents, sizeof (SearchIndexDocument)) ||
      index->header.trigrams % SEARCH_INDEX_ALIGNMENT != 0 ||
      index->header.documents % SEARCH_INDEX_ALIGNMENT != 0)
    {
      g_set_error_literal (error,
                           G_FILE_ERROR,
                           G_FILE_ERROR_INVAL,
                           "Not a searchindex");
      search_index_unref (index);
      return NULL;
    }

  index->trigrams = (SearchIndexTrigram *)(gpointer)&data[index->header.trigrams];
  index->documents = (SearchIndexDocument *)(gpointer)&data[index->header.documents];

  return index;
}

SearchIndex *
search_index_ref (SearchIndex *index)
{
  return g_atomic_rc_box_acquire (index);
}

static void
search_index_finalize (SearchIndex *index)
{
  g_clear_pointer (&index->map, g_mapped_file_unref);
}

void
search_index_unref (SearchIndex *index)
{
  return g_atomic_rc_box_release_full (index, (GDestroyNotify)search_index_finalize);
}

static inline const guint8 *
_search_index_find_document_by_id (SearchIndex *index,
                                   guint        document_id,
                                   guint       *out_len)
{
  const SearchIndexDocument *info;
  const char *data;
  gsize len;

  if G_UNLIKELY (document_id == 0 || document_id >= index->header.n_documents)
    return NULL;

  info = &index->documents[document_id];
  data = g_mapped_file_get_contents (index->map);
  len = g_mapped_file_get_length (index->map);

  if G_LIKELY (len > info->len && len - info->len > info->position)
    {
      *out_len = info->len;
      return (const guint8 *)&data[info->position];
    }

  return NULL;
}

/**
 * search_index_find_document_by_id:
 * @self: a #SearchIndex
 * @document_id: the document identifier
 *
 * Locates the bytes for the document specified by @id.
 *
 * Returns: (transfer full) (nullable): a #GBytes if the document was
 *   found otherwise %NULL.
 */
GBytes *
search_index_find_document_by_id (SearchIndex *index,
                                  guint        document_id)
{
  const guint8 *data;
  guint len;

  if ((data = _search_index_find_document_by_id (index, document_id, &len)))
    return g_bytes_new_with_free_func (data, len,
                                       (GDestroyNotify)g_mapped_file_unref,
                                       g_mapped_file_ref (index->map));

  return NULL;
}

static int
find_trigram_by_id_cmp (gconstpointer keyptr,
                        gconstpointer trigramptr)
{
  const guint *key = keyptr;
  const SearchIndexTrigram *trigram = trigramptr;

  if (*key < trigram->trigram_id)
    return -1;
  else if (*key > trigram->trigram_id)
    return 1;
  else
    return 0;
}

static const SearchIndexTrigram *
search_index_find_trigram_by_id (SearchIndex *index,
                                 guint        trigram_id)
{
  return bsearch (&trigram_id,
                  index->trigrams,
                  index->header.n_trigrams,
                  sizeof *index->trigrams,
                  find_trigram_by_id_cmp);
}

static inline gboolean
search_index_iter_init_raw (SearchIndexIter          *iter,
                            SearchIndex              *index,
                            const guint8             *data,
                            gsize                     len,
                            const SearchIndexTrigram *trigrams)
{
  if (trigrams->position >= len || trigrams->end >= len || trigrams->end < trigrams->position)
    return FALSE;

  iter->index = index;
  iter->pos = &data[trigrams->position];
  iter->end = &data[trigrams->end];
  iter->last = 0;

  return TRUE;
}

gboolean
search_index_iter_init (SearchIndexIter     *iter,
                        SearchIndex         *index,
                        const SearchTrigram *trigram)
{
  const SearchIndexTrigram *trigrams;
  const guint8 *data;
  guint trigram_id;
  gsize len;

  trigram_id = search_trigram_encode (trigram);

  if (!(trigrams = search_index_find_trigram_by_id (index, trigram_id)))
    return FALSE;

  data = (const guint8 *)g_mapped_file_get_contents (index->map);
  len = g_mapped_file_get_length (index->map);
  if (trigrams->position >= len || trigrams->end >= len || trigrams->end < trigrams->position)
    return FALSE;

  return search_index_iter_init_raw (iter, index, data, len, trigrams);
}

static gboolean
search_index_iter_next_id (SearchIndexIter *iter,
                           guint           *out_document_id)
{
  guint u = 0, o = 0;
  guint8 b;

  do
    {
      if (iter->pos == 0 || iter->pos >= iter->end || o > 28)
        return FALSE;

      b = *iter->pos;
      u |= ((guint32)(b & 0x7F) << o);
      o += 7;

      iter->pos++;
    }
  while ((b & 0x80) != 0);

  iter->last += u;

  *out_document_id = iter->last;

  return TRUE;
}

gboolean
search_index_iter_next (SearchIndexIter *iter,
                        SearchDocument  *out_document)
{
  guint document_id;

  if (search_index_iter_next_id (iter, &document_id))
    {
      const guint8 *data;
      guint len;

      if ((data = _search_index_find_document_by_id (iter->index, document_id, &len)))
        {
          out_document->id = document_id;
          out_document->data = data;
          out_document->len = len;

          return TRUE;
        }
    }

  return FALSE;
}

gboolean
search_index_iter_seek_to (SearchIndexIter *iter,
                           guint            document_id)
{
  guint ignored;

  do
    {
      if (iter->last >= document_id)
        break;
    }
  while (search_index_iter_next_id (iter, &ignored));

  return iter->last == document_id;
}

gboolean
search_index_builder_merge (SearchIndexBuilder *builder,
                            SearchIndex        *index)
{
  guint document_id_offset;
  const guint8 *data;
  gsize len;

  g_assert (builder->documents != NULL);
  g_assert (builder->documents->len >= 1);

  /* get our starting document id */
  document_id_offset = builder->documents->len - 1;

  /* Make sure enough space for document ids */
  if (G_MAXUINT - document_id_offset < index->header.n_documents)
    return FALSE;

  /* Add all of the documents to the index */
  for (guint i = 1; i < index->header.n_documents; i++)
    {
      SearchIndexBuilderDocument document = {
        .document = search_index_find_document_by_id (index, i),
        .id = builder->documents->len,
        .position = 0,
      };

      g_array_append_val (builder->documents, document);
    }

  data = (const guint8 *)g_mapped_file_get_contents (index->map);
  len = g_mapped_file_get_length (index->map);

  /* Get the array of trigrams so we can iterate them */
  for (guint i = 0; i < index->header.n_trigrams; i++)
    {
      const SearchIndexTrigram *trigrams = &index->trigrams[i];
      SearchIndexBuilderTrigrams *builder_trigrams;
      SearchIndexIter iter;
      guint trigrams_index;
      guint id;

      if (!search_index_iter_init_raw (&iter, index, data, len, trigrams))
        continue;

      if (!search_sparse_set_get (&builder->trigrams_set, trigrams->trigram_id, &trigrams_index))
        {
          SearchIndexBuilderTrigrams t;

          t.buffer = g_byte_array_new ();
          t.id = trigrams->trigram_id;
          t.last_document_id = 0;
          t.position = 0;

          trigrams_index = builder->trigrams->len;
          search_sparse_set_add_with_data (&builder->trigrams_set, trigrams->trigram_id, trigrams_index);
          g_array_append_val (builder->trigrams, t);
        }

      builder_trigrams = &g_array_index (builder->trigrams, SearchIndexBuilderTrigrams, trigrams_index);

      while (search_index_iter_next_id (&iter, &id))
        {
          id += document_id_offset;
          write_uint (builder_trigrams->buffer, id - builder_trigrams->last_document_id);
          builder_trigrams->last_document_id = id;
        }

    }

  return TRUE;
}

void
search_index_stat (SearchIndex     *index,
                   SearchIndexStat *stat)
{
  stat->n_documents = index->header.n_documents;
  stat->n_documents_bytes = index->header.n_documents_bytes;
  stat->n_trigrams = index->header.n_trigrams;
  stat->n_trigrams_bytes = index->header.n_trigrams_bytes;
  stat->trigrams_data_bytes = index->header.trigrams_data_bytes;
}

/**
 * search_index_iter_next_bytes:
 * @iter: a #SearchIndexIter
 * @document: (out): a #SearchDocument
 *
 * Like search_index_iter_next() but returns a new #GBytes of
 * the document data.
 *
 * Returns: (transfer full) (nullable): a #GBytes or %NULL
 */
GBytes *
search_index_iter_next_bytes (SearchIndexIter *iter,
                              SearchDocument  *document)
{
  if (search_index_iter_next (iter, document))
    {
      return g_bytes_new_with_free_func (document->data,
                                         document->len,
                                         (GDestroyNotify)g_mapped_file_unref,
                                         g_mapped_file_ref (iter->index->map));
    }

  return NULL;
}
