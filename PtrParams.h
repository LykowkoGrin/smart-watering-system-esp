#pragma once

#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <RtcDS1302.h>
#include <ThreeWire.h>
#include <Adafruit_BMP280.h>
#include "IntervalTime.h"

struct ConstructPtrs{
  UniversalTelegramBot* bot;
  RtcDS1302<ThreeWire>* rtc; 
  Adafruit_BMP280* bmp;
  WiFiClientSecure* client;
  const bool* relayStatus;
};
struct ChangePtrs{
  std::vector<IntervalTime>* intervals;
  uint32_t* stopTimerSec;
  float* temperatureThreshold;
};