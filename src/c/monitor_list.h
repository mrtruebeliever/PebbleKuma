#pragma once
#include <pebble.h>

// Lazily creates and returns the monitor-list window (the app's root window).
Window *monitor_list_window(void);

// Rebuilds the menu from the current data and redraws.
void monitor_list_reload(void);

// Destroys the root window. Call once at app exit.
void monitor_list_destroy(void);
