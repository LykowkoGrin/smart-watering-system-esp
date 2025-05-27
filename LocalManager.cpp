#include "LocalManager.h"

LocalManager::LocalManager(const ConstructPtrs& params){
  this->rtc = params.rtc;
  this->relayStatus = params.relayStatus;
  this->bmp = params.bmp;
  this->bmpMutex = params.bmpMutex;
  this->rtcMutex = params.rtcMutex;

  
}

void LocalManager::raiseServer(const String& serverName){
  //if (server != nullptr) delete server;

    //if (server != nullptr) {
    //    server->end();
    //    delete server;
    //    server = nullptr;
   // }


    server = new AsyncWebServer(80);

    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleNewClient(request);
    });

    server->on("/submit", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSubmit(request);
    });

    server->on("/add_item", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleAddItem(request);
    });

    server->on("/delete_item", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleDeleteItem(request);
    });

    server->begin();

    if (!MDNS.begin(serverName.c_str())) {
        Serial.println("Ошибка запуска mDNS!");
    } else {
        Serial.printf("mDNS запущен. Доступ по имени http://%s.local\n", serverName.c_str());
        MDNS.addService("http", "tcp", 80);
    }
}

void LocalManager::handleNewClient(AsyncWebServerRequest *request) {

  uint32_t nowSec;
  float nowTemp;

  if (xSemaphoreTake(rtcMutex, portMAX_DELAY)){
    nowSec = rtc->GetDateTime().TotalSeconds();
    xSemaphoreGive(rtcMutex);
  }

  if (xSemaphoreTake(bmpMutex, portMAX_DELAY)){
    nowTemp = bmp->readTemperature();
    xSemaphoreGive(bmpMutex);
  }

  xSemaphoreTake(mutex, portMAX_DELAY);

  String stopTimerValue = *stopTimerSec ? String((int)((*stopTimerSec - nowSec) / 60)) : "";

  String html = "<html><head><meta charset='UTF-8'></head><body>";
  
  html += (*relayStatus == true) ? "<h2><b>Полив включен</b></h2>" : "<h2><b>Полив выключен</b></h2>";
  html += "<h2><b>Температура:" + String(nowTemp) + "</b></h2>";
  
  html += "<h2>Настройки</h2>";
  html += "<form action='/submit' method='POST'>";
  html += "Минуты до остановки таймера: <input type='text' name='timer_end' value='" + stopTimerValue + "'><br>";
  html += "Температурный порог(пример 30.1): <input type='text' name='temperature_threshold' value='" + String(*temperatureThreshold) + "'><br>";
  html += "<input type='submit' value='Сохранить'>";
  html += "</form><br>";

  html += "<h2>Список интервалов включения</h2>";
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

  xSemaphoreGive(mutex);


  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", html);
  response->addHeader("Content-Type", "text/html; charset=utf-8");
  request->send(response);
}

void LocalManager::handleSubmit(AsyncWebServerRequest *request){
  if (request->hasArg("timer_end")) {
    String minStr = request->arg("timer_end");
    int parsedMin;
    try{ 
      parsedMin = std::stoi(minStr.c_str()); 
    }
    catch(std::exception ex){ 
      String response = "<html><head><meta charset='UTF-8'></head><body>";
      response += "<h2>Неверный формат минут до выключения таймера</h2>";
      response += "<form action='/'>"; // Переход на главную страницу
      response += "<button type='submit'>На главную</button>";
      response += "</form>";
      response += "</body></html>";
      AsyncWebServerResponse *responseWeb = request->beginResponse(200, "text/html", response);
      responseWeb->addHeader("Content-Type", "text/html; charset=utf-8");
      request->send(responseWeb);
      //request->send(200, "text/html", "<h2>Неверный формат минут до выключения таймера</h2><p>Обновите страницу</p>"); 
      return;
    }

    xSemaphoreTake(rtcMutex, portMAX_DELAY);
    xSemaphoreTake(mutex, portMAX_DELAY);
    *stopTimerSec = rtc->GetDateTime().TotalSeconds() + parsedMin * 60;
    xSemaphoreGive(rtcMutex);
    xSemaphoreGive(mutex);
  }
  if (request->hasArg("temperature_threshold")) {
    String tempStr = request->arg("temperature_threshold");
    float parsedTemp;
    try{ 
      parsedTemp= std::stof(tempStr.c_str()); 
    }
    catch(std::exception ex){ 
  
      String response = "<html><head><meta charset='UTF-8'></head><body>";
      response += "<h2>Неверный формат температуры</h2>";
      response += "<form action='/'>"; // Переход на главную страницу
      response += "<button type='submit'>На главную</button>";
      response += "</form>";
      response += "</body></html>";

      AsyncWebServerResponse *responseWeb = request->beginResponse(200, "text/plain", response);
      responseWeb->addHeader("Content-Type", "text/html; charset=utf-8");
      request->send(responseWeb);
      //request->send(200, "text/html", "<h2>Неверный формат температуры</h2><p>Обновите страницу</p>"); 
      return;
    }
    xSemaphoreTake(mutex, portMAX_DELAY);
    *temperatureThreshold = parsedTemp;
    xSemaphoreGive(mutex);
  }


  AsyncWebServerResponse *response = request->beginResponse(303);  // Код 303 (See Other)
  response->addHeader("Location", "/");  // Заголовок для редиректа
  request->send(response);
}

void LocalManager::handleAddItem(AsyncWebServerRequest *request) {
    if (request->hasArg("new_item")) {
        String intervalStr = request->arg("new_item");
        uint8_t h1, m1, h2, m2;

        int delimiterIndex = intervalStr.indexOf('-');
        if (delimiterIndex == -1) {
            // Ошибка: отсутствует тире
            String responseHTML = "<html><head><meta charset='UTF-8'></head><body>";
            responseHTML += "<h2>Не найдено тире в добавляемом интервале</h2>";
            responseHTML += "<form action='/'><button type='submit'>На главную</button></form>";
            responseHTML += "</body></html>";

            request->send(200, "text/html", responseHTML);
            return;
        }

        String startStr = intervalStr.substring(0, delimiterIndex);
        String stopStr = intervalStr.substring(delimiterIndex + 1);
        if (!IntervalTime::parseTime(startStr, h1, m1) || !IntervalTime::parseTime(stopStr, h2, m2)) {
            // Ошибка: неверный формат времени
            String responseHTML = "<html><head><meta charset='UTF-8'></head><body>";
            responseHTML += "<h2>Неправильный формат добавляемого интервала</h2>";
            responseHTML += "<form action='/'><button type='submit'>На главную</button></form>";
            responseHTML += "</body></html>";

            request->send(200, "text/html", responseHTML);
            return;
        }

        IntervalTime inTime;
        inTime.start = h1 * 3600 + m1 * 60;
        inTime.stop = h2 * 3600 + m2 * 60;

        // Добавляем интервал в список
        xSemaphoreTake(mutex, portMAX_DELAY);
        intervals->push_back(inTime);
        xSemaphoreGive(mutex);
    }

    // Редирект на главную
    AsyncWebServerResponse *response = request->beginResponse(303);
    response->addHeader("Location", "/");
    request->send(response);
}

void LocalManager::handleDeleteItem(AsyncWebServerRequest *request) {
    if (request->hasArg("index")) {
        int index = request->arg("index").toInt();

        xSemaphoreTake(mutex, portMAX_DELAY);
        if (index >= 0 && index < intervals->size()) {
            intervals->erase(intervals->begin() + index);
            xSemaphoreGive(mutex);

            // Редирект на главную страницу
            AsyncWebServerResponse *response = request->beginResponse(303);
            response->addHeader("Location", "/");
            request->send(response);
            return;
        } 
        
        // Ошибка: индекс вне диапазона
        String responseHTML = "<html><head><meta charset='UTF-8'></head><body>";
        responseHTML += "<h2>Индекс меньше 0 или больше чем " + String(intervals->size()) + "</h2>";
        responseHTML += "<form action='/'>";
        responseHTML += "<button type='submit'>На главную</button>";
        responseHTML += "</form>";
        responseHTML += "</body></html>";

        xSemaphoreGive(mutex);
        request->send(200, "text/html", responseHTML);
        return;
    }

    xSemaphoreGive(mutex);

    // Если index не передан, просто редиректим на главную
    AsyncWebServerResponse *response = request->beginResponse(303);
    response->addHeader("Location", "/");
    request->send(response);
}

/*
void LocalManager::tickServer(const ChangePtrs& params){
  this->intervals = params.intervals;
  this->stopTimerSec = params.stopTimerSec;
  this->temperatureThreshold = params.temperatureThreshold;
  this->mutex = params.mutex;

  Serial.println("this->mutex = params.mutex;");

  //if(server != nullptr) server->handleClient();

  Serial.println("if(server != nullptr) server->handleClient();");
}
*/
void LocalManager::setChangePtrs(const ChangePtrs& params){
  this->intervals = params.intervals;
  this->stopTimerSec = params.stopTimerSec;
  this->temperatureThreshold = params.temperatureThreshold;
  this->mutex = params.mutex;
}



