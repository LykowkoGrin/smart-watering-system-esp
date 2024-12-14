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

const int parsePin = 23;
const int relayPin1 = 4;

const int keyAddress = 0;
const int parserAddress = keyAddress + sizeof(uint8_t);
const int timerAddress = parserAddress + SettingsParser::getReservedSizeEEPROM();
const int temperatureAddress = timerAddress + sizeof(uint32_t);
const int intervalsAddress = temperatureAddress + sizeof(float);
const int eepromSize = intervalsAddress + 150;

const uint8_t eepromKey = 115;
/*
const int CLOCK_DAT = 27;  
const int CLOCK_CLK = 14;
const int CLOCK_RST = 26;
*/
const int CLOCK_DAT = 23; // DATA (IO)
const int CLOCK_CLK = 18; // CLOCK
const int CLOCK_RST = 5;  // CE (RST)

bool relayStatus = false;
bool isFirstInternetRequest = true;
bool isParseSettingsMode = false;

String chatId;
Adafruit_BMP280 bmp;
WiFiClientSecure client;
WiFiClientSecure client2;
UniversalTelegramBot *bot;//(botToken, client);
ThreeWire myWire(CLOCK_DAT, CLOCK_CLK, CLOCK_RST);
RtcDS1302<ThreeWire> Rtc(myWire);
//PubSubClient mqtt(client);

LocalManager* localManager = nullptr;
TelegramManager* telegramManager = nullptr;
ClusterflyManager* mqttManager = nullptr;

TaskHandle_t wifiTaskHandle = NULL;
struct WiFiParams {
  String ssid;
  String password;
};

uint32_t stopTimerSec;
float temperatureThreshold;
std::vector<IntervalTime> intervals;

//void connectToWiFi();
void setupClock();
void setupFirstTimeEEPROM();
//void handleMessage(int messageIndex);

void readIntervalsFromEEPROM();
void saveIntervalsToEEPROM();

void printDateTime(const RtcDateTime& dt);

void checkWiFiConnection(void * parameter) {
  WiFiParams *params = (WiFiParams *) parameter;
  const uint8_t maxReconnectAttempts = 15;
  bool isFirstConnect = true;
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
        if(!isParseSettingsMode && localManager != nullptr) {
          localManager->raiseServer("smartgate");
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
  pinMode(relayPin1,OUTPUT);
  pinMode(parsePin,INPUT);
  digitalWrite(relayPin1,relayStatus);
  isParseSettingsMode = digitalRead(parsePin);

  Serial.begin(115200);
  EEPROM.begin(eepromSize);

  SettingsParser parser(parserAddress);
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
  constructPtrs.rtc = &Rtc;
  constructPtrs.client = &client2;

  localManager = new LocalManager(constructPtrs);
  if(bot != nullptr) telegramManager = new TelegramManager(constructPtrs,10'000,10'000);
  if(parser.getUserId() != "" && parser.getMqttPassword() != "") mqttManager = new ClusterflyManager(constructPtrs,parser.getUserId(),parser.getMqttPassword());

  WiFiParams *params = new WiFiParams();
  params->ssid = parser.getSSID();
  params->password = parser.getPassword();
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
}
void loop() {
  // put your main code here, to run repeatedly:
  if (WiFi.status() == WL_CONNECTED){
    if(isFirstInternetRequest && bot != nullptr){
      Serial.println("Попытка сделать первый запрос");
      if(bot->sendMessage(chatId, "кран перезагружен")){
        Serial.println("Первый запрос успешен");
        isFirstInternetRequest = false;
        bot->getUpdates(-1);
      }
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    ChangePtrs changePtrs;
    changePtrs.intervals = &intervals;
    changePtrs.stopTimerSec = &stopTimerSec;
    changePtrs.temperatureThreshold = &temperatureThreshold;

    int oldIntervalsSize = intervals.size();
    uint32_t oldTimer = stopTimerSec;
    float oldThreshold = temperatureThreshold;

    if(localManager != nullptr) localManager->tickServer(changePtrs);
    if(telegramManager != nullptr) telegramManager->tickBot(changePtrs);
    if(mqttManager != nullptr) mqttManager->tickMqtt(changePtrs);

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
    /*
    if (millis() - lastTimeBotRan > (dialog.inProcess() ? inputModeBotRequestDelay : baseBotRequestDelay)) {
      int messageCount = bot->getUpdates(-1);
      if (messageCount > 0) {
        handleMessage(messageCount - 1);
      }

      lastTimeBotRan = millis();
    }
    */


  }

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

  if(isTimerActive || isInInterval || isTempThreshold){
    if(!relayStatus){
      relayStatus = true;
      digitalWrite(relayPin1,relayStatus);
    }
  }
  else{
    if(relayStatus){
      relayStatus = false;
      digitalWrite(relayPin1,relayStatus);
    }
  }

  if(!isTimerActive && stopTimerSec != 0){
    stopTimerSec = 0;
    EEPROM.put(timerAddress,stopTimerSec);
    EEPROM.commit();
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS); //секунда
}

void setupFirstTimeEEPROM(){
  EEPROM.write(keyAddress,eepromKey);
  for(int i = 0;i < SettingsParser::getReservedSizeEEPROM();i++){
    EEPROM.write(parserAddress + i,0);
  }
  EEPROM.put(temperatureAddress,1000.0f);
  EEPROM.put(timerAddress,(uint32_t)0);
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