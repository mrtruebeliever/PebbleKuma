#include <pebble.h>
#include "config.h"
#include "data.h"
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

static void init(void) {
  config_load();
  data_set_load_state(LOAD_LOADING);
  config_set_change_callback(on_data_changed);

  app_message_register_inbox_received(config_inbox_received);
  app_message_open(2048, 256);

  window_stack_push(monitor_list_window(), true);
}

static void deinit(void) {
  window_destroy(monitor_list_window());
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
