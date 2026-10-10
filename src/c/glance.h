#pragma once
#include <pebble.h>

// The app's line in the launcher ("app glance"): how the active status page
// stood when the app last loaded it, e.g. "2 DOWN · 14:05" or "12/12 UP · 14:05".
// Called when the app closes.
void glance_update(void);
