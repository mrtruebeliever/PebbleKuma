#include "config.h"
#include "data.h"
#include "i18n.h"

static void (*s_change_cb)(void) = NULL;
static int s_lang = LANG_NL;

GColor config_accent(void) { return GColorFromHEX(DEFAULT_ACCENT_COLOR); }

int config_lang(void) { return s_lang; }

void config_set_change_callback(void (*cb)(void)) { s_change_cb = cb; }

void config_load(void) {
  int sort_by = SORT_PAGE;
  if (persist_exists(PERSIST_SORT_BY)) {
    sort_by = persist_read_int(PERSIST_SORT_BY);
  }
  data_set_sort_by(sort_by);

  if (persist_exists(PERSIST_LANG)) {
    s_lang = persist_read_int(PERSIST_LANG);
    if (s_lang < 0 || s_lang >= LANG_COUNT) { s_lang = LANG_NL; }
  }
}

void config_set_sort_by(int sort_by) {
  data_set_sort_by(sort_by);
  persist_write_int(PERSIST_SORT_BY, data_sort_by());
}

// Copies one monitor's fields out of a per-monitor AppMessage.
static void apply_monitor(DictionaryIterator *iter) {
  Tuple *t_idx = dict_find(iter, MESSAGE_KEY_MON_INDEX);
  if (!t_idx) return;
  int idx = t_idx->value->int32;
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
  m->status = (t = dict_find(iter, MESSAGE_KEY_MON_STATUS)) ? (uint8_t)t->value->int32
                                                            : STATUS_UNKNOWN;
  m->uptime_x100 = (t = dict_find(iter, MESSAGE_KEY_MON_UPTIME)) ? (uint16_t)t->value->int32
                                                                 : UPTIME_UNKNOWN;
  m->ping = (t = dict_find(iter, MESSAGE_KEY_MON_PING)) ? (int16_t)t->value->int32
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
    s_lang = t->value->int32;
    if (s_lang < 0 || s_lang >= LANG_COUNT) { s_lang = LANG_NL; }
    persist_write_int(PERSIST_LANG, s_lang);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_SORT_BY))) {
    data_set_sort_by(t->value->int32);
    persist_write_int(PERSIST_SORT_BY, data_sort_by());
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PAGE_NAMES))) {
    data_set_page_names(t->value->cstring);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PAGE_COUNT))) {
    data_set_page_count(t->value->int32);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ACTIVE_PAGE))) {
    data_set_active_page(t->value->int32);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_LOAD_STATE))) {
    int state = t->value->int32;
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
