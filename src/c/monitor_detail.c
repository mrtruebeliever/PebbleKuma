#include "monitor_detail.h"
#include "data.h"
#include "config.h"
#include "i18n.h"

// Card layers (cards-example pattern): a short fixed status band holding the
// status icon, and a sliding content area. On paging, the content text and the
// band icon slide out, the data swaps, and they slide back in.
#define BAND_H      34
#define SCROLL_OUT  22
#define SCROLL_IN   10
#define SCROLL_MS  150

static Window *s_window = NULL;
static Layer  *s_band_layer = NULL;
static Layer  *s_icon_layer = NULL;
static Layer  *s_content_layer = NULL;
static int     s_pos = 0;
static bool    s_animating = false;

// Animated uptime number (cards-example technique): the displayed % counts from
// s_num_from to s_num_to over the animation; the cloud icon itself never animates.
static int        s_disp_uptime = 0;   // currently displayed uptime, x100
static int        s_num_from = 0;
static int        s_num_to = 0;
static Animation *s_num_anim = NULL;

// Heartbeat bars sweep in left-to-right; s_hb_fill is the 0..ANIMATION_NORMALIZED_MAX
// progress of that fill (left at max when no animation is running, so bars show full).
static int        s_hb_fill = ANIMATION_NORMALIZED_MAX;
static Animation *s_hb_anim = NULL;

static GDrawCommandImage *s_img_up, *s_img_down, *s_img_pending, *s_img_maint, *s_img_cloud;
static GBitmap *s_hr;   // HR pulse icon shown before the heartbeat bars

// Picks a legible text color for content drawn on top of `bg`.
static GColor text_on(GColor bg) {
  if (gcolor_equal(bg, GColorRed) || gcolor_equal(bg, GColorVividCerulean)) {
    return GColorWhite;
  }
  return GColorBlack;
}

// --- PDC recolor -------------------------------------------------------------

typedef struct { GColor stroke; GColor fill; } Recolor;

static bool recolor_cb(GDrawCommand *command, uint32_t index, void *context) {
  Recolor *r = (Recolor *)context;
  gdraw_command_set_stroke_color(command, r->stroke);
  gdraw_command_set_fill_color(command, r->fill);
  return true;
}

static void recolor(GDrawCommandImage *img, GColor stroke, GColor fill) {
  if (!img) { return; }
  Recolor r = { .stroke = stroke, .fill = fill };
  gdraw_command_list_iterate(gdraw_command_image_get_command_list(img), recolor_cb, &r);
}

// --- Animated uptime number --------------------------------------------------

static void num_anim_update(Animation *anim, AnimationProgress progress) {
  s_disp_uptime = s_num_from +
      (int)(((int64_t)progress * (s_num_to - s_num_from)) / ANIMATION_NORMALIZED_MAX);
  if (s_content_layer) { layer_mark_dirty(s_content_layer); }
}

static const AnimationImplementation s_num_impl = {
  .update = num_anim_update,
};

// Animates the displayed uptime % from its current value to `to` (x100).
static void start_uptime_anim(int to) {
  if (s_num_anim) {
    animation_unschedule(s_num_anim);
    animation_destroy(s_num_anim);
    s_num_anim = NULL;
  }
  if (to == UPTIME_UNKNOWN) {           // nothing to count to — show "--"
    s_disp_uptime = UPTIME_UNKNOWN;
    if (s_content_layer) { layer_mark_dirty(s_content_layer); }
    return;
  }
  if (s_disp_uptime == UPTIME_UNKNOWN) { s_disp_uptime = 0; }
  s_num_from = s_disp_uptime;
  s_num_to = to;
  s_num_anim = animation_create();
  animation_set_implementation(s_num_anim, &s_num_impl);
  animation_set_duration(s_num_anim, 350);
  animation_set_curve(s_num_anim, AnimationCurveEaseOut);
  animation_schedule(s_num_anim);
}

// Heartbeat fill sweep.
static void hb_anim_update(Animation *anim, AnimationProgress progress) {
  s_hb_fill = progress;
  if (s_content_layer) { layer_mark_dirty(s_content_layer); }
}

static const AnimationImplementation s_hb_impl = {
  .update = hb_anim_update,
};

static void start_hb_anim(void) {
  if (s_hb_anim) {
    animation_unschedule(s_hb_anim);
    animation_destroy(s_hb_anim);
    s_hb_anim = NULL;
  }
  s_hb_fill = 0;
  s_hb_anim = animation_create();
  animation_set_implementation(s_hb_anim, &s_hb_impl);
  animation_set_duration(s_hb_anim, 450);
  animation_set_curve(s_hb_anim, AnimationCurveEaseOut);
  animation_schedule(s_hb_anim);
}

static GDrawCommandImage *icon_for(uint8_t status) {
  switch (status) {
    case STATUS_UP:      return s_img_up;
    case STATUS_DOWN:    return s_img_down;
    case STATUS_PENDING: return s_img_pending;
    case STATUS_MAINT:   return s_img_maint;
    default:             return NULL;
  }
}

// --- Fixed status band -------------------------------------------------------

// Current card's pastel background (neutral when there's no monitor).
static GColor card_bg(void) {
  Monitor *m = data_monitor_at(s_pos);
  return m ? status_bg_color(m->status) : GColorLightGray;
}

static void band_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int w = b.size.w;
  Monitor *m = data_monitor_at(s_pos);
  GColor bg = card_bg();

  // Band matches the card's pastel; status is conveyed by the colored icon.
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  if (!m) { return; }

  graphics_context_set_text_color(ctx, text_on(bg));
  graphics_draw_text(ctx, status_label(m->status),
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(0, 2, w, 28), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);

  char count[24];
  snprintf(count, sizeof(count), "%d/%d", s_pos + 1, data_monitor_count());
  graphics_draw_text(ctx, count, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(w - 46, 1, 42, 16), GTextOverflowModeFill,
                     GTextAlignmentRight, NULL);
}

// Status icon (white line-art) drawn on top of the band, in its own layer so
// it can slide independently while paging.
static void icon_update(Layer *layer, GContext *ctx) {
  Monitor *m = data_monitor_at(s_pos);
  if (!m) { return; }
  GDrawCommandImage *img = icon_for(m->status);
  if (!img) { return; }
  recolor(img, status_color(m->status), GColorClear);  // the status pop of color
  GSize sz = gdraw_command_image_get_bounds_size(img);
  gdraw_command_image_draw(ctx, img, GPoint(0, (BAND_H - sz.h) / 2));
}

// --- Sliding content ---------------------------------------------------------

// Draws the cloud (dark-outlined for the accent card) with the animated uptime %
// inside it. The number comes from s_disp_uptime; the cloud icon never animates.
static void draw_uptime_cloud(GContext *ctx, GPoint origin) {
  if (!s_img_cloud) { return; }
  GColor ink = text_on(card_bg());   // dark, legible on the pastel card
  recolor(s_img_cloud, ink, GColorClear);
  gdraw_command_image_draw(ctx, s_img_cloud, origin);
  GSize sz = gdraw_command_image_get_bounds_size(s_img_cloud);

  char p[16];
  if (s_disp_uptime == UPTIME_UNKNOWN) {
    snprintf(p, sizeof(p), "--");
  } else if (s_disp_uptime >= 9995) {
    snprintf(p, sizeof(p), "100%%");
  } else {
    snprintf(p, sizeof(p), "%d.%d%%", s_disp_uptime / 100, (s_disp_uptime % 100) / 10);
  }
  graphics_context_set_text_color(ctx, ink);
  graphics_draw_text(ctx, p, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(origin.x, origin.y + sz.h / 2 - 11, sz.w, 22),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void content_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);  // bounds origin is animated while paging
  int w = b.size.w, h = b.size.h;
  GColor ink = text_on(card_bg());   // dark text on the pastel card
  Monitor *m = data_monitor_at(s_pos);
  if (!m) { return; }

  GFont f18 = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont f14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  char line[64];

  // Name (full width — the status icon now lives in the band).
  graphics_context_set_text_color(ctx, ink);
  graphics_draw_text(ctx, m->name, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(6, 4, w - 12, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Uptime cloud (left) + ping (right).
  GSize cloud = s_img_cloud ? gdraw_command_image_get_bounds_size(s_img_cloud) : GSize(64, 36);
  draw_uptime_cloud(ctx, GPoint(6, 40));

  if (m->ping == PING_UNKNOWN) {
    snprintf(line, sizeof(line), "Ping: --");
  } else if (m->pstats[0]) {
    snprintf(line, sizeof(line), "Ping %d ms\n%s", m->ping, m->pstats);
  } else {
    snprintf(line, sizeof(line), "Ping %d ms", m->ping);
  }
  int px = 6 + cloud.w + 10;
  graphics_context_set_text_color(ctx, GColorDarkGray);
  graphics_draw_text(ctx, line, f18, GRect(px, 40, w - px - 6, 44),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  // Type + last-checked.
  if (m->type[0] && m->last_time[0]) {
    snprintf(line, sizeof(line), "%s · %s", m->type, m->last_time);
  } else if (m->type[0]) {
    snprintf(line, sizeof(line), "%s", m->type);
  } else if (m->last_time[0]) {
    snprintf(line, sizeof(line), i18n(STR_LAST_FMT), m->last_time);
  } else {
    line[0] = '\0';
  }
  if (line[0]) {
    graphics_draw_text(ctx, line, f14, GRect(6, 82, w - 12, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  // Last message (only when the monitor reports one).
  if (m->msg[0]) {
    graphics_context_set_text_color(ctx, status_color(m->status));
    graphics_draw_text(ctx, m->msg, f14, GRect(6, 102, w - 12, 32),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  // Heartbeat bar graph (oldest .. newest, left -> right), with the HR pulse icon
  // before it. The black backing panel makes every status color pop on the pastel card.
  const int hr_w = 25, hr_h = 24;
  GRect bars = GRect(6 + hr_w + 4, h - 30, w - 12 - hr_w - 4, 26);
  if (s_hr) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_hr,
        GRect(6, bars.origin.y - 2 + (30 - hr_h) / 2, hr_w, hr_h));
  }
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(bars.origin.x - 2, bars.origin.y - 2,
                                bars.size.w + 4, bars.size.h + 4), 3, GCornersAll);
  int slot = bars.size.w / MAX_HB;
  if (slot < 2) slot = 2;
  // Sweep front position in milli-bar units (0 .. hb_len*1000), driven by s_hb_fill.
  long front = (long)(((int64_t)s_hb_fill * m->hb_len * 1000) / ANIMATION_NORMALIZED_MAX);
  for (int i = 0; i < m->hb_len; i++) {
    long local = front - (long)i * 1000;   // how far this bar has filled, 0..1000
    if (local <= 0) { continue; }
    if (local > 1000) { local = 1000; }
    int bh = (int)((long)bars.size.h * local / 1000);
    if (bh < 1) { bh = 1; }
    graphics_context_set_fill_color(ctx, status_color(m->hb[i]));
    graphics_fill_rect(ctx, GRect(bars.origin.x + i * slot,
                                  bars.origin.y + bars.size.h - bh,  // grow from the bottom
                                  slot - 1, bh), 0, GCornerNone);
  }
  if (m->hb_len == 0) {
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, i18n(STR_NODATA), f14, bars,
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }
}

// --- Paging animation --------------------------------------------------------

static void swap_card(Animation *anim, bool finished, void *context) {
  s_pos += (int)(intptr_t)context;
  Monitor *m = data_monitor_at(s_pos);
  window_set_background_color(s_window, card_bg());   // recolor the card to the new status
  start_uptime_anim(m ? m->uptime_x100 : UPTIME_UNKNOWN);  // count the % to the new card
  start_hb_anim();                                    // re-sweep the heartbeats
  layer_mark_dirty(s_band_layer);
  layer_mark_dirty(s_icon_layer);
  layer_mark_dirty(s_content_layer);
}

static void page_done(Animation *anim, bool finished, void *context) {
  animation_destroy(anim);
  s_animating = false;
}

// Builds an out-then-in bounds-origin slide for `layer`.
static Animation *slide_seq(Layer *layer, int dir, bool with_swap) {
  GPoint out_to = GPoint(0, (dir > 0) ? -SCROLL_OUT : SCROLL_OUT);
  PropertyAnimation *po = property_animation_create_bounds_origin(layer, NULL, &out_to);
  Animation *out = property_animation_get_animation(po);
  animation_set_duration(out, SCROLL_MS);
  animation_set_curve(out, AnimationCurveLinear);
  if (with_swap) {
    animation_set_handlers(out, (AnimationHandlers){ .stopped = swap_card },
                           (void *)(intptr_t)dir);
  }
  GPoint in_from = GPoint(0, (dir > 0) ? -SCROLL_IN : SCROLL_IN);
  PropertyAnimation *pi = property_animation_create_bounds_origin(layer, &in_from, &GPointZero);
  Animation *in = property_animation_get_animation(pi);
  animation_set_duration(in, SCROLL_MS);
  animation_set_curve(in, AnimationCurveEaseOut);
  return animation_sequence_create(out, in, NULL);
}

static void page(int dir) {
  if (s_animating) { return; }
  if (dir > 0 && s_pos + 1 >= data_monitor_count()) { return; }
  if (dir < 0 && s_pos <= 0) { return; }
  s_animating = true;

  Animation *spawn = animation_spawn_create(slide_seq(s_content_layer, dir, true),
                                            slide_seq(s_icon_layer, dir, false), NULL);
  animation_set_handlers(spawn, (AnimationHandlers){ .stopped = page_done }, NULL);
  animation_schedule(spawn);
}

static void next_card(ClickRecognizerRef rec, void *context) { page(+1); }
static void prev_card(ClickRecognizerRef rec, void *context) { page(-1); }

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prev_card);
  window_single_click_subscribe(BUTTON_ID_DOWN, next_card);
}

// --- Window ------------------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_img_up = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_UP);
  s_img_down = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_DOWN);
  s_img_pending = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_PENDING);
  s_img_maint = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_MAINT);
  s_img_cloud = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_CLOUD);
  s_hr = gbitmap_create_with_resource(RESOURCE_ID_IMG_HR_PULSE);

  s_band_layer = layer_create(GRect(0, 0, b.size.w, BAND_H));
  layer_set_update_proc(s_band_layer, band_update);
  layer_add_child(root, s_band_layer);

  s_icon_layer = layer_create(GRect(6, 0, 30, BAND_H));
  layer_set_update_proc(s_icon_layer, icon_update);
  layer_add_child(root, s_icon_layer);

  s_content_layer = layer_create(GRect(0, BAND_H, b.size.w, b.size.h - BAND_H));
  layer_set_update_proc(s_content_layer, content_update);
  layer_add_child(root, s_content_layer);

  // Count the uptime % up from 0 and sweep the heartbeats in, on entry.
  Monitor *m = data_monitor_at(s_pos);
  start_uptime_anim(m ? m->uptime_x100 : UPTIME_UNKNOWN);
  start_hb_anim();
}

static void window_unload(Window *window) {
  if (s_num_anim) {
    animation_unschedule(s_num_anim);
    animation_destroy(s_num_anim);
    s_num_anim = NULL;
  }
  if (s_hb_anim) {
    animation_unschedule(s_hb_anim);
    animation_destroy(s_hb_anim);
    s_hb_anim = NULL;
  }
  layer_destroy(s_band_layer);
  layer_destroy(s_icon_layer);
  layer_destroy(s_content_layer);
  s_band_layer = s_icon_layer = s_content_layer = NULL;
  if (s_img_up) gdraw_command_image_destroy(s_img_up);
  if (s_img_down) gdraw_command_image_destroy(s_img_down);
  if (s_img_pending) gdraw_command_image_destroy(s_img_pending);
  if (s_img_maint) gdraw_command_image_destroy(s_img_maint);
  if (s_img_cloud) gdraw_command_image_destroy(s_img_cloud);
  s_img_up = s_img_down = s_img_pending = s_img_maint = s_img_cloud = NULL;
  if (s_hr) gbitmap_destroy(s_hr);
  s_hr = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void monitor_detail_push(int position) {
  s_pos = position;
  s_animating = false;
  s_disp_uptime = 0;   // window_load animates the count-up to the real value
  s_window = window_create();
  window_set_background_color(s_window, card_bg());
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

void monitor_detail_reload(void) {
  int count = data_monitor_count();
  if (s_pos >= count) { s_pos = count > 0 ? count - 1 : 0; }
  // Count the % to the refreshed value when it changed, and recolor for any
  // status change.
  Monitor *m = data_monitor_at(s_pos);
  int up = m ? m->uptime_x100 : UPTIME_UNKNOWN;
  if (up != s_disp_uptime) { start_uptime_anim(up); }
  if (s_window) window_set_background_color(s_window, card_bg());
  if (s_band_layer) layer_mark_dirty(s_band_layer);
  if (s_icon_layer) layer_mark_dirty(s_icon_layer);
  if (s_content_layer) layer_mark_dirty(s_content_layer);
}

bool monitor_detail_is_shown(void) {
  return s_window != NULL && window_stack_get_top_window() == s_window;
}
