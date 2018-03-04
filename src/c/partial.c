#include <pebble.h>
#include <stdio.h>
#include "math.h"

static Window *s_window;
static Layer *bitmap_layer;
static GColor background_color;
static GColor line_color;
static GPath *fillingPath = NULL;

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static float calcuate_height_ratio(float ratio) {
  return ratio;
}

static void bitmap_layer_update_proc(Layer *layer, GContext* ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t minute = 20;//; (*t).tm_min;
  int32_t hour = (*t).tm_hour;

  int32_t hour_angle = TRIG_MAX_ANGLE * (hour % 12) / 12.0;
  float minute_ratio = (minute / 60.0);

  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  int diameter = MIN(bounds.size.w, bounds.size.h);

  background_color = GColorFromHEX(0xff0000);
  line_color = GColorFromHEX(0x00ff00);

  // background color
  graphics_context_set_fill_color(ctx, background_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, line_color);
  graphics_context_set_antialiased(ctx, true);

  GPoint diameterPoint1 = GPoint(
    center.x + sin_lookup(hour_angle) * (bounds.size.w / 2) / TRIG_MAX_RATIO,
    center.y - cos_lookup(hour_angle) * (bounds.size.h / 2) / TRIG_MAX_RATIO
  );
  GPoint diameterPoint2 = GPoint(
    center.x - sin_lookup(hour_angle) * (bounds.size.w / 2) / TRIG_MAX_RATIO,
    center.y + cos_lookup(hour_angle) * (bounds.size.h / 2) / TRIG_MAX_RATIO
  );

  graphics_draw_line(ctx, diameterPoint1, diameterPoint2);

  float height_ratio = calcuate_height_ratio(minute_ratio); // perpendicular to the hour "diameter"
  int length = diameter / 2.0 * height_ratio;

  graphics_context_set_fill_color(ctx, line_color);

  GPathInfo pathInfo = {
    .num_points = 4,
    .points = (GPoint []) {
      diameterPoint1,
      GPoint(
        diameterPoint1.x + cos_lookup(hour_angle) * length / TRIG_MAX_RATIO,
        diameterPoint1.y + sin_lookup(hour_angle) * length / TRIG_MAX_RATIO
      ),
      GPoint(
        diameterPoint2.x + cos_lookup(hour_angle) * length / TRIG_MAX_RATIO,
        diameterPoint2.y + sin_lookup(hour_angle) * length / TRIG_MAX_RATIO
      ),
      diameterPoint2
    }
  };
  if (fillingPath != NULL) {
    gpath_destroy(fillingPath);
  }
  fillingPath = gpath_create(&pathInfo);
  gpath_draw_filled(ctx, fillingPath);
}

static void prv_window_load(Window *window) {
Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  bitmap_layer = layer_create(bounds);
  layer_set_update_proc(bitmap_layer, bitmap_layer_update_proc);
  layer_add_child(window_layer, bitmap_layer);
}

static void prv_window_unload(Window *window) {
  layer_destroy(bitmap_layer);
}

static void prv_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}
