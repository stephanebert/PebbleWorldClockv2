#pragma once
  
#ifdef PBL_PLATFORM_APLITE
  int time_offset;
#endif
  
time_t unixTime();

void set_offset(time_t time);

void init_offset();

struct tm * utctime(const time_t timep);