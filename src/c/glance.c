#include "glance.h"
#include "data.h"

static char s_subtitle[48];

static void reload(AppGlanceReloadSession *session, size_t limit, void *context) {
  if (limit < 1) { return; }
  AppGlanceSlice slice = {
    .layout.icon = APP_GLANCE_SLICE_DEFAULT_ICON,
    .layout.subtitle_template_string = s_subtitle,
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
  };
  AppGlanceResult r = app_glance_add_slice(session, slice);
  if (r != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "glance: add_slice %d", (int)r);
  }
}

void glance_update(void) {
  int state = data_load_state();
  if (state == LOAD_UNCONFIGURED) { app_glance_reload(NULL, NULL); return; }
  // Loading or offline: the last good summary is still the best one to show.
  int n = data_monitor_count();
  if (state != LOAD_OK || n == 0) { return; }

  int down = 0, pending = 0, up = 0;
  for (int i = 0; i < n; i++) {
    Monitor *m = data_monitor(i);
    if (!m) { continue; }
    if (m->status == STATUS_DOWN)         { down++; }
    else if (m->status == STATUS_PENDING) { pending++; }
    else if (m->status == STATUS_UP)      { up++; }
  }

  // The time says how old the picture is: the launcher keeps it until the app
  // runs again.
  char when[12];
  clock_copy_time_string(when, sizeof(when));
  if (down > 0) {
    snprintf(s_subtitle, sizeof(s_subtitle), "%d %s · %s", down, status_label(STATUS_DOWN), when);
  } else if (pending > 0) {
    snprintf(s_subtitle, sizeof(s_subtitle), "%d %s · %s", pending, status_label(STATUS_PENDING), when);
  } else {
    snprintf(s_subtitle, sizeof(s_subtitle), "%d/%d %s · %s", up, n, status_label(STATUS_UP), when);
  }
  app_glance_reload(reload, NULL);
}
