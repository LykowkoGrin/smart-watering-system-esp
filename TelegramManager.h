#pragma once
#include "IntervalTime.h"
#include "PtrParams.h"

class TelegramManager{
public:
  TelegramManager(const ConstructPtrs& params,const uint32_t& requestDelay,const uint32_t& inputRequestDelay);
  void tickBot(const ChangePtrs& params);
private:
  void handleNewMessage();

  void processFirstMessage();
  void processChoose      ();
  void processTimer       ();
  void processInterval    ();
  void processDelete      ();
  void processTemperature ();
  void processIgnoreCount (); //
  void processMaxFlow     (); //

  enum processStage{
    none,
    timerInput,
    intervalInput,
    temperatureInput,
    delIntervalInput,
    chooseInput,

    maxFlowInput,
    ignoreCountInput
  };
  processStage stage = processStage::none;

  uint32_t requestDelay;
  uint32_t inputRequestDelay;
  unsigned long lastTimeBotRan;
  SemaphoreHandle_t mutex;

  int messageCount;

  UniversalTelegramBot* bot;

  RtcDS1302<ThreeWire>* rtc;
  SemaphoreHandle_t rtcMutex;

  Adafruit_BMP280* bmp;
  SemaphoreHandle_t bmpMutex;

  const bool* relayStatus;
  const float* lastLitersPerMinute;

  ChangePtrs changePtrs;
};