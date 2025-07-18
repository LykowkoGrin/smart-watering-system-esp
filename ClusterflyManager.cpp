#include"ClusterflyManager.h"
#include <TimeLib.h>

ClusterflyManager::ClusterflyManager(const ConstructPtrs& params,String userId,String password,const uint32_t& updateDelay){
  this->updateDelay = updateDelay;
  rtc         = params.rtc;
  relayStatus = params.relayStatus;
  lastLitersPerMinute = params.lastLitersPerMinute;
  bmp         = params.bmp;
  bmpMutex    = params.bmpMutex;
  rtcMutex    = params.rtcMutex;

  this->userId = userId;
  this->password = password;
  mqtt = new PubSubClient(*(params.client));

  String userIdForm = "/" + userId;
  tempThresholdTopic = userIdForm + tempThresholdTopic;
  intervalTopic = userIdForm + intervalTopic;
  timerTopic = userIdForm + timerTopic;
  statusTopic = userIdForm + statusTopic;
  logsTopic = userIdForm + logsTopic;
  tempTopic = userIdForm + tempTopic;
  delIntervalsTopic = userIdForm + delIntervalsTopic;
  
  maxFlowTopic = userIdForm + maxFlowTopic;
  ignoreCountTopic = userIdForm + ignoreCountTopic;
  flowTopic = userIdForm + flowTopic;
  flowBlockTopic = userIdForm + flowBlockTopic;
  unblockFlowTopic = userIdForm + unblockFlowTopic;

  mqtt->setServer("srv2.clusterfly.ru", 9992);
}

void ClusterflyManager::tickMqtt(const ChangePtrs& params){
  intervals = params.intervals;
  stopTimerSec = params.stopTimerSec;
  temperatureThreshold = params.temperatureThreshold;
  maxLitersPerMinute = params.maxLitersPerMinute;
  ignoreAfterTurningOn = params.ignoreAfterTurningOn;
  flowExceededMaxValue = params.flowExceededMaxValue;
  lastDataUpdate = params.lastDataUpdate;
  mutex = params.mutex;
  
  if(!mqtt->connected()) connectToMqtt();
  if(!mqtt->connected()) return;

  mqtt->loop();

  if(millis() - lastUpdateTime > updateDelay){
    if(millis() - lastRequestTime < minRequestDelay) vTaskDelay(minRequestDelay / portTICK_PERIOD_MS);
    updateData();
    lastUpdateTime = millis();
  }
}

void ClusterflyManager::mqttCallback(char* topic, byte* payload, unsigned int length){
  if (instance) {
    instance->handleMessage(topic, payload, length);
  }
}

void ClusterflyManager::handleMessage(char* topic, byte* payload, unsigned int length){
  String strTopic = String(topic);

  if(strTopic == tempThresholdTopic) processTempTopic(payload, length);
  else if(strTopic == intervalTopic) processIntervalTopic(payload, length);
  else if(strTopic == timerTopic) processTimerTopic(payload, length);
  else if(strTopic == delIntervalsTopic) processDeleteTopic(payload, length);
  else if(strTopic == maxFlowTopic) processMaxFlowTopic(payload, length);
  else if(strTopic == ignoreCountTopic) processIgnoreCountTopic(payload, length);
  else if(strTopic == unblockFlowTopic) processUnblockFlow(payload, length);
}

void ClusterflyManager::connectToMqtt(){
  mqtt->connect("esptest2", userId.c_str(), password.c_str());

  mqtt->subscribe(tempThresholdTopic.c_str());
  mqtt->subscribe(intervalTopic.c_str());
  mqtt->subscribe(timerTopic.c_str());
  mqtt->subscribe(delIntervalsTopic.c_str());

  mqtt->subscribe(maxFlowTopic.c_str());
  mqtt->subscribe(ignoreCountTopic.c_str());
  mqtt->subscribe(unblockFlowTopic.c_str());

  instance = this;
  mqtt->setCallback(mqttCallback);
}

void ClusterflyManager::updateData(){
  if(millis() - lastRequestTime < minRequestDelay) vTaskDelay(minRequestDelay / portTICK_PERIOD_MS);
  mqtt->publish(statusTopic.c_str(),String(*relayStatus).c_str());

  vTaskDelay(minRequestDelay / portTICK_PERIOD_MS);
  mqtt->publish(flowBlockTopic.c_str(),String(*flowExceededMaxValue).c_str());


  vTaskDelay(minRequestDelay / portTICK_PERIOD_MS);
  xSemaphoreTake(bmpMutex, portMAX_DELAY);
  mqtt->publish(tempTopic.c_str(),String(bmp->readTemperature()).c_str());
  xSemaphoreGive(bmpMutex);

  vTaskDelay(minRequestDelay / portTICK_PERIOD_MS);
  xSemaphoreTake(mutex, portMAX_DELAY);
  float nowFlow = *lastLitersPerMinute;
  xSemaphoreGive(mutex);
  mqtt->publish(flowTopic.c_str(),String(nowFlow).c_str());
  
  lastRequestTime = millis();
}

void ClusterflyManager::processTempTopic(byte* payload, unsigned int length){
  if(millis() - lastRequestTime < minRequestDelay) vTaskDelay(minRequestDelay / portTICK_PERIOD_MS);

  float t;
  try{
    t = std::stof(String(payload,length).c_str());
    String strResp = "Порог установлен на: " + String(t);
    if(mqtt->publish(logsTopic.c_str(),strResp.c_str())){
      xSemaphoreTake(mutex, portMAX_DELAY);
      *temperatureThreshold = t;
      xSemaphoreGive(mutex);
      updateLastUpdateTime();
    }
  }
  catch(std::exception ex){
    mqtt->publish(logsTopic.c_str(),"неверный формат температуры");
  }
  lastRequestTime = millis();
}

void ClusterflyManager::processDeleteTopic(byte* payload, unsigned int length){
  if(millis() - lastRequestTime < minRequestDelay) vTaskDelay(minRequestDelay / portTICK_PERIOD_MS);

  if(mqtt->publish(logsTopic.c_str(),"Интервалы удалены")){

    xSemaphoreTake(mutex, portMAX_DELAY);
    intervals->clear();
    xSemaphoreGive(mutex);
    updateLastUpdateTime();

    lastRequestTime = millis();
  }
}

void ClusterflyManager::processUnblockFlow(byte* payload, unsigned int length){
  if(millis() - lastRequestTime < minRequestDelay) vTaskDelay(minRequestDelay / portTICK_PERIOD_MS);

  if(mqtt->publish(logsTopic.c_str(),"Поток разблокирован")){

    xSemaphoreTake(mutex, portMAX_DELAY);
    *flowExceededMaxValue = false;
    xSemaphoreGive(mutex);
    updateLastUpdateTime();

    lastRequestTime = millis();
  }
}

void ClusterflyManager::processIntervalTopic(byte* payload, unsigned int length){
  if(millis() - lastRequestTime < minRequestDelay) vTaskDelay(minRequestDelay / portTICK_PERIOD_MS);

  String intervalStr = String(payload,length);
  int delimiterIndex = intervalStr.indexOf('-');
  if(delimiterIndex == -1)
  {
    mqtt->publish(logsTopic.c_str(),"Не найденно тире в интревале");
    lastRequestTime = millis();
    return;
  }
  uint8_t h1,m1,h2,m2;
  String startStr = intervalStr.substring(0, delimiterIndex);
  String stopStr = intervalStr.substring(delimiterIndex + 1);
  if(!IntervalTime::parseTime(startStr,h1,m1) || !IntervalTime::parseTime(stopStr,h2,m2)){
    mqtt->publish(logsTopic.c_str(),"Неверный интервал");
    lastRequestTime = millis();
    return;
  }

  IntervalTime inTime;
  inTime.start = h1 * 3600 + m1 * 60;
  inTime.stop = h2 * 3600 + m2 * 60;
  String reqStr = String("Добавлен интревал ") + String(inTime.start) + String(":") + String(inTime.stop);
  if(mqtt->publish(logsTopic.c_str(),reqStr.c_str())){

    xSemaphoreTake(mutex, portMAX_DELAY);
    intervals->push_back(inTime);
    xSemaphoreGive(mutex);
    updateLastUpdateTime();

    lastRequestTime = millis();
  }
}

void ClusterflyManager::processTimerTopic(byte* payload, unsigned int length){
  if(millis() - lastRequestTime < minRequestDelay) vTaskDelay(minRequestDelay / portTICK_PERIOD_MS);

  String strTime = String(payload,length);
  uint32_t sec;
  if(!convertTimeToSec(strTime,sec)){
    mqtt->publish(logsTopic.c_str(),"Неверный формат окончания таймера");
    lastRequestTime = millis();
    return;
  }

  uint32_t nowSec;
  if(xSemaphoreTake(rtcMutex, portMAX_DELAY)){
    nowSec = (rtc->GetDateTime()).TotalSeconds();
    xSemaphoreGive(rtcMutex);
  }

  String req = String("Таймер остановится через ") + String((sec - nowSec) / 60.f) + String(" минут");
  if(mqtt->publish(logsTopic.c_str(), req.c_str())){
    xSemaphoreTake(mutex, portMAX_DELAY);
    *stopTimerSec = sec;
    xSemaphoreGive(mutex);

    updateLastUpdateTime();
    lastRequestTime = millis();
  }
}

void ClusterflyManager::processMaxFlowTopic(byte* payload, unsigned int length){
  if(millis() - lastRequestTime < minRequestDelay) vTaskDelay(minRequestDelay / portTICK_PERIOD_MS);

  float t;
  try{
    t = std::stof(String(payload,length).c_str());
    String strResp = "Максимальный поток: " + String(t);
    if(mqtt->publish(logsTopic.c_str(),strResp.c_str())){
      xSemaphoreTake(mutex, portMAX_DELAY);
      *maxLitersPerMinute = t;
      xSemaphoreGive(mutex);

      updateLastUpdateTime();
    }
  }
  catch(std::exception ex){
    mqtt->publish(logsTopic.c_str(),"неверный формат");
  }
  lastRequestTime = millis();
}
void ClusterflyManager::processIgnoreCountTopic(byte* payload, unsigned int length){
  if(millis() - lastRequestTime < minRequestDelay) vTaskDelay(minRequestDelay / portTICK_PERIOD_MS);

  int t;
  try{
    t = std::stoi(String(payload,length).c_str());
    String strResp = "Количество игнорирований: " + String(t);
    if(mqtt->publish(logsTopic.c_str(),strResp.c_str())){
      xSemaphoreTake(mutex, portMAX_DELAY);
      *ignoreAfterTurningOn = t;
      xSemaphoreGive(mutex);

      updateLastUpdateTime();
    }
  }
  catch(std::exception ex){
    mqtt->publish(logsTopic.c_str(),"неверный формат");
  }
  lastRequestTime = millis();
}

bool ClusterflyManager::convertTimeToSec(const String& dateTime, uint32_t& seconds){
  if (dateTime.length() != 19 || dateTime[10] != 'T') {
        return false;
    }

    // Извлекаем значения года, месяца, дня, часов, минут и секунд
    int year = dateTime.substring(0, 4).toInt();
    int month = dateTime.substring(5, 7).toInt();
    int day = dateTime.substring(8, 10).toInt();
    int hour = dateTime.substring(11, 13).toInt();
    int minute = dateTime.substring(14, 16).toInt();
    int second = dateTime.substring(17, 19).toInt();

    // Проверяем корректность даты
    if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31 || 
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        return false;
    }

    // Используем библиотеку TimeLib для вычисления секунд
    tmElements_t tm;
    tm.Year = year - 2000;  // tm.Year считает от 2000 года
    tm.Month = month;
    tm.Day = day;
    tm.Hour = hour;
    tm.Minute = minute;
    tm.Second = second;

    seconds = makeTime(tm);  // makeTime возвращает секунды с 1 января 2000 года
    return true;
}

void ClusterflyManager::updateLastUpdateTime(){
  xSemaphoreTake(mutex, portMAX_DELAY);
  xSemaphoreTake(rtcMutex, portMAX_DELAY);
  *lastDataUpdate = rtc->GetDateTime();
  xSemaphoreGive(mutex);
  xSemaphoreGive(rtcMutex);
}



ClusterflyManager* ClusterflyManager::instance = nullptr;