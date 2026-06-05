#pragma once
#include <pebble.h>

// Capacities (fixed arrays — no malloc on the watch).
#define MAX_MONITORS  32
#define MAX_PAGES      8
#define MON_NAME_LEN  40
#define PAGE_NAME_LEN 32
#define MON_MSG_LEN   64
#define MON_PSTATS_LEN 16
#define MON_TIME_LEN   8
#define MON_TYPE_LEN  12
#define MAX_HB        30

// Monitor / heartbeat status values (mirror Uptime Kuma's encoding).
#define STATUS_DOWN     0
#define STATUS_UP       1
#define STATUS_PENDING  2
#define STATUS_MAINT    3
#define STATUS_UNKNOWN  255

// Load state of the active page's monitor list.
#define LOAD_LOADING       0
#define LOAD_OK            1
#define LOAD_ERROR         2  // configured, but the fetch failed (network/VPN)
#define LOAD_UNCONFIGURED  3  // no base URL / pages set yet

// Sort orders.
#define SORT_PAGE    0  // as ordered on the status page
#define SORT_NAME    1  // alphabetical
#define SORT_STATUS  2  // problems first (down, pending, maint, up)
#define SORT_COUNT   3

// Sentinels.
#define UPTIME_UNKNOWN 0xFFFF
#define PING_UNKNOWN   (-1)

// One Uptime Kuma monitor, as streamed from the phone.
typedef struct {
  char     name[MON_NAME_LEN];
  uint8_t  status;                 // STATUS_*
  uint16_t uptime_x100;            // 9950 = 99.50%, UPTIME_UNKNOWN if absent
  int16_t  ping;                   // current ping in ms, PING_UNKNOWN if none
  char     pstats[MON_PSTATS_LEN]; // "min/avg/max" ms, "" if none
  char     last_time[MON_TIME_LEN];// "HH:MM" of last check, "" if none
  char     type[MON_TYPE_LEN];     // monitor type ("http", "ping", ...)
  char     msg[MON_MSG_LEN];       // last heartbeat message ("" when up)
  uint8_t  hb[MAX_HB];             // recent heartbeat statuses, oldest..newest
  uint8_t  hb_len;                 // number of valid entries in hb
} Monitor;

// --- Status pages ------------------------------------------------------------
int         data_page_count(void);
void        data_set_page_count(int n);
const char *data_page_name(int i);
// Fills the page-name table from a '\n'-separated string.
void        data_set_page_names(const char *joined);
int         data_active_page(void);
void        data_set_active_page(int i);

// --- Monitors of the active page --------------------------------------------
int      data_load_state(void);
void     data_set_load_state(int s);
int      data_monitor_count(void);
void     data_set_monitor_count(int n);   // clamps to MAX_MONITORS
void     data_clear_monitors(void);
Monitor *data_monitor(int i);              // by raw index, NULL if out of range

// --- Sorting -----------------------------------------------------------------
void     data_set_sort_by(int s);          // sets order and rebuilds it
int      data_sort_by(void);
void     data_rebuild_order(void);         // recompute the display order
Monitor *data_monitor_at(int pos);         // monitor shown at display position
int      data_index_at(int pos);           // raw index of display position

// --- Display helpers ---------------------------------------------------------
GColor      status_color(uint8_t status);
GColor      status_bg_color(uint8_t status);   // soft pastel, for the card background
const char *status_label(uint8_t status);
