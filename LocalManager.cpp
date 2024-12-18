#include "LocalManager.h"

LocalManager::LocalManager(const ConstructPtrs& params){
  this->rtc = params.rtc;
  this->relayStatus = params.relayStatus;
  this->bmp = params.bmp;
}

void LocalManager::raiseServer(const String& serverName){
  server = new WebServer(80);

  server->on("/",                       [this]() { handleNewClient();  });
  server->on("/submit", HTTP_POST,      [this]() { handleSubmit();     });
  server->on("/add_item", HTTP_POST,    [this]() { handleAddItem();    });
  server->on("/delete_item", HTTP_POST, [this]() { handleDeleteItem(); });

  server->begin();

  if (!MDNS.begin(serverName.c_str())){
    Serial.println("Ошибка запуска mDNS!");
  }
  else{
    Serial.printf("mDNS запущен. Доступ по имени http://%s.local\n", serverName.c_str());
    MDNS.addService("http", "tcp", 80);
  }
}

void LocalManager::handleNewClient() {

  server->sendHeader("Content-Type", "text/html; charset=utf-8");

  uint32_t nowSec = rtc->GetDateTime().TotalSeconds();
  String stopTimerValue = *stopTimerSec ? String((int)((*stopTimerSec - nowSec) / 60)) : "";

  String html = "<html><head><meta charset='UTF-8'></head><body>";
  
  html += (*relayStatus == true) ? "<h2><b>Полив включен</b></h2>" : "<h2><b>Полив выключен</b></h2>";
  html += "<h2><b>Температура:" + String(bmp->readTemperature()) + "</b></h2>";
  
  html += "<h2>Настройки</h2>";
  html += "<form action='/submit' method='POST'>";
  html += "Минуты до остановки таймера: <input type='text' name='timer_end' value='" + stopTimerValue + "'><br>";
  html += "Температурный порог(пример 30.1): <input type='text' name='temperature_threshold' value='" + String(*temperatureThreshold) + "'><br>";
  html += "<input type='submit' value='Сохранить'>";
  html += "</form><br>";

  html += "<h2>Список интервалов</h2>";
  html += "<ul>";
  for (size_t i = 0; i < intervals->size(); ++i) {
      html += "<li>" + (*intervals)[i].toString();
      html += "<form style='display:inline;' action='/delete_item' method='POST'>";
      html += "<input type='hidden' name='index' value='" + String(i) + "'>";
      html += "<input type='submit' value='Удалить'>";
      html += "</form>";
      html += "</li>";
  }
  html += "</ul>";

  html += "<form action='/add_item' method='POST'>";
  html += "Новый интервал: <input type='text' name='new_item'>";
  html += "<input type='submit' value='Добавить'>";
  html += "</form>";

  html += "</body></html>";

  server->send(200, "text/html", html);
}

void LocalManager::handleSubmit(){
  if (server->hasArg("timer_end")) {
    String minStr = server->arg("timer_end");
    int parsedMin;
    try{ 
      parsedMin = std::stoi(minStr.c_str()); 
    }
    catch(std::exception ex){ 
      server->sendHeader("Content-Type", "text/html; charset=utf-8");
      String response = "<html><head><meta charset='UTF-8'></head><body>";
      response += "<h2>Неверный формат минут до выключения таймера</h2>";
      response += "<form action='/'>"; // Переход на главную страницу
      response += "<button type='submit'>На главную</button>";
      response += "</form>";
      response += "</body></html>";
      server->send(200, "text/html", response);
      //server->send(200, "text/html", "<h2>Неверный формат минут до выключения таймера</h2><p>Обновите страницу</p>"); 
      return;
    }
    *stopTimerSec = rtc->GetDateTime().TotalSeconds() + parsedMin * 60;
  }
  if (server->hasArg("temperature_threshold")) {
    String tempStr = server->arg("temperature_threshold");
    float parsedTemp;
    try{ 
      parsedTemp= std::stof(tempStr.c_str()); 
    }
    catch(std::exception ex){ 
      server->sendHeader("Content-Type", "text/html; charset=utf-8");
      String response = "<html><head><meta charset='UTF-8'></head><body>";
      response += "<h2>Неверный формат температуры</h2>";
      response += "<form action='/'>"; // Переход на главную страницу
      response += "<button type='submit'>На главную</button>";
      response += "</form>";
      response += "</body></html>";
      server->send(200, "text/html", response);
      //server->send(200, "text/html", "<h2>Неверный формат температуры</h2><p>Обновите страницу</p>"); 
      return;
    }
    *temperatureThreshold = parsedTemp;
  }

  server->sendHeader("Location", "/");
  server->send(303);
}

void LocalManager::handleAddItem(){
  if (server->hasArg("new_item")) {
    String intervalStr = server->arg("new_item");
    uint8_t h1,m1,h2,m2;

    int delimiterIndex = intervalStr.indexOf('-');
    if(delimiterIndex == -1)
    { 
      server->sendHeader("Content-Type", "text/html; charset=utf-8");
      String response = "<html><head><meta charset='UTF-8'></head><body>";
      response += "<h2>Не найдено тире в добавляемом интервале</h2>";
      response += "<form action='/'>"; // Переход на главную страницу
      response += "<button type='submit'>На главную</button>";
      response += "</form>";
      response += "</body></html>";
      server->send(200, "text/html", response);
      //server->send(200, "text/html", "<h2>Не найдено тире в добавляемом интервале</h2><p>Обновите страницу</p>"); 
      return; 
    }

    String startStr = intervalStr.substring(0, delimiterIndex);
    String stopStr = intervalStr.substring(delimiterIndex + 1);
    if(!IntervalTime::parseTime(startStr,h1,m1) || !IntervalTime::parseTime(stopStr,h2,m2)) {
      server->sendHeader("Content-Type", "text/html; charset=utf-8");
      String response = "<html><head><meta charset='UTF-8'></head><body>";
      response += "<h2>Неправильный формат добавляемого интервала</h2>";
      response += "<form action='/'>"; // Переход на главную страницу
      response += "<button type='submit'>На главную</button>";
      response += "</form>";
      response += "</body></html>";
      server->send(200, "text/html", response);
      //server->send(200, "text/html", "<h2>Неправильный формат добавляемого интервала</h2><p>Обновите страницу</p>"); 
      return; 
    }
    IntervalTime inTime;
    inTime.start = h1 * 3600 + m1 * 60;
    inTime.stop = h2 * 3600 + m2 * 60;

    intervals->push_back(inTime);
  }
  server->sendHeader("Location", "/");
  server->send(303);
}

void LocalManager::handleDeleteItem(){
  if (server->hasArg("index")) {
    int index = server->arg("index").toInt();

    if (index >= 0 && index < intervals->size()) {
        intervals->erase(intervals->begin() + index);
    }
    else{
      server->sendHeader("Content-Type", "text/html; charset=utf-8");
      String response = "<html><head><meta charset='UTF-8'></head><body>";
      response += "<h2>Индекс меньше 0 или больше чем " + String(intervals->size()) + "</h2>";
      response += "<form action='/'>"; // Переход на главную страницу
      response += "<button type='submit'>На главную</button>";
      response += "</form>";
      response += "</body></html>";
      server->send(200, "text/html", response);
      return;
    }
  }
    
  server->sendHeader("Location", "/");
  server->send(303);
}

void LocalManager::tickServer(const ChangePtrs& params){
  this->intervals = params.intervals;
  this->stopTimerSec = params.stopTimerSec;
  this->temperatureThreshold = params.temperatureThreshold;
  server->handleClient();
}





