#include "config.h"
#include "data.h"
#include "i18n.h"

static void (*s_change_cb)(void) = NULL;
static int s_lang = LANG_EN;   // until config_load() resolves the locale

// Decode an integer tuple by its actual type and width. The real phone app
// packs integers at their smallest width (a 1 arrives as a 1-byte tuple), so
// reading value->int32 blindly picks up neighbouring dict bytes; the emulator
// always sends 4 bytes and never shows the bug. Clay may also send strings.
static int32_t tuple_int(const Tuple *t) {
  switch (t->type) {
    case TUPLE_CSTRING: return (int32_t)atoi(t->value->cstring);
    case TUPLE_INT:  if (t->length == 1) return t->value->int8;
                     if (t->length == 2) return t->value->int16;
                     return t->value->int32;
    case TUPLE_UINT: if (t->length == 1) return t->value->uint8;
                     if (t->length == 2) return t->value->uint16;
                     return (int32_t)t->value->uint32;
    default: return 0;
  }
}

// Maps the watch's system locale (e.g. "en_US", "nl_NL") to a UI language.
// Anything this app does not carry falls back to English.
static int detect_locale_lang(void) {
  const char *loc = i18n_get_system_locale();
  if (loc && loc[0] && loc[1]) {
    if (loc[0] == 'n' && loc[1] == 'l') return LANG_NL;
    if (loc[0] == 'f' && loc[1] == 'r') return LANG_FR;
    if (loc[0] == 'd' && loc[1] == 'e') return LANG_DE;
    if (loc[0] == 'e' && loc[1] == 's') return LANG_ES;
  }
  return LANG_EN;
}

// LANG_AUTO means "follow the watch"; a real index means the user picked one.
static int resolve_lang(int stored) {
  if (stored == LANG_AUTO) return detect_locale_lang();
  if (stored < 0 || stored >= LANG_COUNT) return detect_locale_lang();
  return stored;
}

GColor config_accent(void) { return GColorFromHEX(DEFAULT_ACCENT_COLOR); }

int config_lang(void) { return s_lang; }

void config_set_change_callback(void (*cb)(void)) { s_change_cb = cb; }

void config_load(void) {
  int sort_by = SORT_PAGE;
  if (persist_exists(PERSIST_SORT_BY)) {
    sort_by = persist_read_int(PERSIST_SORT_BY);
  }
  data_set_sort_by(sort_by);

  // No stored preference means follow the watch's own locale rather than
  // defaulting everyone to one language.
  s_lang = resolve_lang(persist_exists(PERSIST_LANG) ? persist_read_int(PERSIST_LANG)
                                                     : LANG_AUTO);
}

void config_set_sort_by(int sort_by) {
  data_set_sort_by(sort_by);
  persist_write_int(PERSIST_SORT_BY, data_sort_by());
}

// Copies one monitor's fields out of a per-monitor AppMessage.
static void apply_monitor(DictionaryIterator *iter) {
  Tuple *t_idx = dict_find(iter, MESSAGE_KEY_MON_INDEX);
  if (!t_idx) return;
  int idx = tuple_int(t_idx);
  Monitor *m = data_monitor(idx);
  if (!m) return;

  // Extend the visible count so newly streamed rows appear progressively.
  if (idx >= data_monitor_count()) {
    data_set_monitor_count(idx + 1);
  }

  Tuple *t;
  if ((t = dict_find(iter, MESSAGE_KEY_MON_NAME))) {
    strncpy(m->name, t->value->cstring, MON_NAME_LEN - 1);
    m->name[MON_NAME_LEN - 1] = '\0';
  }
  m->status = (t = dict_find(iter, MESSAGE_KEY_MON_STATUS)) ? (uint8_t)tuple_int(t)
                                                            : STATUS_UNKNOWN;
  m->uptime_x100 = (t = dict_find(iter, MESSAGE_KEY_MON_UPTIME)) ? (uint16_t)tuple_int(t)
                                                                 : UPTIME_UNKNOWN;
  m->ping = (t = dict_find(iter, MESSAGE_KEY_MON_PING)) ? (int16_t)tuple_int(t)
                                                        : PING_UNKNOWN;
  if ((t = dict_find(iter, MESSAGE_KEY_MON_PSTATS))) {
    strncpy(m->pstats, t->value->cstring, MON_PSTATS_LEN - 1);
    m->pstats[MON_PSTATS_LEN - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_MON_TIME))) {
    strncpy(m->last_time, t->value->cstring, MON_TIME_LEN - 1);
    m->last_time[MON_TIME_LEN - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_MON_TYPE))) {
    strncpy(m->type, t->value->cstring, MON_TYPE_LEN - 1);
    m->type[MON_TYPE_LEN - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_MON_MSG))) {
    strncpy(m->msg, t->value->cstring, MON_MSG_LEN - 1);
    m->msg[MON_MSG_LEN - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_MON_HB))) {
    uint16_t n = t->length;
    if (n > MAX_HB) n = MAX_HB;
    memcpy(m->hb, t->value->data, n);
    m->hb_len = n;
  } else {
    m->hb_len = 0;
  }
}

void config_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;

  if ((t = dict_find(iter, MESSAGE_KEY_LANGUAGE))) {
    int picked = tuple_int(t);
    s_lang = resolve_lang(picked);
    persist_write_int(PERSIST_LANG, picked);   // keep AUTO as AUTO
  }
  if ((t = dict_find(iter, MESSAGE_KEY_SORT_BY))) {
    data_set_sort_by(tuple_int(t));
    persist_write_int(PERSIST_SORT_BY, data_sort_by());
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PAGE_NAMES))) {
    data_set_page_names(t->value->cstring);  // page count is derived from this
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ACTIVE_PAGE))) {
    data_set_active_page(tuple_int(t));
  }
  if ((t = dict_find(iter, MESSAGE_KEY_LOAD_STATE))) {
    int state = tuple_int(t);
    data_set_load_state(state);
    if (state == LOAD_LOADING) {
      data_clear_monitors();
    }
  }
  if ((t = dict_find(iter, MESSAGE_KEY_MON_COUNT))) {
    // Reset the list for a fresh page; monitors arrive in following messages.
    data_clear_monitors();
  }

  // A per-monitor record (these stream one message at a time).
  apply_monitor(iter);

  // Keep the display order in sync with the (possibly updated) monitor set.
  data_rebuild_order();

  if (s_change_cb) {
    s_change_cb();
  }
}

void config_request_page(int index) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_int(out, MESSAGE_KEY_REQUEST_PAGE, &index, sizeof(int), true);
  app_message_outbox_send();
}

void config_request_refresh(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  int one = 1;
  dict_write_int(out, MESSAGE_KEY_REFRESH, &one, sizeof(int), true);
  app_message_outbox_send();
}
