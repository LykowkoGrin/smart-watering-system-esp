#pragma once
#include "IntervalTime.h"
#include <WebServer.h>
#include <ESPmDNS.h>
#include "PtrParams.h"

class LocalManager{
public:
  LocalManager(const ConstructPtrs& params);

  void raiseServer(const String& serverName);
  void tickServer(const ChangePtrs& params);
private:
  std::vector<IntervalTime>* intervals;
  uint32_t* stopTimerSec;
  float* temperatureThreshold;
  const bool* relayStatus;
  RtcDS1302<ThreeWire>* rtc;
  Adafruit_BMP280* bmp;

  WebServer* server;

  void handleNewClient();
  void handleSubmit();
  void handleAddItem();
  void handleDeleteItem();
};




























