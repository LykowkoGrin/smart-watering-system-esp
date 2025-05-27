#pragma once
#include "IntervalTime.h"
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include "PtrParams.h"

class LocalManager{
public:
  LocalManager(const ConstructPtrs& params);

  void raiseServer(const String& serverName);
  //void tickServer(const ChangePtrs& params);
  void setChangePtrs(const ChangePtrs& params);
private:
  std::vector<IntervalTime>* intervals;
  uint32_t* stopTimerSec;
  float* temperatureThreshold;
  const bool* relayStatus;
  SemaphoreHandle_t mutex;

  RtcDS1302<ThreeWire>* rtc;
  SemaphoreHandle_t rtcMutex;

  Adafruit_BMP280* bmp;
  SemaphoreHandle_t bmpMutex;

  AsyncWebServer* server;

  void handleNewClient(AsyncWebServerRequest *request);
  void handleSubmit(AsyncWebServerRequest *request);
  void handleAddItem(AsyncWebServerRequest *request);
  void handleDeleteItem(AsyncWebServerRequest *request);
};




























