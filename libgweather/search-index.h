/* search-index.h
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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef struct _SearchIndex        SearchIndex;
typedef struct _SearchIndexBuilder SearchIndexBuilder;

typedef struct _SearchTrigram
{
  gunichar x;
  gunichar y;
  gunichar z;
} SearchTrigram;

typedef struct _SearchTrigramIter
{
  const char *pos;
  const char *end;
  SearchTrigram trigram;
} SearchTrigramIter;

typedef struct _SearchIndexIter
{
  SearchIndex *index;
  const guint8 *pos;
  const guint8 *end;
  guint last;
} SearchIndexIter;

typedef struct _SearchDocument
{
  guint id;
  const guint8 *data;
  gsize len;
} SearchDocument;

typedef struct _SearchIndexStat
{
  guint n_documents;
  guint n_documents_bytes;
  guint n_trigrams;
  guint n_trigrams_bytes;
  guint trigrams_data_bytes;
} SearchIndexStat;

SearchIndexBuilder *search_index_builder_new             (void);
SearchIndexBuilder *search_index_builder_ref             (SearchIndexBuilder         *builder);
void                search_index_builder_unref           (SearchIndexBuilder         *builder);
void                search_index_builder_begin           (SearchIndexBuilder         *builder,
                                                          GBytes                     *document);
void                search_index_builder_rollback        (SearchIndexBuilder         *builder);
void                search_index_builder_commit          (SearchIndexBuilder         *builder);
void                search_index_builder_add             (SearchIndexBuilder         *builder,
                                                          const SearchTrigram        *trigram);
guint               search_index_builder_get_n_documents (SearchIndexBuilder         *builder);
guint               search_index_builder_get_n_trigrams  (SearchIndexBuilder         *builder);
guint               search_index_builder_get_uncommitted (SearchIndexBuilder         *builder);
gboolean            search_index_builder_merge           (SearchIndexBuilder         *builder,
                                                          SearchIndex                *index);
gboolean            search_index_builder_write           (SearchIndexBuilder         *builder,
                                                          const char                 *filename,
                                                          GError                    **error);
SearchIndex        *search_index_new                     (const char                 *filename,
                                                          GError                    **error);
SearchIndex        *search_index_ref                     (SearchIndex                *index);
void                search_index_unref                   (SearchIndex                *index);
void                search_index_stat                    (SearchIndex                *index,
                                                          SearchIndexStat            *stat);
GBytes             *search_index_find_document_by_id     (SearchIndex                *index,
                                                          guint                       document_id);
gboolean            search_index_iter_init               (SearchIndexIter            *iter,
                                                          SearchIndex                *index,
                                                          const SearchTrigram        *trigram);
gboolean            search_index_iter_next               (SearchIndexIter            *iter,
                                                          SearchDocument             *out_document);
GBytes             *search_index_iter_next_bytes         (SearchIndexIter            *iter,
                                                          SearchDocument             *out_document);
gboolean            search_index_iter_seek_to            (SearchIndexIter            *iter,
                                                          guint                       document_id);
guint               search_trigram_encode                (const SearchTrigram        *trigram);
SearchTrigram       search_trigram_decode                (guint                       encoded);
void                search_trigram_iter_init             (SearchTrigramIter          *iter,
                                                          const char                 *text,
                                                          goffset                     len);
gboolean            search_trigram_iter_next             (SearchTrigramIter          *iter,
                                                          SearchTrigram              *trigram);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SearchIndex, search_index_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (SearchIndexBuilder, search_index_builder_unref)

G_END_DECLS
