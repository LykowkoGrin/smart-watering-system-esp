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
  void processMaxFlowTopic(byte* payload, unsigned int length);
  void processIgnoreCountTopic(byte* payload, unsigned int length);
  void processUnblockFlow(byte* payload, unsigned int length);

  bool convertTimeToSec(const String& dateTime, uint32_t& seconds);

  void updateLastUpdateTime();

  unsigned long lastRequestTime = 0;
  unsigned long lastUpdateTime = 0;
  const unsigned long minRequestDelay = 1500;
  uint32_t updateDelay;

  String userId,password;

  String tempThresholdTopic = "/temp_threshold",
  intervalTopic = "/interval",
  timerTopic = "/timer",
  delIntervalsTopic = "/del_intervals",
  maxFlowTopic="/max_flow",
  ignoreCountTopic="/ignore_count",
  unblockFlowTopic = "/unblock_flow";

  String statusTopic = "/status",
  logsTopic = "/logs",
  tempTopic = "/temp",
  flowTopic = "/flow",
  flowBlockTopic = "/flow_block";


  std::vector<IntervalTime>* intervals;
  uint32_t* stopTimerSec;
  float* temperatureThreshold;
  float* maxLitersPerMinute;
  uint8_t* ignoreAfterTurningOn;
  bool* flowExceededMaxValue;
  RtcDateTime* lastDataUpdate;

  const bool* relayStatus;
  const float* lastLitersPerMinute;
  SemaphoreHandle_t mutex;

  RtcDS1302<ThreeWire>* rtc;
  SemaphoreHandle_t rtcMutex;

  Adafruit_BMP280* bmp;
  SemaphoreHandle_t bmpMutex;

  PubSubClient* mqtt;

  static ClusterflyManager* instance;
};