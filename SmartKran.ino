
/*
Cкетч компилируется примерно 5 минут: https://vkvideo.ru/video620293254_456239025?t=5m57s
*/


#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <EEPROM.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <PubSubClient.h> //mqtt

#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <vector>

#include "SettingsParser.h"
#include "IntervalTime.h"
#include "LocalManager.h"
#include "TelegramManager.h"
#include "ClusterflyManager.h"

const int parsePin = 19;
const int relayPin = 4;
const int flowPin = 2;

const int keyAddress = 0;
const int parserAddress = keyAddress + sizeof(uint8_t);
const int timerAddress = parserAddress + SettingsParser::getReservedSizeEEPROM();
const int temperatureAddress = timerAddress + sizeof(uint32_t);
const int maxFlowAddress = temperatureAddress + sizeof(float);
const int ignoreCountAddress = maxFlowAddress + sizeof(float);

const int intervalsAddress = ignoreCountAddress + sizeof(uint8_t);
const int eepromSize = intervalsAddress + 150;

const uint8_t eepromKey = 137;
const float pulsesPerLiter = 300.0f;

const int CLOCK_DAT = 23; // DATA (IO)
const int CLOCK_CLK = 18; // CLOCK
const int CLOCK_RST = 5;  // CE (RST)

bool relayStatus = false;
bool isParseSettingsMode = false;

String chatId;

WiFiClientSecure client;
WiFiClientSecure client2;
UniversalTelegramBot *bot;

Adafruit_BMP280 bmp;
SemaphoreHandle_t bmpMutex = xSemaphoreCreateMutex();

ThreeWire myWire(CLOCK_DAT, CLOCK_CLK, CLOCK_RST);
RtcDS1302<ThreeWire> Rtc(myWire);
SemaphoreHandle_t rtcMutex = xSemaphoreCreateMutex();


LocalManager* localManager = nullptr;
TelegramManager* telegramManager = nullptr;
ClusterflyManager* mqttManager = nullptr;

TaskHandle_t wifiTaskHandle = NULL;
struct WiFiParams {
  String ssid;
  String password;

  bool staticIpParamsMustBeInit;

  IPAddress staticIP;
  IPAddress gateway;
  IPAddress subnet; 
  IPAddress primaryDNS;
  IPAddress secondaryDNS;

};

WiFiParams updatedWiFiParams;
bool wifiParamsMustBeSaved = false;

uint32_t stopTimerSec;
float temperatureThreshold;
std::vector<IntervalTime> intervals;
float lastLitersPerMinute = 0.0f;
float maxLitersPerMinute = 0.0f;
uint8_t ignoreAfterTurningOn = 0;
bool flowExceededMaxValue = false; //больше ничего и не надо будет. Не заюудь за ночь
SemaphoreHandle_t mutex = xSemaphoreCreateMutex(); //это на строчки выше тема. Вотч демо вотч демо ww

uint32_t flowUpdatesAfterTurningOn = 0; //Счетчик количества измерений после включения
unsigned long lastFlowUpdate = 0;
volatile int pulseCount = 0;    // Счетчик импульсов
portMUX_TYPE pulseCountMux = portMUX_INITIALIZER_UNLOCKED; // Для атомарных операций
void IRAM_ATTR pulseCounter() {
  portENTER_CRITICAL_ISR(&pulseCountMux);
  pulseCount++;
  portEXIT_CRITICAL_ISR(&pulseCountMux);
}

void turnOnRelay();
void turnOffRelay();
void setupClock();
void setupFirstTimeEEPROM();

void readIntervalsFromEEPROM();
void saveIntervalsToEEPROM();

void printDateTime(const RtcDateTime& dt);

void botTask(void *pvParameters);
void mqttTask(void *pvParameters);
void localServTask(void *pvParameters);

void checkWiFiConnection(void * parameter) {
  //WiFi.persistent(false);

  //WiFi.mode(WIFI_STA);

  //WiFi.disconnect(true);

  //vTaskDelay(pdMS_TO_TICKS(5000));

  WiFiParams *params = (WiFiParams *) parameter;
  const uint8_t maxReconnectAttempts = 15;
  bool isFirstConnect = true;

  bool staticConnIsInited = (params->staticIP != IPAddress(0,0,0,0));
  Serial.println("wifi params: ");

  Serial.println(params->staticIP);
  Serial.println(params->gateway);
  Serial.println(params->subnet);
  Serial.println(params->primaryDNS);
  Serial.println(params->secondaryDNS);
  Serial.println(staticConnIsInited);
/*
  if(staticConnIsInited) WiFi.config(params->staticIP, 
                                      params->gateway, 
                                      params->subnet, 
                                      params->primaryDNS, 
                                      params->secondaryDNS);
*/
  bool serverIsRaised = false;
  for(;;) {
    if (WiFi.status() != WL_CONNECTED) {
      uint8_t reconnectAttempts = 0;



      WiFi.begin(params->ssid, params->password);

      
      Serial.print("Подключение к WiFi");
      while(WiFi.status() != WL_CONNECTED && maxReconnectAttempts > reconnectAttempts){
        reconnectAttempts++;
        Serial.print(".");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
      Serial.println();
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Подключено к Wi-Fi");
        client.setInsecure();
        client2.setInsecure();
        if(!serverIsRaised && localManager != nullptr) {
          localManager->raiseServer("smartgate");
          serverIsRaised = true;
        }
        

        if(!staticConnIsInited){
          updatedWiFiParams.staticIP = WiFi.localIP();
          updatedWiFiParams.gateway = WiFi.gatewayIP();
          updatedWiFiParams.subnet = WiFi.subnetMask();
          updatedWiFiParams.primaryDNS = WiFi.dnsIP(0);
          updatedWiFiParams.secondaryDNS = WiFi.dnsIP(1);

          wifiParamsMustBeSaved = true;
        }

        
      } else {
        Serial.println("Не удалось подключиться к Wi-Fi");
      }
    }
    // Ждем 15 секунд перед следующей проверкой
    vTaskDelay(15'000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(relayPin,OUTPUT);
  pinMode(parsePin,INPUT);
  pinMode(flowPin, INPUT_PULLUP);
  digitalWrite(relayPin,relayStatus); //удалишь - убью
  isParseSettingsMode = digitalRead(parsePin);
  
  lastFlowUpdate = millis();
  // Настройка прерывания на спад сигнала
  attachInterrupt(digitalPinToInterrupt(flowPin), 
                 pulseCounter, 
                 FALLING);
  

  Serial.begin(115200);
  EEPROM.begin(eepromSize);

  SettingsParser parser(parserAddress,false);
  if(isParseSettingsMode){
    WiFi.softAP("Leonov's gate");
    parser.raiseServer("smartgate");
    while(true) parser.tickServer();
  }
  chatId = parser.getChatId();

  if(parser.getBotToken() != "") bot = new UniversalTelegramBot(parser.getBotToken(), client);

  ConstructPtrs constructPtrs;
  constructPtrs.bmp = &bmp;
  constructPtrs.bot = bot;
  constructPtrs.relayStatus = &relayStatus;
  constructPtrs.lastLitersPerMinute = &lastLitersPerMinute;
  constructPtrs.rtc = &Rtc;
  constructPtrs.client = &client2;
  constructPtrs.bmpMutex = bmpMutex;
  constructPtrs.rtcMutex = rtcMutex;

  localManager = new LocalManager(constructPtrs);
  ChangePtrs chPtrs;
  chPtrs.intervals = &intervals;
  chPtrs.stopTimerSec = &stopTimerSec;
  chPtrs.temperatureThreshold = &temperatureThreshold;
  chPtrs.ignoreAfterTurningOn = &ignoreAfterTurningOn;
  chPtrs.maxLitersPerMinute = &maxLitersPerMinute;
  chPtrs.flowExceededMaxValue = &flowExceededMaxValue;
  chPtrs.mutex = mutex;
  localManager->setChangePtrs(chPtrs);

  if(bot != nullptr) telegramManager = new TelegramManager(constructPtrs,10'000,10'000);
  if(parser.getUserId() != "" && parser.getMqttPassword() != "") mqttManager = new ClusterflyManager(constructPtrs,parser.getUserId(),parser.getMqttPassword());

  WiFiParams *params = new WiFiParams();
  params->ssid = parser.getSSID();
  params->password = parser.getPassword();

  params->staticIpParamsMustBeInit = false;

  params->staticIP = parser.getStaticIP();//IPAddress(192,168,1,52);
  params->gateway = parser.getGateway();//IPAddress(192,168,1,1);
  params->subnet = parser.getSubnet();//IPAddress(255,255,255,0);
  params->primaryDNS = parser.getPrimaryDNS();//IPAddress(192,168,1,1);
  params->secondaryDNS = parser.getSecondaryDNS();//IPAddress(0,0,0,0);

  xTaskCreatePinnedToCore(
    checkWiFiConnection,  // Функция задачи
    "WiFiCheckTask",      // Название задачи
    4096,                 // Размер стека задачи
    (void *)params,       // Параметры для передачи в задачу
    1,                    // Приоритет задачи
    &wifiTaskHandle,      // Хендл задачи
    1                     // Ядро для выполнения (0 или 1)
  );

  if(EEPROM.read(keyAddress) != eepromKey){
    Serial.println("Прочитанный ключ: " + String(EEPROM.read(keyAddress)));
    setupFirstTimeEEPROM();
  }
  setupClock();
  EEPROM.get(timerAddress,stopTimerSec);
  EEPROM.get(temperatureAddress,temperatureThreshold);
  EEPROM.get(maxFlowAddress, maxLitersPerMinute);
  EEPROM.get(ignoreCountAddress, ignoreAfterTurningOn);
  readIntervalsFromEEPROM();

  Serial.print("Время остановки таймера: ");
  printDateTime(RtcDateTime(stopTimerSec));
  Serial.println();
  Serial.println("Температурный порог: " + String(temperatureThreshold));

  if(!bmp.begin(0x76)){
    vTaskDelay(5'000 / portTICK_PERIOD_MS);
    bot->sendMessage(chatId, "Датчик температуры не подключен");
  }
  else{
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X16,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_NONE,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_1000); /* Standby time. */
  }


  Serial.print("Свободная память: ");
  Serial.println(ESP.getFreeHeap());
  if(telegramManager != nullptr) xTaskCreatePinnedToCore(botTask, "Task1", 8192, NULL, 1, NULL, tskNO_AFFINITY);
  if(mqttManager != nullptr) xTaskCreatePinnedToCore(mqttTask, "Task2", 8192, NULL, 1, NULL, tskNO_AFFINITY);
  //if(localManager != nullptr) xTaskCreatePinnedToCore(localServTask, "Task3", 65536, NULL, 1, NULL, tskNO_AFFINITY); //закоментил тк использую Async библеотеку
}


void botTask(void *pvParameters) {

  bool isFirstInternetRequest = true;

  while (true) {

    if (isFirstInternetRequest && WiFi.status() == WL_CONNECTED){
      Serial.println("Попытка сделать первый запрос");
      if(bot->sendMessage(chatId, "кран перезагружен")){
        Serial.println("Первый запрос успешен");
        isFirstInternetRequest = false;
        bot->getUpdates(-1);
      }
    }


    ChangePtrs changePtrs;

    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      changePtrs.intervals = &intervals;
      changePtrs.stopTimerSec = &stopTimerSec;
      changePtrs.temperatureThreshold = &temperatureThreshold;
      changePtrs.ignoreAfterTurningOn = &ignoreAfterTurningOn;
      changePtrs.maxLitersPerMinute = &maxLitersPerMinute;
      changePtrs.flowExceededMaxValue = &flowExceededMaxValue;

      changePtrs.mutex = mutex;

      xSemaphoreGive(mutex);
    }
    if (WiFi.status() == WL_CONNECTED) telegramManager->tickBot(changePtrs); // Может быть долгим


    vTaskDelay(pdMS_TO_TICKS(1000));  // Ожидание в миллисекундах
  }

}

void mqttTask(void *pvParameters) {

  while (true) {
    ChangePtrs changePtrs;

    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      changePtrs.intervals = &intervals;
      changePtrs.stopTimerSec = &stopTimerSec;
      changePtrs.temperatureThreshold = &temperatureThreshold;
      changePtrs.ignoreAfterTurningOn = &ignoreAfterTurningOn;
      changePtrs.maxLitersPerMinute = &maxLitersPerMinute;
      changePtrs.flowExceededMaxValue = &flowExceededMaxValue;
      changePtrs.mutex = mutex;

      xSemaphoreGive(mutex);
    }

    if (WiFi.status() == WL_CONNECTED) mqttManager->tickMqtt(changePtrs); // Может быть долгим

    vTaskDelay(pdMS_TO_TICKS(1000));  // Ожидание в миллисекундах
  }

}

/*
void localServTask(void *pvParameters) {

  while (true) {
    ChangePtrs changePtrs;

    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      changePtrs.intervals = &intervals;
      changePtrs.stopTimerSec = &stopTimerSec;
      changePtrs.temperatureThreshold = &temperatureThreshold;
      changePtrs.mutex = mutex;

      xSemaphoreGive(mutex);
    }

    if (WiFi.status() == WL_CONNECTED) localManager->tickServer(changePtrs); // Может быть долгим


    vTaskDelay(pdMS_TO_TICKS(300));  // Ожидание в миллисекундах
  }
}
*/

int oldIntervalsSize;
uint32_t oldTimer;
float oldThreshold;
uint8_t oldIgnoreAfterTurningOn;
float oldMaxLitersPerMinute;
bool isFirstIter = true;

void loop() {
  // put your main code here, to run repeatedly:
  xSemaphoreTake(mutex, portMAX_DELAY);

  if(isFirstIter){
    oldIntervalsSize = intervals.size();
    oldTimer = stopTimerSec;
    oldThreshold = temperatureThreshold;
    oldMaxLitersPerMinute = maxLitersPerMinute;
    oldIgnoreAfterTurningOn = ignoreAfterTurningOn;
    isFirstIter = false;
  }

  if(oldIntervalsSize != intervals.size()) {
    saveIntervalsToEEPROM();
  }
  if(oldTimer != stopTimerSec){
    EEPROM.put(timerAddress,stopTimerSec);
    EEPROM.commit();
  }
  if(oldThreshold != temperatureThreshold){
    EEPROM.put(temperatureAddress,temperatureThreshold);
    EEPROM.commit();
  }
  if(oldIgnoreAfterTurningOn != ignoreAfterTurningOn){
    EEPROM.put(ignoreCountAddress,ignoreAfterTurningOn); //
    EEPROM.commit();
  }
  if(oldMaxLitersPerMinute != maxLitersPerMinute){
    EEPROM.put(maxFlowAddress,maxLitersPerMinute); //
    EEPROM.commit();
  }
  xSemaphoreGive(mutex);


  if(wifiParamsMustBeSaved){
    SettingsParser parser(parserAddress,true);

    parser.updateIPConfig(updatedWiFiParams.staticIP,
                          updatedWiFiParams.gateway,
                          updatedWiFiParams.subnet,
                          updatedWiFiParams.primaryDNS,
                          updatedWiFiParams.secondaryDNS);

    wifiParamsMustBeSaved = false;
  }


  xSemaphoreTake(mutex, portMAX_DELAY);
  xSemaphoreTake(rtcMutex, portMAX_DELAY);
  xSemaphoreTake(bmpMutex, portMAX_DELAY);

  RtcDateTime now = Rtc.GetDateTime();
  bool isTimerActive = now.TotalSeconds() < stopTimerSec;
  bool isInInterval = false;
  bool isTempThreshold = bmp.readTemperature() >= temperatureThreshold; //
  for(uint8_t i = 0; i < intervals.size(); i++){
    if(intervals[i].inInterval(now)) {
      isInInterval = true;
      break;
    }
  }

  if(millis() - lastFlowUpdate >= 60 * 1000){

    int currentPulseCount;
    portENTER_CRITICAL(&pulseCountMux);
    currentPulseCount = pulseCount;
    pulseCount = 0;
    portEXIT_CRITICAL(&pulseCountMux);
    lastFlowUpdate = millis();

    if(relayStatus) flowUpdatesAfterTurningOn++;
    else flowUpdatesAfterTurningOn = 0;

    lastLitersPerMinute = currentPulseCount / pulsesPerLiter;

    if(flowUpdatesAfterTurningOn > ignoreAfterTurningOn && lastLitersPerMinute > maxLitersPerMinute)
      flowExceededMaxValue = true;
    
  }

  

  if(!flowExceededMaxValue && (isTimerActive || isInInterval || isTempThreshold)){
    turnOnRelay();
  }
  else{
    turnOffRelay();
  }

  if(!isTimerActive && stopTimerSec != 0){
    stopTimerSec = 0;
    EEPROM.put(timerAddress,stopTimerSec);
    EEPROM.commit();
  }

  xSemaphoreGive(mutex);
  xSemaphoreGive(rtcMutex);
  xSemaphoreGive(bmpMutex);

  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void turnOnRelay(){
  if(!relayStatus){
    relayStatus = true;
    digitalWrite(relayPin,relayStatus);
  }
}

void turnOffRelay(){
  if(relayStatus){
    relayStatus = false;
    digitalWrite(relayPin,relayStatus);
  }
}

void setupFirstTimeEEPROM(){
  EEPROM.write(keyAddress,eepromKey);
  for(int i = 0;i < SettingsParser::getReservedSizeEEPROM();i++){
    EEPROM.write(parserAddress + i,0);
  }
  EEPROM.put(temperatureAddress,1000.0f);
  EEPROM.put(timerAddress,(uint32_t)0);

  EEPROM.put(maxFlowAddress,0.0f);
  EEPROM.put(ignoreCountAddress,0.0f);

  EEPROM.write(intervalsAddress,0);
  EEPROM.commit();
}

void printDateTime(const RtcDateTime& dt) {
  char datestring[20];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Month(),
             dt.Day(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second());
  Serial.print(datestring);
}

void setupClock(){
  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Serial.print("Компилируемое время: ");
  printDateTime(compiled);
  Serial.println();

  if (!Rtc.IsDateTimeValid()) {

    Serial.println("Часы имеют не корректное время");
    Rtc.SetDateTime(compiled);
  }

  if (Rtc.GetIsWriteProtected()) {
    Serial.println("Защита от записи будет отключена");
    Rtc.SetIsWriteProtected(false);
  }

  if (!Rtc.GetIsRunning()) {
    Serial.println("Включаем часы");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) {
    Serial.println("Время часов меньше чем компилируемое");
    Rtc.SetDateTime(compiled);
  } else if (now > compiled) {
    Serial.println("Время часов больше чем компилируемое");
  } else if (now == compiled) {
    Serial.println("Время часов равно компилируемому");
  }
}

void readIntervalsFromEEPROM(){
  uint8_t intervalsCount = EEPROM.read(intervalsAddress);
  for(uint8_t i = 0;i < intervalsCount;i++){
    IntervalTime inTime;
    EEPROM.get(intervalsAddress + sizeof(uint8_t) + i * sizeof(IntervalTime), inTime);
    intervals.push_back(inTime);
  }
}

void saveIntervalsToEEPROM(){
  EEPROM.write(intervalsAddress,intervals.size());
  for(int i = 0 ; i < intervals.size(); i++){
    EEPROM.put(intervalsAddress + sizeof(uint8_t) + i * sizeof(IntervalTime),intervals[i]);
  }
  EEPROM.commit();
}