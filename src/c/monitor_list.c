#include "monitor_list.h"
#include "monitor_detail.h"
#include "data.h"
#include "config.h"

static Window         *s_window = NULL;
static MenuLayer      *s_menu = NULL;
static StatusBarLayer *s_status_bar = NULL;

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
  action_menu_level_add_action(sort, "Pagina-volgorde", sort_action_performed,
                               (void *)(uintptr_t)SORT_PAGE);
  action_menu_level_add_action(sort, "Naam", sort_action_performed,
                               (void *)(uintptr_t)SORT_NAME);
  action_menu_level_add_action(sort, "Status (problemen eerst)", sort_action_performed,
                               (void *)(uintptr_t)SORT_STATUS);
  action_menu_level_add_child(root, sort, "Sorteren");

  for (int i = 0; i < pages; i++) {
    action_menu_level_add_action(root, data_page_name(i),
                                 page_action_performed, (void *)(uintptr_t)i);
  }

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

static int16_t get_cell_height(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  return 44;
}

// Draws the page-selector row (row 0).
static void draw_page_row(GContext *ctx, const Layer *cell) {
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  GColor title_c = GColorBlack;   // accent is too light to read on the white list
  GColor sub_c   = hl ? GColorBlack : GColorDarkGray;

  const char *page = data_page_count() > 0 ? data_page_name(data_active_page()) : "-";
  char title[PAGE_NAME_LEN + 16];
  snprintf(title, sizeof(title), "Pagina: %s", page);

  graphics_context_set_text_color(ctx, title_c);
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(8, 0, b.size.w - 12, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  static const char *const kSortLabel[SORT_COUNT] = { "pagina-volgorde", "naam", "status" };
  char sub[40];
  snprintf(sub, sizeof(sub), "Menu · sortering: %s", kSortLabel[data_sort_by()]);
  graphics_context_set_text_color(ctx, sub_c);
  graphics_draw_text(ctx, sub,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(8, 26, b.size.w - 12, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

// Draws a status message row (loading / error / empty).
static void draw_status_row(GContext *ctx, const Layer *cell) {
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  const char *title, *sub;
  switch (data_load_state()) {
    case LOAD_LOADING:      title = "Laden…";          sub = "Even geduld";              break;
    case LOAD_ERROR:        title = "Geen verbinding"; sub = "Check VPN/netwerk · Select"; break;
    case LOAD_UNCONFIGURED: title = "Niet ingesteld";  sub = "Stel in via de telefoon";   break;
    default:                title = "Geen monitors";   sub = "";                          break;
  }
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(8, 2, b.size.w - 12, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, hl ? GColorBlack : GColorDarkGray);
  graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(8, 26, b.size.w - 12, 16),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

// Draws one monitor row (by display position): status dot + name + uptime.
static void draw_monitor_row(GContext *ctx, const Layer *cell, int pos) {
  Monitor *m = data_monitor_at(pos);
  if (!m) { return; }
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);

  // Status dot at the left.
  graphics_context_set_fill_color(ctx, status_color(m->status));
  graphics_fill_circle(ctx, GPoint(16, b.size.h / 2), 6);

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, m->name, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(30, 0, b.size.w - 36, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  char sub[40];
  if (m->uptime_x100 == UPTIME_UNKNOWN) {
    snprintf(sub, sizeof(sub), "%s", status_label(m->status));
  } else {
    snprintf(sub, sizeof(sub), "%s  ·  %d.%02d%%", status_label(m->status),
             m->uptime_x100 / 100, m->uptime_x100 % 100);
  }
  graphics_context_set_text_color(ctx, hl ? GColorBlack : GColorDarkGray);
  graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(30, 26, b.size.w - 36, 16),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
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
  } else if (data_load_state() == LOAD_ERROR) {
    data_set_load_state(LOAD_LOADING);
    config_request_refresh();
    monitor_list_reload();
  }
}

static void select_long_click(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  open_options_menu();
}

// --- Window ------------------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorWhite, GColorBlack);
  status_bar_layer_set_separator_mode(s_status_bar, StatusBarLayerSeparatorModeDotted);
  layer_add_child(root, status_bar_layer_get_layer(s_status_bar));

  int top = STATUS_BAR_LAYER_HEIGHT;
  s_menu = menu_layer_create(GRect(0, top, b.size.w, b.size.h - top));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_num_rows,
    .get_cell_height = get_cell_height,
    .draw_row = draw_row,
    .select_click = select_click,
    .select_long_click = select_long_click,
  });
  menu_layer_set_normal_colors(s_menu, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu, config_accent(), GColorBlack);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  status_bar_layer_destroy(s_status_bar);
  s_status_bar = NULL;
}

Window *monitor_list_window(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, GColorWhite);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = window_load,
      .unload = window_unload,
    });
  }
  return s_window;
}

void monitor_list_reload(void) {
  if (s_menu) {
    menu_layer_set_highlight_colors(s_menu, config_accent(), GColorBlack);
    menu_layer_reload_data(s_menu);
  }
}
