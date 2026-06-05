#pragma once
#include <pebble.h>

// Persist storage keys.
#define PERSIST_SORT_BY      2

// Fixed brand accent color (#5cdd8b) — no longer user-configurable.
#define DEFAULT_ACCENT_COLOR 0x5cdd8b

// Returns the fixed accent color.
GColor config_accent(void);

// Loads the accent color from persist storage (or default).
void config_load(void);

// Registers a callback fired whenever incoming data changes (redraw hook).
void config_set_change_callback(void (*cb)(void));

// AppMessage inbox handler: applies accent + pages + streamed monitors.
void config_inbox_received(DictionaryIterator *iter, void *context);

// Changes the sort order and persists it (used by the in-app menu).
void config_set_sort_by(int sort_by);

// Outbox helpers (watch -> phone).
void config_request_page(int index);  // ask the phone to load page `index`
void config_request_refresh(void);     // ask the phone to re-fetch the active page
