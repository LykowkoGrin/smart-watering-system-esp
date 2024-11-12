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

  enum processStage{
    none,
    timerInput,
    intervalInput,
    temperatureInput,
    delIntervalInput,
    chooseInput
  };
  processStage stage = processStage::none;

  uint32_t requestDelay;
  uint32_t inputRequestDelay;
  unsigned long lastTimeBotRan;

  int messageCount;

  UniversalTelegramBot* bot;
  RtcDS1302<ThreeWire>* rtc;
  Adafruit_BMP280* bmp;
  const bool* relayStatus;

  ChangePtrs changePtrs;
};