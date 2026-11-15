#pragma once

#include <glib.h>

// This parser extracts tags, priority etc. from the task text.
typedef struct {
  gchar *text;     // Clean text
  GPtrArray *tags; // gchar*
  gint64 priority; // 0..9
} ErrandsTaskParser;

// ---------- PRIVATE ---------- //

static inline ErrandsTaskParser *errands__task_parser_new(void) {
  ErrandsTaskParser *p = g_new0(ErrandsTaskParser, 1);
  p->tags = g_ptr_array_new_with_free_func(g_free);
  return p;
}

static inline void errands__task_parser_free(ErrandsTaskParser *p) {
  g_clear_pointer(&p->text, g_free);
  g_clear_pointer(&p->tags, g_ptr_array_unref);
  g_clear_pointer(&p, g_free);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(ErrandsTaskParser, errands__task_parser_free)

// ---------- PUBLIC ---------- //

static inline ErrandsTaskParser *errands_task_parser_parse(const char *text) {
  ErrandsTaskParser *p = errands__task_parser_new();
  p->priority = -1;
  if (!text) return p;

  g_autoptr(GRegex) re_tags = g_regex_new("##(\\S+)", 0, 0, NULL);
  g_autoptr(GRegex) re_priority = g_regex_new("#p([0-9])\\b", 0, 0, NULL);
  g_autoptr(GRegex) re_ws = g_regex_new("\\s+", 0, 0, NULL);

  // Priority: #pN (N = 0..9), last one wins
  GMatchInfo *mi = NULL;
  if (g_regex_match(re_priority, text, 0, &mi)) {
    while (g_match_info_matches(mi)) {
      gchar *digit = g_match_info_fetch(mi, 1);
      if (digit && *digit) p->priority = digit[0] - '0';
      g_free(digit);
      g_match_info_next(mi, NULL);
    }
  }
  g_match_info_free(mi);

  // Remove #pN tokens from text
  gchar *s1 = g_regex_replace_literal(re_priority, text, -1, 0, "", 0, NULL);

  // Tags: ##tag
  mi = NULL;
  if (g_regex_match(re_tags, s1, 0, &mi)) {
    while (g_match_info_matches(mi)) {
      gchar *tag = g_match_info_fetch(mi, 1);
      if (tag && *tag) g_ptr_array_add(p->tags, tag);
      else g_free(tag);
      g_match_info_next(mi, NULL);
    }
  }
  g_match_info_free(mi);

  // Strip tags, then normalize whitespace
  gchar *s2 = g_regex_replace_literal(re_tags, s1, -1, 0, "", 0, NULL);
  gchar *clean = g_regex_replace_literal(re_ws, s2, -1, 0, " ", 0, NULL);
  g_strstrip(clean);
  p->text = *clean ? clean : (g_free(clean), NULL);

  g_free(s1);
  g_free(s2);

  return p;
}
