#include <pebble.h>
#include "config.h"
#include "data.h"
#include "i18n.h"
#include "monitor_list.h"
#include "monitor_detail.h"

// Fired by config_inbox_received whenever incoming data changes: redraw
// whichever window is currently visible.
static void on_data_changed(void) {
  monitor_list_reload();
  if (monitor_detail_is_shown()) {
    monitor_detail_reload();
  }
}

// An inbox message was dropped before we could read it (buffer pressure / BT).
static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d", (int)reason);
}

// A page/refresh request failed to reach the phone — surface it so the watch
// doesn't sit on a stale "loading" forever; tapping a row retries.
static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason,
                          void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
  data_set_load_state(LOAD_ERROR);
  on_data_changed();
}

// Subscribing powers the touch sensor on and is the one touch path that works on
// firmware 4.33.1; the system navigation bridge crashes the app (see below).
// Events go to the window on top.
static void touch_dispatch(const TouchEvent *event, void *context) {
  if (monitor_detail_is_top())    { monitor_detail_handle_touch(event); }
  else if (monitor_list_is_top()) { monitor_list_handle_touch(event); }
}

static void init(void) {
  // No app_touch_navigation_enable() here: on firmware 4.33.1 the system touch
  // bridge faults inside firmware on the first touch and kills the app. Buttons
  // only until a later firmware fixes it.
  config_load();
  data_set_stall_callback(on_data_changed);
  data_set_load_state(LOAD_LOADING);
  config_set_change_callback(on_data_changed);
  app_message_register_inbox_received(config_inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(2048, 256);
  window_stack_push(monitor_list_window(), true);
  touch_service_subscribe(touch_dispatch, NULL);
}

static void deinit(void) {
  monitor_detail_destroy();
  monitor_list_destroy();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
