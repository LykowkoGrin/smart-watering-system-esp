#pragma once
#include <RtcDS1302.h>


struct IntervalTime {
  bool inInterval(const RtcDateTime& dt);
  String toString();
  static bool parseTime(const String& timeStr, uint8_t& hour, uint8_t& minute);
  uint32_t start;
  uint32_t stop;
};


