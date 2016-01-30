#include <pebble.h>
#include "applite_utc.h"

// go here to confirm image https://www.timeanddate.com/worldclock/sunearth.html
// http://codecorner.galanter.net/2015/04/03/simplify-access-to-framebuffer-on-pebble-time/

#define STR_SIZE 20
#define REDRAW_INTERVAL 15

static Window *window;
static TextLayer *time_text_layer;
static TextLayer *zone1_text_layer;
static TextLayer *zone0_text_layer;
static TextLayer *date_text_layer;
static TextLayer *s_battery_layer;

static GBitmap *world_bitmap;
//static GBitmap *world_NIGHT_bitmap;
static Layer *canvas;
// this is a manually created bitmap, of the same size as world_bitmap
#ifdef PBL_PLATFORM_APLITE 
  static GBitmap image;
#endif
static int redraw_counter;
char *s;
int prev_month;

#ifdef PBL_PLATFORM_APLITE
  int time_offset;
#endif

#ifdef PBL_COLOR
  int monthMaps[] = {
    RESOURCE_ID_WORLD_01,
    RESOURCE_ID_WORLD_02,
    RESOURCE_ID_WORLD_03,
    RESOURCE_ID_WORLD_04,
    RESOURCE_ID_WORLD_05,
    RESOURCE_ID_WORLD_06,
    RESOURCE_ID_WORLD_07,
    RESOURCE_ID_WORLD_08,
    RESOURCE_ID_WORLD_09,
    RESOURCE_ID_WORLD_10,
    RESOURCE_ID_WORLD_11,
    RESOURCE_ID_WORLD_12};
#endif

// timezone offsets, seconds
static int offset0 = 0; // UTC
static int offset1 = 8 * 3600; // China

static void handle_battery(BatteryChargeState charge_state) {
  static char battery_text[] = "10+";

  if (charge_state.is_charging) {
    //snprintf(battery_text, sizeof(battery_text), "++");
    snprintf(battery_text, sizeof(battery_text), "%d+", charge_state.charge_percent);
  } else {
    snprintf(battery_text, sizeof(battery_text), "%d", charge_state.charge_percent);
  }
  memmove(battery_text, &battery_text[0], sizeof(battery_text) - 1);
  text_layer_set_text(s_battery_layer, battery_text);
}

static void draw_watch(struct Layer *layer, GContext *ctx) {

  graphics_draw_bitmap_in_rect(ctx, world_bitmap, GRect(0, 47, 144, 72));
  int now = (int)unixTime();

  float year_frac; // 0 == northern vernal equinox
  float day_frac;
  year_frac = ((now - 1426891493) % 31556941) / 31556941.; // 2015-03-20T22:44:53Z == @1426891493
  day_frac = (now % 86400) / 86400.;
  
  //TODO: Appease Kepler. Orbit is an elipse!
  int sun_x = (int)((float)TRIG_MAX_ANGLE * (1.0 - day_frac));
  // Earth's inclination is 23.4 degrees, so sun should vary 23.4/90=.26 up and down
  int sun_y = -sin_lookup(year_frac * TRIG_MAX_ANGLE) * 23.44/90 * 0.25;
  
  
  int x, y;

  #ifdef PBL_COLOR
    static GBitmap *bb;
    bb = gbitmap_create_with_resource(RESOURCE_ID_NIGHT_PBLv2);
  #endif

  #ifdef PBL_COLOR
    GBitmap *fb = graphics_capture_frame_buffer_format(ctx, GBitmapFormat8Bit);
  #else
    GBitmap *fb = graphics_capture_frame_buffer(ctx);
  #endif
    uint8_t *fb_data = gbitmap_get_data(fb);  

  #define WINDOW_WIDTH 144 
  #ifdef PBL_COLOR
    uint8_t *background_data = gbitmap_get_data(bb); 
    uint8_t (*background_matrix)[WINDOW_WIDTH] = (uint8_t (*)[WINDOW_WIDTH]) background_data;
  #endif
  uint8_t (*fb_matrix)[WINDOW_WIDTH] = (uint8_t (*)[WINDOW_WIDTH]) fb_data;

  for(x = 0; x < 144; x++) {
    int x_angle = (int)((float)TRIG_MAX_ANGLE * (float)x / (float)(144));
    for(y = 0; y < 72; y++) {
//       APP_LOG(APP_LOG_LEVEL_DEBUG, "y=%d", y);
      int y_angle = (int)((float)TRIG_MAX_ANGLE * (float)y / (float)(72 * 2)) - TRIG_MAX_ANGLE/4;
      // spherical law of cosines
      float angle = ((float)sin_lookup(sun_y)/(float)TRIG_MAX_RATIO) * ((float)sin_lookup(y_angle)/(float)TRIG_MAX_RATIO);
      angle = angle + ((float)cos_lookup(sun_y)/(float)TRIG_MAX_RATIO) * ((float)cos_lookup(y_angle)/(float)TRIG_MAX_RATIO) * ((float)cos_lookup(sun_x - x_angle)/(float)TRIG_MAX_RATIO);
      if (angle > 0) { //^ (0x1 & (((char *)world_bitmap->addr)[byte] >> (x % 8)))) {
        // white pixel
      } else {
        // black pixel
        #ifdef PBL_COLOR
          fb_matrix[y+47][x] = background_matrix[y][x];
        #else
          int byte = x/8;
          int bit = x%8;
          uint8_t *addr = fb_data + 20 * (y+47) + byte;
          *addr = *addr^(1 << bit);
        #endif
      }
    }
  }
  graphics_release_frame_buffer(ctx,fb);
  #ifdef PBL_COLOR
    gbitmap_destroy(bb);
  #endif
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  handle_battery(battery_state_service_peek());
  //static char time_text[8] = "00:00 am";
  static char time_text[] = "00:00";
  static char zone_text_0[] = "00:00  ";
  static char zone_text_1[] = "00:00  ";
  static char date_text[] = "Xxx 00 Xxx";
  struct tm * zonetime_0;
  struct tm * zonetime_1;
  
  time_t now = unixTime();

  strftime(date_text, sizeof(date_text), "%a %e %b", tick_time);
  text_layer_set_text(date_text_layer, date_text);

  if (clock_is_24h_style()) {
    strftime(time_text, sizeof(time_text), "%R", tick_time);
  }
  else{
    strftime(time_text, sizeof(time_text), "%I:%M", tick_time);
  }

  text_layer_set_text(time_text_layer, time_text);

  //TODO: Configure TZs, DST from internet
//   zonetime_0 = tick_time;
//   zonetime_0->tm_hour += 6;
//   if (zonetime_0->tm_hour > 23) {
//     zonetime_0->tm_hour -= 24;
//   }  
  
  zonetime_0 = utctime(now + offset0);

  if (clock_is_24h_style()) {
    strftime(zone_text_0, sizeof(zone_text_0), "%R",  zonetime_0);
  }
  else{
    strftime(zone_text_0, sizeof(zone_text_0), "%I:%M%P", zonetime_0);
  }
  text_layer_set_text(zone0_text_layer, zone_text_0);

  zonetime_1 = utctime(now + offset1);
  
  if (clock_is_24h_style()) {
    strftime(zone_text_1, sizeof(zone_text_1), "%R", zonetime_1);
  }
  else{
    strftime(zone_text_1, sizeof(zone_text_1), "%I:%M%P", zonetime_1);
  }
  text_layer_set_text(zone1_text_layer, zone_text_1);
  
  #ifdef PBL_COLOR
  if(tick_time->tm_mon != prev_month){
    prev_month = tick_time->tm_mon;
    gbitmap_destroy(world_bitmap);
    world_bitmap = gbitmap_create_with_resource(monthMaps[tick_time->tm_mon]);
  }
  #endif

  redraw_counter++;
  if (redraw_counter >= REDRAW_INTERVAL) {
    //    draw_earth();
    redraw_counter = 0;
  }
}

// Get the time from the phone, which is probably UTC
// Calculate and store the offset when compared to the local clock
static void app_message_inbox_received(DictionaryIterator *iterator, void *context) {
  Tuple *t = dict_find(iterator, 0);
  time_t unixtime = t->value->int32;
  set_offset(unixtime);
}

static void window_load(Window *window) {
  #ifdef PBL_COLOR
    window_set_background_color(window, GColorBlack);
  #else
    window_set_background_color(window, GColorBlack );
  #endif
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  //Local
  time_text_layer = text_layer_create(GRect(0, 2, 144-0, 168-0));
  text_layer_set_background_color(time_text_layer, GColorClear );
  text_layer_set_text_color(time_text_layer, GColorWhite);
  
  #ifdef PBL_PLATFORM_APLITE
    text_layer_set_font(time_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
  #else
    text_layer_set_font(time_text_layer, fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS));
  #endif  
    
  text_layer_set_text(time_text_layer, "");
  text_layer_set_text_alignment(time_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(time_text_layer));

  //Date
  date_text_layer = text_layer_create(GRect(0, 120, 144-0, 168-120));
  text_layer_set_background_color(date_text_layer, GColorClear ); 
  text_layer_set_text_color(date_text_layer, GColorWhite);
  text_layer_set_font(date_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text(date_text_layer, "");
  text_layer_set_text_alignment(date_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(date_text_layer));

  //zone 0
  zone0_text_layer = text_layer_create(GRect(0, 140, 144-72, 168-140));
  text_layer_set_background_color(zone0_text_layer, GColorClear );
  text_layer_set_text_color(zone0_text_layer, GColorWhite);
  text_layer_set_font(zone0_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text(zone0_text_layer, "");
  text_layer_set_text_alignment(zone0_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(zone0_text_layer));  

  //zone 1
  zone1_text_layer = text_layer_create(GRect(72, 140, 144-72, 168-140));
  text_layer_set_background_color(zone1_text_layer, GColorClear );
  text_layer_set_text_color(zone1_text_layer, GColorWhite);
  text_layer_set_font(zone1_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text(zone1_text_layer, "");
  text_layer_set_text_alignment(zone1_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(zone1_text_layer));

  //BATTERY TEXT
  s_battery_layer = text_layer_create(GRect(110, -3, 32, 40));
  text_layer_set_text_color(s_battery_layer, GColorWhite);
  text_layer_set_background_color(s_battery_layer, GColorClear);
  text_layer_set_font(s_battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_battery_layer, GTextAlignmentRight);
  text_layer_set_text(s_battery_layer, "  -");
  layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));


  //BOTH APPLITE AND BASALT
  canvas = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  layer_set_update_proc(canvas, draw_watch);
  layer_add_child(window_layer, canvas);
  //APPLITE ONLY 
  #ifdef PBL_PLATFORM_APLITE 
    image = *world_bitmap;
    image.addr = malloc(image.row_size_bytes * image.bounds.size.h);
    //draw_earth();// 20150607 does this fix appligth?
  #else
    //imageB = gbitmap_create_with_resource(RESOURCE_ID_WORLD);
  #endif     
    //  draw_earth(); ///20150607 does this fix appligth?

}

static void window_unload(Window *window) {
  text_layer_destroy(time_text_layer);
  text_layer_destroy(zone1_text_layer);
  text_layer_destroy(zone0_text_layer);
  text_layer_destroy(date_text_layer);
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  layer_destroy(canvas);
  #ifdef PBL_PLATFORM_APLITE
    free(image.addr);
  #endif
}

static void init(void) {
  redraw_counter = 20; //PRESET READY TO UPDATE ON FIRST RUN -- MAY NEED TO FIX FOR APPLITE TO NOT LOAD TWICE

  init_offset();

  #ifdef PBL_PLATFORM_APLITE 
    world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_MAP_BW);
  #else 
    world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_DAYMAP_COLOR);
    prev_month = -1;
    //world_NIGHT_bitmap = gbitmap_create_with_resource(RESOURCE_ID_NIGHT_PBL); // DO THIS ABOVE IN OTHER ROUTINE
  #endif

  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  const bool animated = true;
  window_stack_push(window, animated);

  s = malloc(STR_SIZE);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  battery_state_service_subscribe(handle_battery);

  app_message_register_inbox_received(app_message_inbox_received);
  app_message_open(30, 0);
}

static void deinit(void) {
  //tick_timer_service_unsubscribe();
  free(s);
  window_destroy(window);
  gbitmap_destroy(world_bitmap);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}

