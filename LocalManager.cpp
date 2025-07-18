#include "LocalManager.h"

LocalManager::LocalManager(const ConstructPtrs& params){
  this->rtc = params.rtc;
  this->relayStatus = params.relayStatus;
  this->lastLitersPerMinute = params.lastLitersPerMinute;
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

    server->on("/m2m", HTTP_GET, [this](AsyncWebServerRequest *request) {
      handleM2M(request);
    });

    server->on("/m2me", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleM2ME(request);
    });

    server->on("/set_system_time", HTTP_POST, [this](AsyncWebServerRequest *request) {
    handleSetSystemTime(request);
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

    server->on("/reset_flow", HTTP_POST, [this](AsyncWebServerRequest *request) {
      handleResetFlow(request);
    });

    server->on("/humidity", HTTP_POST, [this](AsyncWebServerRequest *request) {
      handleHumidity(request);
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
  html += "<h2><b>Температура: " + String(nowTemp) + " °C</b></h2>";
  html += "<h2><b>Текущий расход: " + String(*lastLitersPerMinute) + " л/мин</b></h2>";
  html += "<h2><b>Превышение расхода: " + String(*flowExceededMaxValue ? "ДА" : "НЕТ") + "</b></h2>";
  html += "<h2><b>Влажность почвы: " + String(*humidity) + "</b></h2>";
  html += "<form action='/reset_flow' method='POST'>";
  html += "<input type='submit' value='Сбросить превышение'>";
  html += "</form>";
  
  html += "<h2>Настройки</h2>";
  html += "<form action='/submit' method='POST'>";
  html += "Макс. расход (л/мин): <input type='text' name='max_liters' value='" + String(*maxLitersPerMinute) + "'><br>";
  html += "Игнорировать показания после включения (раз): <input type='text' name='ignore_after_on' value='" + String(*ignoreAfterTurningOn) + "'><br>";
  html += "Минуты до остановки таймера: <input type='text' name='timer_end' value='" + stopTimerValue + "'><br>";
  html += "Температурный порог (°C): <input type='text' name='temperature_threshold' value='" + String(*temperatureThreshold) + "'><br>";
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

  // В методе handleNewClient добавить новую форму (после существующих форм)
  html += "<h2>Обновление системного времени</h2>";
  html += "<form action='/set_system_time' method='POST'>";
  html += "UNIX время: <input type='text' name='unix_time' value='" + String(nowSec) + "'><br>";
  html += "<input type='submit' value='Установить'>";
  html += "</form>";

  html += "</body></html>";

  xSemaphoreGive(mutex);


  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", html);
  response->addHeader("Content-Type", "text/html; charset=utf-8");
  request->send(response);
}

void LocalManager::handleSubmit(AsyncWebServerRequest *request){

  if (request->hasArg("max_liters")) {
    String maxStr = request->arg("max_liters");
    try {
      float parsedMax = std::stof(maxStr.c_str());
      xSemaphoreTake(mutex, portMAX_DELAY);
      *maxLitersPerMinute = parsedMax;
      xSemaphoreGive(mutex);
      updateLastUpdateTime();
    } 
    catch(std::exception& ex) {
      handleError(request, "Неверный формат максимального расхода");
      return;
    }
  }

  // Обработка ignore_after_on
  if (request->hasArg("ignore_after_on")) {
    String ignoreStr = request->arg("ignore_after_on");
    try {
      int parsedIgnore = std::stoi(ignoreStr.c_str());
      xSemaphoreTake(mutex, portMAX_DELAY);
      *ignoreAfterTurningOn = parsedIgnore;
      xSemaphoreGive(mutex);
      updateLastUpdateTime();
    } 
    catch(std::exception& ex) {
      handleError(request, "Неверный формат времени игнорирования");
      return;
    }
  }

  if (request->hasArg("timer_end")) {
    String minStr = request->arg("timer_end");
    if (minStr.isEmpty()) {
        // Пустая строка - устанавливаем таймер в 0 (выключаем)
        xSemaphoreTake(mutex, portMAX_DELAY);
        *stopTimerSec = 0; // 0 означает, что таймер не активен
        xSemaphoreGive(mutex);
        updateLastUpdateTime();
    } else {
        // Обработка числового значения
        int parsedMin;
        try{ 
            parsedMin = std::stoi(minStr.c_str()); 
        }
        catch(std::exception ex){ 
            String response = "<html><head><meta charset='UTF-8'></head><body>";
            response += "<h2>Неверный формат минут до выключения таймера</h2>";
            response += "<form action='/'>";
            response += "<button type='submit'>На главную</button>";
            response += "</form>";
            response += "</body></html>";

            AsyncWebServerResponse *responseWeb = request->beginResponse(200, "text/plain", response);
            responseWeb->addHeader("Content-Type", "text/html; charset=utf-8");
            request->send(responseWeb);
            return;
        }

        xSemaphoreTake(rtcMutex, portMAX_DELAY);
        xSemaphoreTake(mutex, portMAX_DELAY);
        *stopTimerSec = rtc->GetDateTime().TotalSeconds() + parsedMin * 60;
        xSemaphoreGive(rtcMutex);
        xSemaphoreGive(mutex);
        updateLastUpdateTime();
    }
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
    updateLastUpdateTime();
  }


  AsyncWebServerResponse *response = request->beginResponse(303);  // Код 303 (See Other)
  response->addHeader("Location", "/");  // Заголовок для редиректа
  request->send(response);
}

void LocalManager::handleError(AsyncWebServerRequest *request, const String& message) {
  String response = "<html><head><meta charset='UTF-8'></head><body>";
  response += "<h2>" + message + "</h2>";
  response += "<form action='/'><button type='submit'>На главную</button></form>";
  response += "</body></html>";
  
  AsyncWebServerResponse *responseWeb = request->beginResponse(200, "text/plain", response);
  responseWeb->addHeader("Content-Type", "text/html; charset=utf-8");
  request->send(responseWeb);
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
        updateLastUpdateTime();
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
            updateLastUpdateTime();

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

void LocalManager::handleResetFlow(AsyncWebServerRequest *request) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    *flowExceededMaxValue = false; // Сбрасываем флаг
    xSemaphoreGive(mutex);
    updateLastUpdateTime();
    
    // Редирект обратно на главную
    AsyncWebServerResponse *response = request->beginResponse(303);
    response->addHeader("Location", "/");
    request->send(response);
}

void LocalManager::handleSetSystemTime(AsyncWebServerRequest *request) {
    if (request->hasArg("unix_time")) {
        String timeStr = request->arg("unix_time");
        
        // Проверка на пустую строку
        if (timeStr.isEmpty()) {
            // Редирект без изменений
            AsyncWebServerResponse *response = request->beginResponse(303);
            response->addHeader("Location", "/");
            request->send(response);
            return;
        }
        
        try {
            uint32_t newTime = std::stoul(timeStr.c_str());
            
            // Установка нового времени в RTC
            if (xSemaphoreTake(rtcMutex, portMAX_DELAY)) {
                rtc->SetDateTime(RtcDateTime(newTime));
                xSemaphoreGive(rtcMutex);
                
                // Сброс времен обновления
                xSemaphoreTake(mutex, portMAX_DELAY);
                *lastDataUpdate = RtcDateTime(0);  // Сброс в 0
                *lastHumidityUpdate = RtcDateTime(0);  // Сброс в 0
                xSemaphoreGive(mutex);
                
                Serial.printf("System time updated to: %u\n", newTime);
            }
        }
        catch (std::exception& e) {
            Serial.println("Error converting time string");
        }
    }
    
    // Редирект на главную
    AsyncWebServerResponse *response = request->beginResponse(303);
    response->addHeader("Location", "/");
    request->send(response);
}

void LocalManager::handleHumidity(AsyncWebServerRequest *request) {
  if (!request->hasParam("value", true)) {
    request->send(400, "text/plain", "Missing value parameter");
    return;
  }

  const AsyncWebParameter* p = request->getParam("value", true);
  
  if (p->value().isEmpty()) {
    request->send(400, "text/plain", "Empty value parameter");
    return;
  }

  int humidityValue = p->value().toInt();
  
  xSemaphoreTake(mutex, portMAX_DELAY);
  *humidity = humidityValue;
  xSemaphoreGive(mutex);
  
  xSemaphoreTake(mutex, portMAX_DELAY);
  xSemaphoreTake(rtcMutex, portMAX_DELAY);
  *lastHumidityUpdate = rtc->GetDateTime();
  xSemaphoreGive(mutex);
  xSemaphoreGive(rtcMutex);

  request->send(200, "text/plain", "Humidity updated");
}

void LocalManager::handleM2M(AsyncWebServerRequest *request) {
    // Создаем JSON-документ
    DynamicJsonDocument doc(2048);
    
    // Получаем текущее время системы
    uint32_t currentTimeSec = 0;
    if (xSemaphoreTake(rtcMutex, portMAX_DELAY) == pdTRUE) {
        currentTimeSec = rtc->GetDateTime().TotalSeconds();
        xSemaphoreGive(rtcMutex);
    }
    doc["currentTime"] = currentTimeSec;  // Добавляем текущее время
    
    // Захватываем мьютекс для безопасного доступа к данным
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        // 1. Интервалы полива
        JsonArray intervalsArr = doc.createNestedArray("intervals");
        for (const auto& interval : *intervals) {
            JsonObject intervalObj = intervalsArr.createNestedObject();
            intervalObj["start"] = interval.start;
            intervalObj["stop"] = interval.stop;
        }
        
        // 2. Основные параметры
        doc["stopTimerSec"] = *stopTimerSec;
        doc["temperatureThreshold"] = *temperatureThreshold;
        doc["maxLitersPerMinute"] = *maxLitersPerMinute;
        doc["ignoreAfterTurningOn"] = *ignoreAfterTurningOn;
        doc["flowExceededMaxValue"] = *flowExceededMaxValue;
        
        // 3. Временные метки (преобразуем в Unix-время)
        doc["lastDataUpdate"] = lastDataUpdate->TotalSeconds();
        doc["lastHumidityUpdate"] = lastHumidityUpdate->TotalSeconds();
        
        // 4. Показатели датчиков
        doc["humidity"] = *humidity;
        doc["relayStatus"] = *relayStatus;
        doc["lastLitersPerMinute"] = *lastLitersPerMinute;
        
        xSemaphoreGive(mutex);
        
        // Сериализуем JSON в строку
        String jsonResponse;
        serializeJson(doc, jsonResponse);
        
        // Отправляем ответ
        request->send(200, "application/json", jsonResponse);
    } else {
        request->send(500, "application/json", "{\"error\":\"mutex_timeout\"}");
    }
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
  this->maxLitersPerMinute = params.maxLitersPerMinute;
  this->ignoreAfterTurningOn = params.ignoreAfterTurningOn;
  this->flowExceededMaxValue = params.flowExceededMaxValue;
  this->lastDataUpdate = params.lastDataUpdate;
  this->lastHumidityUpdate = params.lastHumidityUpdate;
  this->humidity = params.humidity;
  this->mutex = params.mutex;
}

void LocalManager::updateLastUpdateTime(){
  xSemaphoreTake(mutex, portMAX_DELAY);
  xSemaphoreTake(rtcMutex, portMAX_DELAY);
  *lastDataUpdate = rtc->GetDateTime();
  xSemaphoreGive(mutex);
  xSemaphoreGive(rtcMutex);
}

void LocalManager::handleM2ME(AsyncWebServerRequest *request) {
    Serial.println("\n===== [M2ME] HANDLER STARTED =====");
    Serial.printf("[M2ME] Content-Length: %d\n", request->contentLength());
    Serial.printf("[M2ME] Content-Type: %s\n", request->contentType().c_str());
    Serial.printf("[M2ME] Arguments count: %d\n", request->args());

    // Выводим все аргументы
    for (int i = 0; i < request->args(); i++) {
        Serial.printf("[M2ME] Arg %d: name='%s', value='%s'\n", 
                     i, 
                     request->argName(i).c_str(),
                     request->arg(i).c_str());
    }

    // Основной метод: получаем параметр "body"
    String jsonStr;
    if (request->hasParam("body", true)) {
        jsonStr = request->getParam("body", true)->value();
        Serial.println("[M2ME] Using 'body' parameter");
    }
    // Альтернативный метод: параметр "plain"
    else if (request->hasParam("plain", true)) {
        jsonStr = request->getParam("plain", true)->value();
        Serial.println("[M2ME] Using 'plain' parameter");
    }
    else {
        Serial.println("[M2ME] Error: missing 'body' parameter");
        request->send(400, "application/json", "{\"error\":\"missing_body_parameter\"}");
        return;
    }

    // Проверяем, получили ли мы данные
    if (jsonStr.length() == 0) {
        Serial.println("[M2ME] Error: body parameter is empty");
        request->send(400, "application/json", "{\"error\":\"empty_body\"}");
        return;
    }

    Serial.print("[M2ME] Received JSON: ");
    Serial.println(jsonStr);
    Serial.print("[M2ME] JSON length: ");
    Serial.println(jsonStr.length());

    // Проверяем размер JSON
    if (jsonStr.length() > 512) {
        Serial.println("[M2ME] Error: payload too large");
        request->send(413, "application/json", "{\"error\":\"payload_too_large\"}");
        return;
    }

    // Парсим JSON
    Serial.println("[M2ME] Parsing JSON...");
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        Serial.printf("[M2ME] JSON parse error: %s\n", error.c_str());
        String errMsg = "{\"error\":\"parse_error\",\"details\":\"" + String(error.c_str()) + "\"}";
        request->send(400, "application/json", errMsg);
        return;
    }

    Serial.println("[M2ME] JSON parsed successfully");

    // Обработка данных
    bool updateNeeded = false;
    String responseMsg = "{";

    // Захватываем мьютекс перед изменением данных
    Serial.println("[M2ME] Waiting for mutex...");
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        Serial.println("[M2ME] Mutex taken");

        // 1. Обновление интервалов
        if (doc.containsKey("intervals")) {
            Serial.println("[M2ME] Updating intervals...");
            JsonArray intervalsArr = doc["intervals"];
            intervals->clear();
            for (JsonObject interval : intervalsArr) {
                IntervalTime newInterval;
                newInterval.start = interval["start"].as<uint32_t>();
                newInterval.stop = interval["stop"].as<uint32_t>();
                intervals->push_back(newInterval);
            }
            updateNeeded = true;
            responseMsg += "\"intervals\":\"updated\",";
        }

        // 2. Обновление числовых параметров
        const char* fields[] = {
            "stopTimerSec", "temperatureThreshold", 
            "maxLitersPerMinute", "ignoreAfterTurningOn"
        };
        
        for (const char* field : fields) {
            if (doc.containsKey(field)) {
                Serial.printf("[M2ME] Updating field '%s'\n", field);
                if (strcmp(field, "stopTimerSec") == 0) *stopTimerSec = doc[field].as<uint32_t>();
                else if (strcmp(field, "temperatureThreshold") == 0) *temperatureThreshold = doc[field].as<float>();
                else if (strcmp(field, "maxLitersPerMinute") == 0) *maxLitersPerMinute = doc[field].as<float>();
                else if (strcmp(field, "ignoreAfterTurningOn") == 0) *ignoreAfterTurningOn = doc[field].as<uint8_t>();
                
                updateNeeded = true;
                responseMsg += "\"" + String(field) + "\":\"updated\",";
            }
        }

        // 3. Обновление флагов
        if (doc.containsKey("flowExceededMaxValue")) {
            Serial.println("[M2ME] Updating flowExceededMaxValue");
            *flowExceededMaxValue = doc["flowExceededMaxValue"].as<bool>();
            updateNeeded = true;
            responseMsg += "\"flowExceededMaxValue\":\"updated\",";
        }

        // 4. Обновление влажности
        if (doc.containsKey("humidity")) {
            Serial.println("[M2ME] Updating humidity");
            *humidity = doc["humidity"].as<int>();
            if (xSemaphoreTake(rtcMutex, portMAX_DELAY) == pdTRUE) {
                *lastHumidityUpdate = rtc->GetDateTime();
                xSemaphoreGive(rtcMutex);
            }
            responseMsg += "\"humidity\":\"updated\",";
        }

        // Финализируем ответ
        if (responseMsg.endsWith(",")) {
            responseMsg.remove(responseMsg.length() - 1);
        }
        responseMsg += "}";
        
        xSemaphoreGive(mutex);
        Serial.println("[M2ME] Mutex released");
        
        Serial.print("[M2ME] Sending response: ");
        Serial.println(responseMsg);
        request->send(200, "application/json", responseMsg);
    } else {
        Serial.println("[M2ME] Mutex timeout");
        request->send(500, "application/json", "{\"error\":\"mutex_timeout\"}");
    }
    Serial.println("===== [M2ME] HANDLER FINISHED =====");
}

