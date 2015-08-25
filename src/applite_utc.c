#include <pebble.h>
#include "applite_utc.h"

  
// wrapper for time to return correct time since epoch
// on aplite, time() returns incorrect value, off by current UTC offset
time_t unixTime(){
  #ifdef PBL_PLATFORM_APLITE
    return time(NULL) + time_offset;
  #else
    return time(NULL);
  #endif
}

void set_offset(time_t time){
  #ifdef PBL_PLATFORM_APLITE
    time_offset = time - time(NULL);
    status_t s = persist_write_int(TIME_OFFSET_PERSIST, time_offset); 
    if (s) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Saved time offset %d with status %d", time_offset, (int) s);
    } else {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Failed to save time offset with status %d", (int) s);
    }
  #endif
}

void init_offset(){
  #ifdef PBL_PLATFORM_APLITE
  // Load the UTC offset, if it exists
  time_offset = 0;
  if (persist_exists(TIME_OFFSET_PERSIST)) {
    time_offset = persist_read_int(TIME_OFFSET_PERSIST);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "loaded offset %d", time_offset);
  }
  #endif
}

struct tm * utctime(const time_t timep){
  #ifdef PBL_PLATFORM_APLITE
    return localtime(&timep);
  #else
  #ifdef PBL_PLATFORM_BASALT
    return gmtime(&timep);
  #endif
  #endif
  
}