#include "data.h"

static char    s_page_names[MAX_PAGES][PAGE_NAME_LEN];
static int     s_page_count = 0;
static int     s_active_page = 0;

static Monitor s_monitors[MAX_MONITORS];
static int     s_monitor_count = 0;
static int     s_load_state = LOAD_LOADING;

static int     s_sort_by = SORT_PAGE;
static uint8_t s_order[MAX_MONITORS];  // display position -> raw monitor index

// --- Status pages ------------------------------------------------------------

int data_page_count(void) { return s_page_count; }

void data_set_page_count(int n) {
  if (n < 0) n = 0;
  if (n > MAX_PAGES) n = MAX_PAGES;
  s_page_count = n;
}

const char *data_page_name(int i) {
  if (i < 0 || i >= s_page_count) return "";
  return s_page_names[i];
}

void data_set_page_names(const char *joined) {
  int idx = 0;
  int col = 0;
  for (int i = 0; idx < MAX_PAGES; i++) {
    char c = joined[i];
    if (c == '\n' || c == '\0') {
      s_page_names[idx][col] = '\0';
      idx++;
      col = 0;
      if (c == '\0') break;
    } else if (col < PAGE_NAME_LEN - 1) {
      s_page_names[idx][col++] = c;
    }
  }
  s_page_count = idx;
  if (s_active_page >= s_page_count) s_active_page = 0;
}

int data_active_page(void) { return s_active_page; }

void data_set_active_page(int i) {
  if (i < 0) i = 0;
  if (s_page_count > 0 && i >= s_page_count) i = s_page_count - 1;
  s_active_page = i;
}

// --- Monitors ----------------------------------------------------------------

int data_load_state(void) { return s_load_state; }
void data_set_load_state(int s) { s_load_state = s; }
int data_monitor_count(void) { return s_monitor_count; }

void data_set_monitor_count(int n) {
  if (n < 0) n = 0;
  if (n > MAX_MONITORS) n = MAX_MONITORS;
  s_monitor_count = n;
}

void data_clear_monitors(void) {
  s_monitor_count = 0;
  memset(s_monitors, 0, sizeof(s_monitors));
  data_rebuild_order();
}

Monitor *data_monitor(int i) {
  if (i < 0 || i >= MAX_MONITORS) return NULL;
  return &s_monitors[i];
}

// --- Sorting -----------------------------------------------------------------

// Case-insensitive string compare (Pebble libc lacks strcasecmp).
static int ci_cmp(const char *a, const char *b) {
  while (*a && *b) {
    char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
    char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
    if (ca != cb) return (int)ca - (int)cb;
    a++; b++;
  }
  return (int)*a - (int)*b;
}

// Severity rank: lower sorts first (problems before healthy).
static int severity(uint8_t status) {
  switch (status) {
    case STATUS_DOWN:    return 0;
    case STATUS_PENDING: return 1;
    case STATUS_MAINT:   return 2;
    case STATUS_UP:      return 3;
    default:             return 4;
  }
}

// Returns >0 when monitor `a` should sort after `b` under the active order.
static int order_cmp(int a, int b) {
  Monitor *ma = &s_monitors[a], *mb = &s_monitors[b];
  if (s_sort_by == SORT_STATUS) {
    int d = severity(ma->status) - severity(mb->status);
    if (d != 0) return d;
    // Within the same status, worse uptime (lower %) comes first.
    if (ma->uptime_x100 != mb->uptime_x100) {
      return (int)ma->uptime_x100 - (int)mb->uptime_x100;
    }
    return ci_cmp(ma->name, mb->name);
  }
  // SORT_NAME (SORT_PAGE never reaches here).
  return ci_cmp(ma->name, mb->name);
}

void data_rebuild_order(void) {
  for (int i = 0; i < s_monitor_count; i++) s_order[i] = i;
  if (s_sort_by == SORT_PAGE) return;
  for (int i = 1; i < s_monitor_count; i++) {  // insertion sort (n <= 32)
    uint8_t key = s_order[i];
    int j = i - 1;
    while (j >= 0 && order_cmp(s_order[j], key) > 0) {
      s_order[j + 1] = s_order[j];
      j--;
    }
    s_order[j + 1] = key;
  }
}

void data_set_sort_by(int s) {
  if (s < 0 || s >= SORT_COUNT) s = SORT_PAGE;
  s_sort_by = s;
  data_rebuild_order();
}

int data_sort_by(void) { return s_sort_by; }

Monitor *data_monitor_at(int pos) {
  if (pos < 0 || pos >= s_monitor_count) return NULL;
  return &s_monitors[s_order[pos]];
}

int data_index_at(int pos) {
  if (pos < 0 || pos >= s_monitor_count) return 0;
  return s_order[pos];
}

// --- Display helpers ---------------------------------------------------------

GColor status_color(uint8_t status) {
  switch (status) {
    case STATUS_UP:      return GColorJaegerGreen;     // #00AA55 (deeper than accent)
    case STATUS_DOWN:    return GColorRed;             // #FF0000
    case STATUS_PENDING: return GColorChromeYellow;    // #FFAA00
    case STATUS_MAINT:   return GColorVividCerulean;   // #00AAFF
    default:             return GColorLightGray;
  }
}

// Soft pastel of each status, used as the detail card background.
GColor status_bg_color(uint8_t status) {
  switch (status) {
    case STATUS_UP:      return GColorMintGreen;       // #AAFFAA
    case STATUS_DOWN:    return GColorMelon;           // #FFAAAA
    case STATUS_PENDING: return GColorPastelYellow;    // #FFFFAA
    case STATUS_MAINT:   return GColorCeleste;         // #AAFFFF
    default:             return GColorLightGray;
  }
}

const char *status_label(uint8_t status) {
  switch (status) {
    case STATUS_UP:      return "UP";
    case STATUS_DOWN:    return "DOWN";
    case STATUS_PENDING: return "PENDING";
    case STATUS_MAINT:   return "MAINT";
    default:             return "?";
  }
}
