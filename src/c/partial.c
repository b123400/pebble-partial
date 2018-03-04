#include <pebble.h>
#include <stdio.h>
#include "math.h"
#include "SmallMaths.h"

const float HALF_PI = 1.57079633;
#define SETTINGS_KEY 1

static Window *s_window;
static Layer *bitmap_layer;
static GColor background_color;
static GColor line_color;
static GPath *fillingPath = NULL;

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define ABS(X) (((X) >= 0) ? (X) : -(X))

typedef struct ClaySettings {
  GColor BackgroundColor;
  GColor LineColor;
} ClaySettings;

static ClaySettings settings;

static float calcuate_height_ratio(float ratio) {
  // here, given the input `ratio`, I have to calculate the height ratio to radius
  // Which means "Within the half circle, if a certain area is filled up, what is it's depth".
  // Integrate (0 -> k) (1-x^2) dx
  // Which means solving "area = ((1-x^2)^0.5 * x + asin(x))"
  // since it's so complicated to calculated it, I am just using brute force.
  // after all the radius of pebble round is just 90px, A maximum of 7 tries is enough to find the answer.
  float targetArea = HALF_PI * ratio;
  float currentMin = 0.f;
  float currentMax = 1.f;
  float lastValue = -1.f;
  while(true) {
    float currentValue = (currentMin + currentMax) / 2.0;
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "(%f - %f), currentValue: %f", currentMin, currentMax, currentValue);
    float area = sm_sqrt(1-sm_powint(currentValue,2)) * currentValue + sm_asin(currentValue);
    if (ABS(area - targetArea) < 0.0005) {
      lastValue = currentValue;
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "area close: %f, %f", area, targetArea);
      break;
    } else if (area > targetArea) {
      currentMax = currentValue;
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "down to: (%f - %f)", currentMin, currentMax);
    } else {
      currentMin = currentValue;
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "up to: (%f - %f)", currentMin, currentMax);
    }
    if (lastValue > 0 && ABS(lastValue - currentValue) < 1/90.0) {
      lastValue = currentValue;
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "seems ok? %f %f", lastValue, currentValue);
      break;
    }
    if (currentMin > currentMax) {
      // in case if this happens, floating point has probably fucked up so lets pretend it's accurate enough
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "fuck %f %f", currentMin, currentMax);
      lastValue = currentValue;
      break;
    }
    lastValue = currentValue;
  }
  return lastValue;
}

static void bitmap_layer_update_proc(Layer *layer, GContext* ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t minute = (*t).tm_min;
  int32_t hour = (*t).tm_hour;

  int32_t hour_angle = TRIG_MAX_ANGLE * (hour % 12) / 12.0;
  float minute_ratio = minute / 60.0;

  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  int diameter = MIN(bounds.size.w, bounds.size.h);

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

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {
  // Read color preferences
  Tuple *bg_color_t = dict_find(iter, MESSAGE_KEY_background_color);
  if(bg_color_t) {
    background_color = GColorFromHEX(bg_color_t->value->int32);
  }
  Tuple *line_color_t = dict_find(iter, MESSAGE_KEY_line_color);
  if(line_color_t) {
    line_color = GColorFromHEX(line_color_t->value->int32);
  }
  layer_mark_dirty(bitmap_layer);

  settings.BackgroundColor = background_color;
  settings.LineColor = line_color;
  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
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

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(bitmap_layer);
}

static void prv_init(void) {

  // default settings
  settings.BackgroundColor = GColorWhite;
  settings.LineColor = GColorRed;

  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
  // apply saved data
  background_color = settings.BackgroundColor;
  line_color = settings.LineColor;

  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(128, 128);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
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
