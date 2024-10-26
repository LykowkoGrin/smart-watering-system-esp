#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <EEPROM.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <vector>

const char* ssid = "WiFi name";
const char* password = "WiFi password";
const char* botToken = "your bot token";
const char* chatId = "your chat id";

const int eepromSize = 500;
const int relayPin1 = 4;
const int relayPin2 = relayPin1;
const int keyAddress = 0;
const int timerAddress = keyAddress + sizeof(uint8_t);
const int temperatureAddress = timerAddress + sizeof(uint32_t);
const int intervalsAddress = temperatureAddress + sizeof(float);

const uint8_t eepromKey = 111;

const int CLOCK_DAT = 27;  
const int CLOCK_CLK = 14;
const int CLOCK_RST = 26;

const int baseBotRequestDelay = 10'000;
const int inputModeBotRequestDelay = 10'000;
unsigned long lastTimeBotRan = 0;
bool relayStatus = false;
bool isFirstInternetRequest = true;

Adafruit_BMP280 bmp;
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);
ThreeWire myWire(CLOCK_DAT, CLOCK_CLK, CLOCK_RST);
RtcDS1302<ThreeWire> Rtc(myWire);
TaskHandle_t wifiTaskHandle = NULL;

uint32_t stopTimerSec;
float temperatureThreshold;

//void connectToWiFi();
void setupClock();
void setupFirstTimeEEPROM();
void handleMessage(int messageIndex);

void readIntervalsFromEEPROM();
void saveIntervalsToEEPROM();

void printDateTime(const RtcDateTime& dt);
bool parseTime(const String& timeStr, uint8_t& hour, uint8_t& minute);

void checkWiFiConnection(void * parameter) {
  const uint8_t maxReconnectAttempts = 15;
  bool isFirstConnect = true;
  for(;;) {
    if (WiFi.status() != WL_CONNECTED) {
      uint8_t reconnectAttempts = 0;
      WiFi.begin(ssid, password);
      Serial.print("Подключение к WiFi");
      while(WiFi.status() != WL_CONNECTED && maxReconnectAttempts > reconnectAttempts){
        reconnectAttempts++;
        Serial.print(".");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
      Serial.println();
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Подключено к Wi-Fi");
        client.setInsecure();   // Отключаем проверку сертификатов, чтобы избежать проблем с SSL
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
  pinMode(relayPin2,OUTPUT);
  digitalWrite(relayPin1,relayStatus);
  digitalWrite(relayPin2,relayStatus);

  xTaskCreatePinnedToCore(
    checkWiFiConnection,  // Функция задачи
    "WiFiCheckTask",      // Название задачи
    4096,                 // Размер стека задачи
    NULL,                 // Параметры для передачи в задачу
    1,                    // Приоритет задачи
    &wifiTaskHandle,      // Хендл задачи
    1                     // Ядро для выполнения (0 или 1)
  );

  Serial.begin(115200);
  EEPROM.begin(eepromSize);

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
    bot.sendMessage(chatId, "Датчик температуры не подключен");
  }
  else{
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X16,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_NONE,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_1000); /* Standby time. */
  }


  //connectToWiFi();
}

struct IntervalTime {
  bool inInterval(const RtcDateTime& dt){
    uint32_t dtInSec = dt.Hour() * 3600 + dt.Minute() * 60;
    if(start < stop)
      return start <= dtInSec && dtInSec <= stop;
    if(start > stop){
      if(start <= dtInSec) return true;
      if(stop >= dtInSec) return true;
    }
  }
  String toString(){
    RtcDateTime startDt= RtcDateTime(start);
    RtcDateTime stopDt= RtcDateTime(stop);
    String startStr = String(startDt.Hour()) + ":" + String(startDt.Minute());
    String stopStr = String(stopDt.Hour()) + ":" + String(stopDt.Minute());
    return startStr + "-" + stopStr;
  }
  uint32_t start;
  uint32_t stop;
};

std::vector<IntervalTime> intervals;

class Dialog{
public:
  bool processNewMessage(const String& msg){
    if(stage == processStage::none){
      if(msg != "/полив") return false;
      bool isSend = bot.sendMessage(chatId, "Введите число от 1 до 4:\n1 - включить полив на некторое время\n2 - добавить интервал включения полива\n3 - установить температурный порог включения полива\n4 - убрать один из интревалов включения полива");
      if(!isSend) ESP.restart();
      stage = processStage::chooseInput;
      return true;
    }
    else if(stage == processStage::chooseInput){
      if(msg.length() != 1){
        if(!bot.sendMessage(chatId,"Кол-во символов более 1!")) ESP.restart();
        stage = processStage::none;
        return false;
      }

      int num = 0;
      try {
        num = std::stoi(msg.c_str());
      }
      catch(std::exception ex){
        if(!bot.sendMessage(chatId,"Неверный формат!")) ESP.restart();
        stage = processStage::none;
        return false;
      }

      if(num < 1 || num > 4){
        if(!bot.sendMessage(chatId,"Цифра должна быть от 1 до 4!")) ESP.restart();
        stage = processStage::none;
        return false;
      }

      switch(num){
        case 1:
          if(!bot.sendMessage(chatId,"Введите время, на которое будет включен полив в формате: \"час:минута\". Час и минута могут быть более чем 24 и 60, но не более 256.")) 
            ESP.restart();
          stage = processStage::timerInput;
          break;
        case 2:
          if(!bot.sendMessage(chatId,"Введите начало и конец включения в формате: \"час:минута-час:минута\". Если начало будет больше чем конец, то выключение произоёдет через 24 часа.")) 
            ESP.restart();
          stage = processStage::intervalInput;
          break;
        case 3:
          if(!bot.sendMessage(chatId,"Введите порог температуры. Можно использовать не целые числа.")) 
            ESP.restart();
          stage = processStage::temperatureInput;
          break;
        case 4:
          String intervalsStr;
          for(int i = 0 ; i < intervals.size(); i ++){
            intervalsStr += "\n" + String(i) + ") " + intervals[i].toString();
          }
          if(!bot.sendMessage(chatId,"Введите номер удаляемого интервала:" + intervalsStr)) ESP.restart();
          stage = processStage::delIntervalInput;
          break;
      }

      return true;
    }
    else if(stage == processStage::timerInput){
      if(msg.length() < 3){
        stage = processStage::none;
        if(!bot.sendMessage(chatId,"Неверный формат!")) ESP.restart();
        return false;
      }
      uint8_t h,m;
      if(!parseTime(msg,h,m)){
        stage = processStage::none;
        if(!bot.sendMessage(chatId,"Неверный формат!")) ESP.restart();
        return false;
      }
      RtcDateTime now = Rtc.GetDateTime();
      stopTimerSec = now.TotalSeconds() + h * 3600 + m * 60;
      if(!bot.sendMessage(chatId,"Таймер установлен на " + String(h) + " часов и " + String(m) + " минут")) ESP.restart();
      EEPROM.put(timerAddress,stopTimerSec);
      EEPROM.commit();
      stage = processStage::none;
      return true;
    }
    else if(stage == processStage::intervalInput){
      if(msg.length() < 7){
        stage = processStage::none;
        if(!bot.sendMessage(chatId,"Неверный формат!")) ESP.restart();
        return false;
      }
      int delimiterIndex = msg.indexOf('-');
      if(delimiterIndex == -1){
        stage = processStage::none;
        if(!bot.sendMessage(chatId,"Тире не найдено!")) ESP.restart();
        return false;
      }

      String startStr = msg.substring(0, delimiterIndex);
      String stopStr = msg.substring(delimiterIndex + 1);
      uint8_t startH,startM,stopH,stopM;
      if(!parseTime(startStr, startH, startM) || !parseTime(stopStr, stopH, stopM)){
        stage = processStage::none;
        if(!bot.sendMessage(chatId,"Неверный формат!")) ESP.restart();
        return false;
      }
      if(startH > 23 || startM > 59 || stopH > 23 || stopM > 59){
        stage = processStage::none;
        if(!bot.sendMessage(chatId,"Час не должен быть больше 23 и минута больше чем 59!")) ESP.restart();
        return false;
      }
      String intervalSetResponse = "Добавлен интревал с " + String(startH) + ":" + String(startM) + " до " + String(stopH) + ":" + String(stopM);
      if(!bot.sendMessage(chatId,intervalSetResponse)) ESP.restart();
      IntervalTime inTime;
      inTime.start = startH * 3600 + startM * 60;
      inTime.stop = stopH * 3600 + stopM * 60;
      intervals.push_back(inTime);
      saveIntervalsToEEPROM();
      stage = processStage::none;
      return true;
    }
    else if(stage == processStage::delIntervalInput){

      int index;
      try{
        index = std::stoi(msg.c_str());
      }
      catch(std::exception ex){
        if(!bot.sendMessage(chatId,"Неверный формат!")) ESP.restart();
        stage = processStage::none;
        return false;
      }

      if(index < 0 || index >= intervals.size()){
        if(!bot.sendMessage( chatId,"Число должно быть больше 0 и меньше чем " + String(intervals.size()) )) ESP.restart();
        stage = processStage::none;
        return false;
      }

      if(!bot.sendMessage( chatId, "Интревал " + intervals[index].toString() + " будет удалён")) ESP.restart();
      intervals.erase(intervals.begin()+index);
      
      stage = processStage::none;
      return true;

    }
    else if(stage == processStage::temperatureInput){
      String niceFormatMsg = msg;
      niceFormatMsg.replace(',','.');
      float tempVal;
      try{
        tempVal = std::stof(niceFormatMsg.c_str());
      }
      catch(std::exception ex){
        if(!bot.sendMessage(chatId,"Неверный формат!")) ESP.restart();
        return false;
      }
      if(!bot.sendMessage(chatId,"Температурный порог установлен на " + String(tempVal))) ESP.restart();
      temperatureThreshold = tempVal;
      EEPROM.put(temperatureAddress,temperatureThreshold);
      EEPROM.commit();

      stage = processStage::none;
      return true;
    }

    return false;
  }
  bool inProcess(){
    return stage != processStage::none;
  }
private:
  enum processStage{
    none,
    timerInput,
    intervalInput,
    temperatureInput,
    delIntervalInput,
    chooseInput
  };
  processStage stage = processStage::none;
};

Dialog dialog;

void loop() {
  // put your main code here, to run repeatedly:
  if (WiFi.status() == WL_CONNECTED){
    if(isFirstInternetRequest){
      Serial.println("Попытка сделать первый запрос");
      if(bot.sendMessage(chatId, "кран перезагружен")){
        Serial.println("Первый запрос успешен");
        isFirstInternetRequest = false;
        bot.getUpdates(-1);
      }
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (millis() - lastTimeBotRan > (dialog.inProcess() ? inputModeBotRequestDelay : baseBotRequestDelay)) {
      int messageCount = bot.getUpdates(-1);
      if (messageCount > 0) {
        handleMessage(messageCount - 1);
      }

      lastTimeBotRan = millis();
    }
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
      digitalWrite(relayPin2,relayStatus);
    }
  }
  else{
    if(relayStatus){
      relayStatus = false;
      digitalWrite(relayPin1,relayStatus);
      digitalWrite(relayPin2,relayStatus);
    }
  }

  if(!isTimerActive && stopTimerSec != 0){
    stopTimerSec = 0;
    EEPROM.put(timerAddress,stopTimerSec);
    EEPROM.commit();
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS); //секунда
}


void handleMessage(int messageIndex){
  if(messageIndex < 0) return;
  String messageText = bot.messages[messageIndex].text;
  Serial.println(bot.messages[messageIndex].chat_id);

  if(dialog.processNewMessage(messageText)){
    
  } else if(messageText == "/статус"){

    String intervalsInfo;
    for(int i = 0 ; i < intervals.size(); i ++){
      intervalsInfo += "\n" + intervals[i].toString();
    }

    if(intervals.size()){
      intervalsInfo = "Интревалы включения полива: " + intervalsInfo;
    }
    else{
      intervalsInfo = "Интревалы включения не указаны";
    }

    uint32_t timeToStop = stopTimerSec - Rtc.GetDateTime().TotalSeconds();
    String timerInfo = stopTimerSec == 0 ? "Таймер выключен" : "Остановка таймера через " + String(timeToStop / 60.f) + " минут";
    String relayInfo = relayStatus ? "Полив включен" : "Полив выключен";
    String temperatureInfo = "Температурный порог: " + String(temperatureThreshold) + ". Актуальная температура: " + String(bmp.readTemperature());
    
    if(bot.sendMessage(bot.messages[messageIndex].chat_id, intervalsInfo + "\n" + timerInfo + "\n" + temperatureInfo + "\n" + relayInfo)){
      
    }
    else{
      ESP.restart();
    }
  }

}

void setupFirstTimeEEPROM(){
  EEPROM.write(keyAddress,eepromKey);
  EEPROM.put(temperatureAddress,1000.0f);
  EEPROM.put(timerAddress,(uint32_t)0);
  EEPROM.write(intervalsAddress,0);
  EEPROM.commit();
}

void connectToWiFi() {
  const int maxReconnectAttempts = 10;
  int reconnectAttempts = 0;

  Serial.println("Подключение к WiFi...");
  WiFi.begin(ssid, password);
  
  // Ограничиваем количество попыток подключения
  while (WiFi.status() != WL_CONNECTED && reconnectAttempts < maxReconnectAttempts) {
    delay(1000);
    Serial.print(".");
    reconnectAttempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nПодключено к Wi-Fi");
    reconnectAttempts = 0; // Сбрасываем количество попыток после успешного подключения
    client.setInsecure();   // Отключаем проверку сертификатов, чтобы избежать проблем с SSL
  } else {
    Serial.println("\nНе удалось подключиться к Wi-Fi");
    ESP.restart();
  }
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

bool isNumeric(const String& str) {
    for (unsigned int i = 0; i < str.length(); i++) {
        if (!isDigit(str[i])) {
            return false; // Если встречен нечисловой символ
        }
    }
    return true;
}

bool parseTime(const String& timeStr, uint8_t& hour, uint8_t& minute) {
    int colonIndex = timeStr.indexOf(':');
    
    // Проверка, есть ли двоеточие и корректен ли формат
    if (colonIndex == -1) {
        return false; // Нет двоеточия
    }

    // Извлекаем часы и минуты
    String hourStr = timeStr.substring(0, colonIndex);
    String minuteStr = timeStr.substring(colonIndex + 1);

    // Проверяем, что обе части строки являются числами
    if (isNumeric(hourStr) && isNumeric(minuteStr)) {
        hour = hourStr.toInt();
        minute = minuteStr.toInt();
        return true; // Успешный парсинг
    }

    return false; // Некорректный формат
}
