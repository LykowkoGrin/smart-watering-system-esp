#include "TelegramManager.h"


TelegramManager::TelegramManager(const ConstructPtrs& params,const uint32_t& requestDelay,const uint32_t& inputRequestDelay){
  bot = params.bot;
  rtc = params.rtc;
  bmp = params.bmp;
  bmpMutex = params.bmpMutex;
  rtcMutex = params.rtcMutex;

  relayStatus = params.relayStatus;
  lastLitersPerMinute = params.lastLitersPerMinute;

  this->requestDelay = requestDelay;
  this->inputRequestDelay = inputRequestDelay;
  lastTimeBotRan = 0;
}

void TelegramManager::tickBot(const ChangePtrs& params){
  if(millis() - lastTimeBotRan < (stage == processStage::none ? requestDelay : inputRequestDelay)) return;

  mutex = params.mutex;

  lastTimeBotRan = millis();
  lastDataUpdate = params.lastDataUpdate;
  changePtrs = params;
  handleNewMessage();
}

void TelegramManager::handleNewMessage(){
  messageCount = bot->getUpdates(-1);

  if (messageCount == 0) return;

  switch(stage){
    case(none):
      processFirstMessage();
      break;
    case(chooseInput):
      processChoose();
      break;
    case(timerInput):
      processTimer();
      break;
    case(intervalInput):
      processInterval();
      break;
    case(delIntervalInput):
      processDelete();
      break;
    case(temperatureInput):
      processTemperature();
      break;
    case(maxFlowInput):
      processMaxFlow();
      break;
    case(ignoreCountInput):
      processIgnoreCount();
      break;
  }
}

void TelegramManager::processFirstMessage(){
  String msg = bot->messages[messageCount - 1].text;
  String chatId = bot->messages[messageCount - 1].chat_id;
  if(msg == "/полив"){
    bool isSend = bot->sendMessage(chatId, 
"Введите число от 1 до 7:\n"
"1 - установить таймер\n"
"2 - добавить интервал включения полива\n"
"3 - установить температурный порог включения полива\n"
"4 - убрать один из интервалов включения полива\n"
"5 - установить верхнюю границу потока воды\n"
"6 - установить количество игнорируемых измерений после включения\n"
"7 - возобновить полив");
    if(!isSend) stage = processStage::none;
    stage = processStage::chooseInput;
  }
  else if(msg == "/статус"){
    xSemaphoreTake(mutex, portMAX_DELAY);
    String intervalsInfo;
    for(int i = 0 ; i < (changePtrs.intervals)->size(); i ++){
      intervalsInfo += "\n" + (*changePtrs.intervals)[i].toString();
    }

    if((changePtrs.intervals)->size()){
      intervalsInfo = "Интревалы включения полива: " + intervalsInfo;
    }
    else{
      intervalsInfo = "Интревалы включения не указаны";
    }

    float nowTemp;
    uint32_t nowSec;
    if (xSemaphoreTake(bmpMutex, portMAX_DELAY)){
      nowTemp = bmp->readTemperature();
      xSemaphoreGive(bmpMutex);
    }

    if (xSemaphoreTake(rtcMutex, portMAX_DELAY)){
      nowSec = rtc->GetDateTime().TotalSeconds();
      xSemaphoreGive(rtcMutex);
    }

    uint32_t timeToStop = *changePtrs.stopTimerSec - nowSec;
    String timerInfo = *changePtrs.stopTimerSec == 0 ? "Таймер выключен" : "Остановка таймера через " + String(timeToStop / 60.f) + " минут";
    String relayInfo = *relayStatus ? "Полив включен" : "Полив выключен";
    String temperatureInfo = "Температурный порог: " + String(*changePtrs.temperatureThreshold) + ". Актуальная температура: " + String(nowTemp);
    String flowInfo1 = "Поток л/мин: " + String(*lastLitersPerMinute);
    String flowInfo2 = "Верхняя граница потока: " + String(*changePtrs.maxLitersPerMinute);
    String flowInfo3 = "Количество измерений потока до принятия решения: " + String(*changePtrs.ignoreAfterTurningOn);
    String flowInfo4 = *changePtrs.flowExceededMaxValue ? "Полив заблокирован" : "Поток НЕ заблокирован";
    xSemaphoreGive(mutex);
    bot->sendMessage(chatId, intervalsInfo + "\n" + timerInfo + "\n" + temperatureInfo + "\n" + relayInfo + "\n" + flowInfo1 + "\n" + flowInfo2 + "\n" + flowInfo3 + "\n" + flowInfo4);
  }
}

void TelegramManager::processChoose(){
  String msg = bot->messages[messageCount - 1].text;
  String chatId = bot->messages[messageCount - 1].chat_id;

  if(msg.length() != 1){
    bot->sendMessage(chatId,"Кол-во символов более 1!");
    stage = processStage::none;
    return;
  }

  int num = 0;
  try {
    num = std::stoi(msg.c_str());
  }
  catch(std::exception ex){
    bot->sendMessage(chatId,"Неверный формат!");
    stage = processStage::none;
    return;
  }

  if(num < 1 || num > 7){
    bot->sendMessage(chatId,"Цифра должна быть от 1 до 7!");
    stage = processStage::none;
    return;
  }

  switch(num) {
    case 1:
      if(bot->sendMessage(chatId, 
          "Введите время, на которое будет включен полив в формате: \"час:минута\". "
          "Час и минута могут быть более чем 24 и 60, но не более 256.")) {
        stage = processStage::timerInput;
      } else {
        stage = processStage::none;
      }
      break;

    case 2:
      if(bot->sendMessage(chatId,
          "Введите начало и конец включения в формате: \"час:минута-час:минута\". "
          "Если начало будет больше чем конец, то выключение произойдет через 24 часа.")) {  // Исправлена орфография
        stage = processStage::intervalInput;
      } else {
        stage = processStage::none;
      }
      break;

    case 3:
      if(bot->sendMessage(chatId,
          "Введите порог температуры. Можно использовать нецелые числа.")) {
        stage = processStage::temperatureInput;
      } else {
        stage = processStage::none;
      }
      break;

    case 4: {
      xSemaphoreTake(mutex, portMAX_DELAY);
      String intervalsStr;
      const size_t intervalCount = (*changePtrs.intervals).size();  // Исправлен тип
      
      for(size_t i = 0; i < intervalCount; i++) {  // Исправлен тип переменной
        intervalsStr += '\n';
        intervalsStr += i;
        intervalsStr += ") ";
        intervalsStr += (*changePtrs.intervals)[i].toString();
      }
      
      if(bot->sendMessage(chatId, 
          "Введите номер удаляемого интервала:" + intervalsStr)) {
        stage = processStage::delIntervalInput;
      } else {
        stage = processStage::none;
      }
      xSemaphoreGive(mutex);
      break;
    }

    case 5:
      if(bot->sendMessage(chatId,
          "Введите значение скорости потока (л/мин), при котором будет выключен кран "
          "до перезагрузки или до соответствующего запроса.")) {
        stage = processStage::maxFlowInput;
      } else {
        stage = processStage::none;
      }
      break;

    case 6:
      if(bot->sendMessage(chatId,
          "Введите сколько раз будут игнорироваться значения скорости потока после включения. "
          "Замер скорости происходит раз в минуту.")) {
        stage = processStage::ignoreCountInput;
      } else {
        stage = processStage::none;
      }
      break;
    case 7:
      if(bot->sendMessage(chatId,"Полив разблокирован.")) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        *changePtrs.flowExceededMaxValue = false;
        xSemaphoreGive(mutex);
        
        updateLastUpdateTime();
      }
      
      stage = processStage::none;
      
      break;
  }
}

void TelegramManager::processTimer(){
  String msg = bot->messages[messageCount - 1].text;
  String chatId = bot->messages[messageCount - 1].chat_id;

  if(msg.length() < 3){
    stage = processStage::none;
    bot->sendMessage(chatId,"Неверный формат!");
    return;
  }
  uint8_t h,m;
  if(!IntervalTime::parseTime(msg,h,m)){
    stage = processStage::none;
    bot->sendMessage(chatId,"Неверный формат!");
    return;
  }
  if(!bot->sendMessage(chatId,"Таймер установлен на " + String(h) + " часов и " + String(m) + " минут")){
    stage = processStage::none;
    return;
  }

  RtcDateTime now;
  if (xSemaphoreTake(rtcMutex, portMAX_DELAY)){
    now = rtc->GetDateTime();
    xSemaphoreGive(rtcMutex);
  }

  if (xSemaphoreTake(mutex, portMAX_DELAY)){
    *changePtrs.stopTimerSec = now.TotalSeconds() + h * 3600 + m * 60;
    xSemaphoreGive(mutex);
  }

  updateLastUpdateTime();


  stage = processStage::none;
}

void TelegramManager::processInterval(){
  String msg = bot->messages[messageCount - 1].text;
  String chatId = bot->messages[messageCount - 1].chat_id;

  if(msg.length() < 7){
    stage = processStage::none;
    bot->sendMessage(chatId,"Неверный формат!");
    return;
  }
  int delimiterIndex = msg.indexOf('-');
  if(delimiterIndex == -1){
    stage = processStage::none;
    bot->sendMessage(chatId,"Тире не найдено!");
    return;
  }

  String startStr = msg.substring(0, delimiterIndex);
  String stopStr = msg.substring(delimiterIndex + 1);
  uint8_t startH,startM,stopH,stopM;
  if(!IntervalTime::parseTime(startStr, startH, startM) || !IntervalTime::parseTime(stopStr, stopH, stopM)){
    stage = processStage::none;
    bot->sendMessage(chatId,"Неверный формат!");
    return;
  }
  if(startH > 23 || startM > 59 || stopH > 23 || stopM > 59){
    stage = processStage::none;
    bot->sendMessage(chatId,"Час не должен быть больше 23 и минута больше чем 59!");
    return;
  }
  String intervalSetResponse = "Добавлен интревал с " + String(startH) + ":" + String(startM) + " до " + String(stopH) + ":" + String(stopM);
  if(!bot->sendMessage(chatId,intervalSetResponse)){
    stage = processStage::none;
    return;
  }
  IntervalTime inTime;
  inTime.start = startH * 3600 + startM * 60;
  inTime.stop = stopH * 3600 + stopM * 60;
  if (xSemaphoreTake(mutex, portMAX_DELAY)){
    (changePtrs.intervals)->push_back(inTime);
    xSemaphoreGive(mutex);
  }

  updateLastUpdateTime();

  stage = processStage::none;
}

void TelegramManager::processDelete(){
  String msg = bot->messages[messageCount - 1].text;
  String chatId = bot->messages[messageCount - 1].chat_id;

  int index;
  try{
    index = std::stoi(msg.c_str());
  }
  catch(std::exception ex){
    bot->sendMessage(chatId,"Неверный формат!");
    stage = processStage::none;
    return;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  if(index < 0 || index >= (*changePtrs.intervals).size()){
    bot->sendMessage( chatId,"Число должно быть больше 0 и меньше чем " + String((*changePtrs.intervals).size()));
    stage = processStage::none;
    xSemaphoreGive(mutex);
    return;
  }

  if(!bot->sendMessage( chatId, "Интревал " + (*changePtrs.intervals)[index].toString() + " будет удалён")){
    stage = processStage::none;
    xSemaphoreGive(mutex);
    return;
  }

  
  (*changePtrs.intervals).erase((changePtrs.intervals)->begin()+index);
  xSemaphoreGive(mutex);

  updateLastUpdateTime();
  stage = processStage::none;
}

void TelegramManager::processTemperature(){
  String msg = bot->messages[messageCount - 1].text;
  String chatId = bot->messages[messageCount - 1].chat_id;

  msg.replace(',','.');
  float tempVal;
  try{
    tempVal = std::stof(msg.c_str());
  }
  catch(std::exception ex){
    bot->sendMessage(chatId,"Неверный формат!");
    stage = processStage::none;
    return;
  }
  if(!bot->sendMessage(chatId,"Температурный порог установлен на " + String(tempVal))){
    stage = processStage::none;
    return;
  }

  xSemaphoreTake(mutex, portMAX_DELAY);
  *changePtrs.temperatureThreshold = tempVal;
  xSemaphoreGive(mutex);

  updateLastUpdateTime();

  stage = processStage::none;
}

void TelegramManager::processMaxFlow(){
  String msg = bot->messages[messageCount - 1].text;
  String chatId = bot->messages[messageCount - 1].chat_id;

  msg.replace(',','.');
  float maxFlow;
  try{
    maxFlow = std::stof(msg.c_str());
  }
  catch(std::exception ex){
    bot->sendMessage(chatId,"Неверный формат!");
    stage = processStage::none;
    return;
  }
  if(!bot->sendMessage(chatId,"Максимальная скорость потока: " + String(maxFlow))){
    stage = processStage::none;
    return;
  }

  xSemaphoreTake(mutex, portMAX_DELAY);
  *changePtrs.maxLitersPerMinute = maxFlow;
  xSemaphoreGive(mutex);

  updateLastUpdateTime();

  stage = processStage::none;
}

void TelegramManager::processIgnoreCount(){
  String msg = bot->messages[messageCount - 1].text;
  String chatId = bot->messages[messageCount - 1].chat_id;

  int num = 0;
  try {
    num = std::stoi(msg.c_str());
  }
  catch(std::exception ex){
    bot->sendMessage(chatId,"Неверный формат!");
    stage = processStage::none;
    return;
  }

  if(num < 0 || num >= 255){
    bot->sendMessage(chatId,"Значение должно быть положительным и меньше 256");
    stage = processStage::none;
    return;
  }

  if(!bot->sendMessage(chatId,"Количество игнорирований: " + String(num))){
    stage = processStage::none;
    return;
  }

  xSemaphoreTake(mutex, portMAX_DELAY);
  *changePtrs.ignoreAfterTurningOn = num;
  xSemaphoreGive(mutex);

  updateLastUpdateTime();

  stage = processStage::none;
}

void TelegramManager::updateLastUpdateTime(){
  xSemaphoreTake(mutex, portMAX_DELAY);
  xSemaphoreTake(rtcMutex, portMAX_DELAY);
  *lastDataUpdate = rtc->GetDateTime();
  xSemaphoreGive(mutex);
  xSemaphoreGive(rtcMutex);
}
