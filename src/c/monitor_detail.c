#include "monitor_detail.h"
#include <string.h>
#include "touch_nav.h"
#include "data.h"
#include "config.h"
#include "i18n.h"

// Card layers (cards-example pattern): a short fixed status band holding the
// status icon, and a sliding content area. On paging, the content text and the
// band icon slide out, the data swaps, and they slide back in.
// Card geometry per screen. The card is drawn for the 200px rectangle; on a
// round screen it sits in a 200px-wide frame pulled down off the top bezel
// (CARD_TOP), and the 144px screen gets smaller fonts and tighter rows.
#if PBL_DISPLAY_WIDTH < 180
#define COMPACT 1
#define BAND_H      28
#define LABEL_FONT  FONT_KEY_GOTHIC_18_BOLD
#define NAME_FONT   FONT_KEY_GOTHIC_18_BOLD
#define NAME_H      22
#define INFO_FONT   FONT_KEY_GOTHIC_14       // ping, type, message
#define INFO_LINE   16
#define CLOUD_Y     24
#define TYPE_Y      62
#define MSG_Y       78
#define MSG_H       34
#define BARS_H      20
#define MARGIN      4
#else
#define COMPACT 0
#define BAND_H      34
#define LABEL_FONT  FONT_KEY_GOTHIC_24_BOLD
#define NAME_FONT   FONT_KEY_GOTHIC_24_BOLD
#define NAME_H      30
#define INFO_FONT   FONT_KEY_GOTHIC_18
#define INFO_LINE   22
#define CLOUD_Y     40
// Round: one step up, so a two-line message clears the heartbeat strip,
// which sits higher where the circle narrows.
#define TYPE_Y      PBL_IF_ROUND_ELSE(86, 94)
#define MSG_Y       PBL_IF_ROUND_ELSE(106, 118)
#define MSG_H       40
#define BARS_H      26
#define MARGIN      6
#endif

#if defined(PBL_ROUND)
#define CARD_TOP    30
#define CARD_BOTTOM 14
// The bottom of the circle is narrow: the heartbeat strip (pulse icon + bars)
// keeps to this width, centred.
#define BARS_STRIP_W 130
#endif
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

// Current card's pastel background (neutral when there's no monitor). On
// black-and-white the pastels would all dither, so the card stays white and the
// status is carried by the icon and the label.
static GColor card_bg(void) {
#if defined(PBL_BW)
  return GColorWhite;
#else
  Monitor *m = data_monitor_at(s_pos);
  return m ? status_bg_color(m->status) : GColorLightGray;
#endif
}

// A heartbeat bar on its black backing: the status colour, or on
// black-and-white solid white for up and a dither for anything else.
static GColor beat_color(uint8_t status) {
  return PBL_IF_COLOR_ELSE(status_color(status),
                           status == STATUS_UP ? GColorWhite : GColorLightGray);
}

// The status icon's colour: its status colour, or the card's ink.
static GColor icon_color(uint8_t status) {
  return PBL_IF_COLOR_ELSE(status_color(status), text_on(card_bg()));
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
                     fonts_get_system_font(LABEL_FONT),
                     GRect(0, 2, w, 28), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);

#if defined(PBL_ROUND)
  return;   // no corner for the count up here; it joins the type line instead
#endif
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
  recolor(img, icon_color(m->status), GColorClear);  // the status pop of color
  GSize sz = gdraw_command_image_get_bounds_size(img);
  int16_t x = 0;
#if defined(PBL_ROUND)
  // The band's left end is cut off by the bezel: the icon sits just left of the
  // centred label instead. The layer spans the band for this.
  GRect b = layer_get_bounds(layer);
  int16_t label_w = graphics_text_layout_get_content_size(
      status_label(m->status), fonts_get_system_font(LABEL_FONT), GRect(0, 0, b.size.w, 28),
      GTextOverflowModeFill, GTextAlignmentLeft).w;
  x = b.size.w / 2 - label_w / 2 - sz.w - 4;
#endif
  gdraw_command_image_draw(ctx, img, GPoint(x, (BAND_H - sz.h) / 2));
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
  graphics_draw_text(ctx, p, fonts_get_system_font(COMPACT ? FONT_KEY_GOTHIC_14_BOLD
                                                           : FONT_KEY_GOTHIC_18_BOLD),
                     GRect(origin.x, origin.y + sz.h / 2 - 11, sz.w, 22),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void content_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);  // bounds origin is animated while paging
  int w = b.size.w, h = b.size.h;
  GColor ink = text_on(card_bg());   // dark text on the pastel card
  Monitor *m = data_monitor_at(s_pos);
  if (!m) { return; }

  GFont f18b = fonts_get_system_font(COMPACT ? FONT_KEY_GOTHIC_14_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  GFont f18 = fonts_get_system_font(INFO_FONT);
  char line[64];

  // Name (full width — the status icon now lives in the band).
  graphics_context_set_text_color(ctx, ink);
  graphics_draw_text(ctx, m->name, fonts_get_system_font(NAME_FONT),
                     GRect(MARGIN, COMPACT ? 0 : 4, w - 2 * MARGIN, NAME_H),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Uptime cloud (left) + ping (right).
  GSize cloud = s_img_cloud ? gdraw_command_image_get_bounds_size(s_img_cloud) : GSize(64, 36);
  draw_uptime_cloud(ctx, GPoint(MARGIN, CLOUD_Y));

  if (m->ping == PING_UNKNOWN) {
    snprintf(line, sizeof(line), "Ping: --");
  } else if (m->pstats[0]) {
    snprintf(line, sizeof(line), "Ping %d ms\n%s", m->ping, m->pstats);
  } else {
    snprintf(line, sizeof(line), "Ping %d ms", m->ping);
  }
  int px = MARGIN + cloud.w + (COMPACT ? 6 : 10);
  graphics_context_set_text_color(ctx, ink);
  graphics_draw_text(ctx, line, f18b, GRect(px, CLOUD_Y, w - px - MARGIN, 2 * INFO_LINE + 4),
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
#if defined(PBL_ROUND)
  // The band had no room for the card count; it rides along here.
  size_t used = strlen(line);
  snprintf(line + used, sizeof(line) - used, "%s%d/%d", used ? " · " : "",
           s_pos + 1, data_monitor_count());
#endif
  if (line[0]) {
    graphics_draw_text(ctx, line, f18, GRect(MARGIN, TYPE_Y, w - 2 * MARGIN, INFO_LINE),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  // Last message (only when the monitor reports one).
  if (m->msg[0]) {
    // Black, not status_color(): the card ground is that same status's pastel,
    // so red-on-melon measured 1.3:1 and cobalt-on-celeste worse -- and this
    // line is the reason a monitor is down, the most important string here.
    graphics_context_set_text_color(ctx, GColorBlack);
    // On a round screen the circle closes in by this row: keep off the edge.
    const int16_t mx = MARGIN + PBL_IF_ROUND_ELSE(8, 0);
    graphics_draw_text(ctx, m->msg, f18, GRect(mx, MSG_Y, w - 2 * mx, MSG_H),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  // Heartbeat bar graph (oldest .. newest, left -> right), with the HR pulse icon
  // before it. The black backing panel makes every status color pop on the pastel card.
  const int hr_w = 25, hr_h = 24;
#if defined(PBL_ROUND)
  const int strip_x = (w - BARS_STRIP_W) / 2, strip_w = BARS_STRIP_W;
#else
  const int strip_x = MARGIN, strip_w = w - 2 * MARGIN;
#endif
  GRect bars = GRect(strip_x + hr_w + 4, h - BARS_H - 4, strip_w - hr_w - 4, BARS_H);
  if (s_hr) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_hr,
        GRect(strip_x, bars.origin.y - 2 + (BARS_H + 4 - hr_h) / 2, hr_w, hr_h));
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
    graphics_context_set_fill_color(ctx, beat_color(m->hb[i]));
    graphics_fill_rect(ctx, GRect(bars.origin.x + i * slot,
                                  bars.origin.y + bars.size.h - bh,  // grow from the bottom
                                  slot - 1, bh), 0, GCornerNone);
  }
  if (m->hb_len == 0) {
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, i18n(STR_NODATA), f18, bars,
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

// --- Touch -------------------------------------------------------------------
//
// The card fills the screen and never scrolls, so the flicks page between
// monitors exactly as the buttons do, and a rightward swipe goes back.

bool monitor_detail_is_top(void) {
  return s_window && window_stack_get_top_window() == s_window;
}

void monitor_detail_handle_touch(const TouchEvent *event) {
  switch (touch_nav_feed(NULL, event, NULL)) {
    case TOUCH_NAV_SWIPE_UP:   next_card(NULL, NULL); break;
    case TOUCH_NAV_SWIPE_DOWN: prev_card(NULL, NULL); break;
    case TOUCH_NAV_BACK:       window_stack_pop(true); break;
    default: break;
  }
}

// --- Window ------------------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
#if defined(PBL_ROUND)
  b = GRect((b.size.w - 200) / 2, CARD_TOP, 200, b.size.h - CARD_TOP - CARD_BOTTOM);
#endif

  s_img_up = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_UP);
  s_img_down = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_DOWN);
  s_img_pending = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_PENDING);
  s_img_maint = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_MAINT);
  s_img_cloud = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_CLOUD);
  s_hr = gbitmap_create_with_resource(RESOURCE_ID_IMG_HR_PULSE);

  s_band_layer = layer_create(GRect(b.origin.x, b.origin.y, b.size.w, BAND_H));
  layer_set_update_proc(s_band_layer, band_update);
  layer_add_child(root, s_band_layer);

  s_icon_layer = layer_create(PBL_IF_ROUND_ELSE(GRect(b.origin.x, b.origin.y, b.size.w, BAND_H),
                                                GRect(MARGIN, 0, 30, BAND_H)));
  layer_set_update_proc(s_icon_layer, icon_update);
  layer_add_child(root, s_icon_layer);

  s_content_layer = layer_create(GRect(b.origin.x, b.origin.y + BAND_H, b.size.w,
                                       b.size.h - BAND_H));
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
}

void monitor_detail_push(int position) {
  s_pos = position;
  s_animating = false;
  s_disp_uptime = 0;   // window_load animates the count-up to the real value
  // Reuse a single detail window across pushes (matches monitor_list's pattern):
  // load/unload manage the per-push layers and resources, so the window itself is
  // created once and destroyed at app exit, never from within its own unload.
  if (!s_window) {
    s_window = window_create();
    window_set_click_config_provider(s_window, click_config);
    window_set_touch_bridge_disabled(s_window, true);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = window_load,
      .unload = window_unload,
    });
  }
  window_set_background_color(s_window, card_bg());
  window_stack_push(s_window, true);
}

void monitor_detail_destroy(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
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
