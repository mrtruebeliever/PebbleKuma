#pragma once
#include <pebble.h>

// Pushes the detail card deck, focused on the monitor at display `position`.
// Up/down then page between monitors in the current (sorted) list order.
void monitor_detail_push(int position);

// Re-reads the current monitor and redraws (if the detail window is shown).
void monitor_detail_reload(void);

// True while the detail window is on top of the stack.
bool monitor_detail_is_shown(void);

// Destroys the reusable detail window. Call once at app exit.
void monitor_detail_destroy(void);

// Raw touch, forwarded from the app-wide subscription while this window is on top.
bool monitor_detail_is_top(void);
void monitor_detail_handle_touch(const TouchEvent *event);
