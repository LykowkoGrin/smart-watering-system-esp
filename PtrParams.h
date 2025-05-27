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
  SemaphoreHandle_t rtcMutex;

  Adafruit_BMP280* bmp;
  SemaphoreHandle_t bmpMutex;

  WiFiClientSecure* client;
  const bool* relayStatus;
  const float* lastLitersPerMinute;


};
struct ChangePtrs{
  std::vector<IntervalTime>* intervals;
  uint32_t* stopTimerSec;
  float* temperatureThreshold;
  float* maxLitersPerMinute;
  uint8_t* ignoreAfterTurningOn;
  bool* flowExceededMaxValue;

  SemaphoreHandle_t mutex;
};