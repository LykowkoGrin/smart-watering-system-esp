#include "PtrParams.h"
#include <PubSubClient.h>

class ClusterflyManager{
public:
  ClusterflyManager(const ConstructPtrs& params,String userId, String password,const uint32_t& updateDelay = 25'000);
  void tickMqtt(const ChangePtrs& params);
private:
  static void mqttCallback(char* topic, byte* payload, unsigned int length);
  void handleMessage(char* topic, byte* payload, unsigned int length);
  void connectToMqtt();
  void updateData();

  void processTempTopic(byte* payload, unsigned int length);
  void processIntervalTopic(byte* payload, unsigned int length);
  void processTimerTopic(byte* payload, unsigned int length);
  void processDeleteTopic(byte* payload, unsigned int length);

  bool convertTimeToSec(const String& dateTime, uint32_t& seconds);

  unsigned long lastRequestTime = 0;
  unsigned long lastUpdateTime = 0;
  const unsigned long minRequestDelay = 1500;
  uint32_t updateDelay;

  String userId,password;

  String tempThresholdTopic = "/temp_threshold",intervalTopic = "/interval",timerTopic = "/timer",delIntervalsTopic = "/del_intervals";
  String statusTopic = "/status",logsTopic = "/logs",tempTopic = "/temp";

  std::vector<IntervalTime>* intervals;
  uint32_t* stopTimerSec;
  float* temperatureThreshold;

  const bool* relayStatus;
  RtcDS1302<ThreeWire>* rtc;
  Adafruit_BMP280* bmp;
  PubSubClient* mqtt;

  static ClusterflyManager* instance;
};