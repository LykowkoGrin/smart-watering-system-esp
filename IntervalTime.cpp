#include "IntervalTime.h"
#include <string>

bool IntervalTime::inInterval(const RtcDateTime& dt){
  uint32_t dtInSec = dt.Hour() * 3600 + dt.Minute() * 60;
  if(start < stop)
    return start <= dtInSec && dtInSec <= stop;
  if(start > stop){
    if(start <= dtInSec) return true;
    if(stop >= dtInSec) return true;
  }
}

String IntervalTime::toString(){
  RtcDateTime startDt= RtcDateTime(start);
  RtcDateTime stopDt= RtcDateTime(stop);
  String startStr = String(startDt.Hour()) + ":" + String(startDt.Minute());
  String stopStr = String(stopDt.Hour()) + ":" + String(stopDt.Minute());
  return startStr + "-" + stopStr;
}

bool IntervalTime::parseTime(const String& timeStr, uint8_t& hour, uint8_t& minute){
  int colonIndex = timeStr.indexOf(':');
  
  if (colonIndex == -1) {
      return false;
  }

  String hourStr = timeStr.substring(0, colonIndex);
  String minuteStr = timeStr.substring(colonIndex + 1);

  int hourI,minI;
  try{hourI = std::stoi(hourStr.c_str());}
  catch(std::exception ex){ return false;}

  try{minI = std::stoi(minuteStr.c_str());}
  catch(std::exception ex){ return false;}

  hour = (uint8_t)hourI;
  minute = (uint8_t)minI;

  return true;
}