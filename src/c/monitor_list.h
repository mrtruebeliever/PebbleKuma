#pragma once
#include <pebble.h>

// Lazily creates and returns the monitor-list window (the app's root window).
Window *monitor_list_window(void);

// Rebuilds the menu from the current data and redraws.
void monitor_list_reload(void);

// Destroys the root window. Call once at app exit.
void monitor_list_destroy(void);

// Raw touch, forwarded from the app-wide subscription while this window is on top.
bool monitor_list_is_top(void);
void monitor_list_handle_touch(const TouchEvent *event);
