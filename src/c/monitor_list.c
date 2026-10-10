#include "monitor_list.h"
#include "touch_nav.h"
#include "monitor_detail.h"
#include "data.h"
#include "config.h"
#include "i18n.h"

// Branded header bar (replaces the plain system StatusBarLayer): accent fill,
// small bear icon, app name, and the clock — same PebbleDoist-style treatment,
// kept at the same height the system bar used so it costs nothing extra
// against the row budget below.
// A round screen's top edge is too short for name, icon and clock: there the
// header is just the clock, centred, and a little taller so it clears the
// bezel.
#define HEADER_H PBL_IF_ROUND_ELSE(30, 20)

// Rows are pulled in from the sides on a round screen, where only the focused
// row in the middle has the full width.
#define ROW_INSET PBL_IF_ROUND_ELSE(22, 0)

// The row names: 24 bold, or 18 on the narrow 144px screen so they are not cut
// off after a few letters -- that is the size at the watch's default text size
// (Settings → Display; Large on screens 200px+ tall, Medium below). One step
// smaller or larger follows the setting. Rows grow with the name.
typedef struct { const char *font; int16_t h; } NameSize;
static const NameSize NAME_SIZES[] = {
  { FONT_KEY_GOTHIC_14_BOLD, 16 }, { FONT_KEY_GOTHIC_18_BOLD, 20 },
  { FONT_KEY_GOTHIC_24_BOLD, 24 }, { FONT_KEY_GOTHIC_28_BOLD, 30 },
};
static const NameSize *s_name = &NAME_SIZES[PBL_DISPLAY_WIDTH < 180 ? 1 : 2];

static void pick_name_size(void) {
  int def = PBL_DISPLAY_HEIGHT >= 200 ? PreferredContentSizeLarge : PreferredContentSizeMedium;
  int step = (int)preferred_content_size() - def;
  int i = (PBL_DISPLAY_WIDTH < 180 ? 1 : 2) + (step < -1 ? -1 : (step > 1 ? 1 : step));
  s_name = &NAME_SIZES[i];
}

#define ROW_NAME_FONT (s_name->font)
#define ROW_NAME_H    (s_name->h)

static Window   *s_window = NULL;
static MenuLayer *s_menu = NULL;
static Layer    *s_header = NULL;
static GBitmap  *s_header_icon = NULL;

static void header_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(config_accent(), GColorWhite));
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, b.size.h - 1), GPoint(b.size.w, b.size.h - 1));

  GFont f14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);
#if defined(PBL_ROUND)
  char clock_buf[16];
  clock_copy_time_string(clock_buf, sizeof(clock_buf));
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, clock_buf, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(0, b.size.h - 18, b.size.w, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  return;
#endif
  int name_x = 4;
  if (s_header_icon) {
    GRect ib = gbitmap_get_bounds(s_header_icon);
    int iy = (b.size.h - ib.size.h) / 2;
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_header_icon, GRect(4, iy, ib.size.w, ib.size.h));
    name_x = 4 + ib.size.w + 4;
  }

  char time_buf[16];
  clock_copy_time_string(time_buf, sizeof(time_buf));
  // Just wide enough for "23:59" / "11:59 PM": on a 144px screen every pixel
  // the clock does not need goes to the app name.
  const int time_w = clock_is_24h_style() ? 36 : 56;
  int time_left = b.size.w - 4 - time_w;
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, time_buf, f14, GRect(time_left, 2, time_w, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  graphics_draw_text(ctx, "PebbleKuma", f14, GRect(name_x, 2, time_left - name_x - 4, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void header_tick(struct tm *tick_time, TimeUnits units_changed) {
  if (s_header) { layer_mark_dirty(s_header); }
}

// Number of body rows below the page-selector row (row 0).
static int body_rows(void) {
  if (data_load_state() == LOAD_OK && data_monitor_count() > 0) {
    return data_monitor_count();
  }
  return 1;  // a single status row (loading / error / empty)
}

// --- ActionMenu: switch page / change sort ----------------------------------

static void page_action_performed(ActionMenu *menu, const ActionMenuItem *item,
                                  void *context) {
  int idx = (int)(uintptr_t)action_menu_item_get_action_data(item);
  data_set_active_page(idx);
  data_set_load_state(LOAD_LOADING);
  data_clear_monitors();
  config_request_page(idx);
  monitor_list_reload();
}

static void sort_action_performed(ActionMenu *menu, const ActionMenuItem *item,
                                  void *context) {
  int sort_by = (int)(uintptr_t)action_menu_item_get_action_data(item);
  config_set_sort_by(sort_by);
  monitor_list_reload();
}

static void options_menu_did_close(ActionMenu *menu, const ActionMenuItem *performed,
                                   void *context) {
  // `context` is the root level we created in open_options_menu().
  action_menu_hierarchy_destroy((ActionMenuLevel *)context, NULL, NULL);
}

static void open_options_menu(void) {
  int pages = data_page_count();

  // Root: a "Sorteren" submenu followed by one action per status page.
  ActionMenuLevel *root = action_menu_level_create(1 + (pages > 0 ? pages : 0));

  ActionMenuLevel *sort = action_menu_level_create(SORT_COUNT);
  action_menu_level_add_action(sort, i18n(STR_AM_PAGEORDER), sort_action_performed,
                               (void *)(uintptr_t)SORT_PAGE);
  action_menu_level_add_action(sort, i18n(STR_AM_NAME), sort_action_performed,
                               (void *)(uintptr_t)SORT_NAME);
  action_menu_level_add_action(sort, i18n(STR_AM_STATUS), sort_action_performed,
                               (void *)(uintptr_t)SORT_STATUS);
  action_menu_level_add_child(root, sort, i18n(STR_MENU_SORT_TITLE));

  for (int i = 0; i < pages; i++) {
    action_menu_level_add_action(root, data_page_name(i),
                                 page_action_performed, (void *)(uintptr_t)i);
  }

  // These two do not mean what they look like they mean, so do not "fix" them:
  // verified on the emulator by setting .foreground to red, .background colours
  // the narrow rail down the left edge and .foreground colours only the small
  // crumb dot on it. The sheet itself is always black with white focused and
  // grey unfocused item text, and that is not configurable through this API.
  ActionMenuConfig config = {
    .root_level = root,
    .colors = {
      .background = config_accent(),
      .foreground = GColorBlack,
    },
    .align = ActionMenuAlignCenter,
    .did_close = options_menu_did_close,
    .context = root,
  };
  action_menu_open(&config);
}

// --- MenuLayer callbacks -----------------------------------------------------

static uint16_t get_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  return 1 + body_rows();
}

// Monitor rows are 40px (was 44) so 4 fit on screen under the page-selector
// row instead of 3; the page-selector row itself keeps its original height,
// since it also carries the sort/page-switch ActionMenu entry point.
static int16_t get_cell_height(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  if (ci->row == 0) return ROW_NAME_H + 20;
  return ROW_NAME_H + 16;
}

// Text colours on a row. The colour highlight is the light accent, so text
// stays black on it; black-and-white highlights in black, so text inverts.
static GColor row_fg(bool hl) {
  return PBL_IF_COLOR_ELSE(GColorBlack, hl ? GColorWhite : GColorBlack);
}

static GColor row_sub(bool hl) {
  return PBL_IF_COLOR_ELSE(hl ? GColorBlack : GColorDarkGray, row_fg(hl));
}

// The selected row: the accent, or black where the accent would only dither.
static GColor highlight_bg(void) {
  return PBL_IF_COLOR_ELSE(config_accent(), GColorBlack);
}

// The row's usable area: the cell, less the round inset.
static GRect row_area(const Layer *cell) {
  GRect b = layer_get_bounds(cell);
  return GRect(ROW_INSET, 0, b.size.w - 2 * ROW_INSET, b.size.h);
}

// Draws the page-selector row (row 0).
static void draw_page_row(GContext *ctx, const Layer *cell) {
  GRect b = row_area(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  GColor title_c = row_fg(hl);   // accent is too light to read on the white list
  GColor sub_c   = row_sub(hl);

  const char *page = data_page_count() > 0 ? data_page_name(data_active_page()) : "-";
  char title[PAGE_NAME_LEN + 16];
  snprintf(title, sizeof(title), i18n(STR_PAGE_FMT), page);

  graphics_context_set_text_color(ctx, title_c);
  graphics_draw_text(ctx, title, fonts_get_system_font(ROW_NAME_FONT),
                     GRect(b.origin.x + 8, 0, b.size.w - 12, ROW_NAME_H + 4),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  char sub[48];
  snprintf(sub, sizeof(sub), i18n(STR_MENU_SORT_FMT), i18n(STR_SORT_PAGE + data_sort_by()));
  graphics_context_set_text_color(ctx, sub_c);
  graphics_draw_text(ctx, sub,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(b.origin.x + 8, ROW_NAME_H + 2, b.size.w - 12, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

// Draws a status message row (loading / error / empty).
static void draw_status_row(GContext *ctx, const Layer *cell) {
  GRect b = row_area(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  const char *title, *sub;
  switch (data_load_state()) {
    case LOAD_LOADING:      title = i18n(STR_LOADING_T); sub = i18n(STR_LOADING_S); break;
    case LOAD_ERROR:        title = i18n(STR_ERROR_T);   sub = i18n(STR_ERROR_S);   break;
    case LOAD_UNCONFIGURED: title = i18n(STR_UNCONF_T);  sub = i18n(STR_UNCONF_S);  break;
    default:                title = i18n(STR_EMPTY_T);   sub = "";                  break;
  }
  graphics_context_set_text_color(ctx, row_fg(hl));
  graphics_draw_text(ctx, title, fonts_get_system_font(ROW_NAME_FONT),
                     GRect(b.origin.x + 8, 0, b.size.w - 12, ROW_NAME_H),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, row_sub(hl));
  graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(b.origin.x + 8, ROW_NAME_H - 1, b.size.w - 12, 16),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

// Draws one monitor row (by display position): status dot + name + uptime.
// The uptime percentage draws separately from the status word, bold, so it
// stands out as the number people open this app to check — but at the same
// GOTHIC_14 size as the status word (not GOTHIC_18_BOLD) so the row stays
// compact (40px tall, 4 fit on screen).
static void draw_monitor_row(GContext *ctx, const Layer *cell, int pos) {
  Monitor *m = data_monitor_at(pos);
  if (!m) { return; }
  GRect b = row_area(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  GColor sub_c = row_sub(hl);
  const int16_t x = b.origin.x + 30;   // text column, right of the mark

  // Status mark at the left.
  draw_status_mark(ctx, GPoint(b.origin.x + 16, b.size.h / 2), 6, m->status, row_fg(hl));

  graphics_context_set_text_color(ctx, row_fg(hl));
  graphics_draw_text(ctx, m->name, fonts_get_system_font(ROW_NAME_FONT),
                     GRect(x, 0, b.size.w - 36, ROW_NAME_H),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  GFont f14  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f14b = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  const char *status = status_label(m->status);
  GSize sw = graphics_text_layout_get_content_size(status, f14,
                 GRect(0, 0, b.size.w - 36, 16), GTextOverflowModeFill, GTextAlignmentLeft);

  graphics_context_set_text_color(ctx, sub_c);
  graphics_draw_text(ctx, status, f14, GRect(x, ROW_NAME_H - 1, b.size.w - 36, 16),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  if (m->uptime_x100 != UPTIME_UNKNOWN) {
    char pct[16];
    snprintf(pct, sizeof(pct), "%d.%02d%%", m->uptime_x100 / 100, m->uptime_x100 % 100);
    int16_t px = x + sw.w + 8;
    int16_t pw = b.size.w - 36 - sw.w - 8;
    if (pw > 0) {
      graphics_draw_text(ctx, pct, f14b, GRect(px, ROW_NAME_H - 1, pw, 16),
                         GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    }
  }
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *ci, void *c) {
  if (ci->row == 0) {
    draw_page_row(ctx, cell);
  } else if (data_load_state() == LOAD_OK && data_monitor_count() > 0) {
    draw_monitor_row(ctx, cell, ci->row - 1);
  } else {
    draw_status_row(ctx, cell);
  }
}

static void select_click(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  if (ci->row == 0) {
    open_options_menu();
    return;
  }
  if (data_load_state() == LOAD_OK && data_monitor_count() > 0) {
    monitor_detail_push(ci->row - 1);  // display position; deck pages from here
  } else if (data_load_state() == LOAD_ERROR || data_load_state() == LOAD_LOADING) {
    // Retrying from the loading row too: if the phone swallowed the first
    // request, pressing the only row on screen should re-issue it rather than
    // do nothing.
    data_set_load_state(LOAD_LOADING);
    config_request_refresh();
    monitor_list_reload();
  }
}

static void select_long_click(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  open_options_menu();
}

// --- Touch -------------------------------------------------------------------
//
// The system touch bridge (app_touch_navigation_enable) faults inside firmware
// on 4.33.1, so touch is handled here from the raw event stream — see touch_nav.h.

// Keeps the selection on a visible row after a drag or glide: SELECT would
// otherwise act on a row that scrolled out of sight, and the next UP/DOWN would
// yank the list back. The middle row is where button navigation puts it anyway.
static void touch_settled(void) {
  if (!s_menu) { return; }
  GRect frame = layer_get_frame(menu_layer_get_layer(s_menu));
  int row = touch_nav_row_at(s_menu, GPoint(frame.size.w / 2, frame.origin.y + frame.size.h / 2),
                             get_num_rows(s_menu, 0, NULL), get_cell_height);
  if (row >= 0) {
    menu_layer_set_selected_index(s_menu, MenuIndex(0, (uint16_t)row), MenuRowAlignNone, false);
    // MenuRowAlignNone does not scroll, and without a scroll the menu never
    // repaints — the highlight would stay invisible until the next button press.
    layer_mark_dirty(menu_layer_get_layer(s_menu));
  }
}

bool monitor_list_is_top(void) {
  return s_window && window_stack_get_top_window() == s_window;
}

void monitor_list_handle_touch(const TouchEvent *event) {
  if (!s_menu) { return; }
  GPoint point;
  switch (touch_nav_feed(menu_layer_get_scroll_layer(s_menu), event, &point)) {
    case TOUCH_NAV_TAP: {
      int row = touch_nav_row_at(s_menu, point, get_num_rows(s_menu, 0, NULL), get_cell_height);
      if (row < 0) { return; }   // the header bar, or past the last row
      MenuIndex index = MenuIndex(0, (uint16_t)row);
      menu_layer_set_selected_index(s_menu, index, MenuRowAlignNone, false);
      select_click(s_menu, &index, NULL);
      break;
    }
    case TOUCH_NAV_BACK:
      window_stack_pop(true);   // root window, so this leaves the app — same as BACK
    default:
      break;
  }
}

// --- Window ------------------------------------------------------------------

static void window_load(Window *window) {
  pick_name_size();
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_header_icon = gbitmap_create_with_resource(RESOURCE_ID_IMG_HEADER_ICON);
  s_header = layer_create(GRect(0, 0, b.size.w, HEADER_H));
  layer_set_update_proc(s_header, header_update);
  layer_add_child(root, s_header);
  tick_timer_service_subscribe(MINUTE_UNIT, header_tick);

  int top = HEADER_H;
  s_menu = menu_layer_create(GRect(0, top, b.size.w, b.size.h - top));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_num_rows,
    .get_cell_height = get_cell_height,
    .draw_row = draw_row,
    .select_click = select_click,
    .select_long_click = select_long_click,
  });
  menu_layer_set_normal_colors(s_menu, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu, highlight_bg(), row_fg(true));
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));

  window_set_touch_bridge_disabled(window, true);
}

static void window_unload(Window *window) {
  touch_nav_reset();
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  tick_timer_service_unsubscribe();
  layer_destroy(s_header);
  s_header = NULL;
  gbitmap_destroy(s_header_icon);
  s_header_icon = NULL;
}

static void window_appear(Window *window)    { touch_nav_set_settled_handler(touch_settled); }
static void window_disappear(Window *window) { touch_nav_reset(); }

Window *monitor_list_window(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, GColorWhite);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = window_load,
      .unload = window_unload,
      .appear = window_appear,
      .disappear = window_disappear,
    });
  }
  return s_window;
}

void monitor_list_reload(void) {
  if (s_menu) {
    menu_layer_set_highlight_colors(s_menu, highlight_bg(), row_fg(true));
    menu_layer_reload_data(s_menu);
  }
}

void monitor_list_destroy(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}
