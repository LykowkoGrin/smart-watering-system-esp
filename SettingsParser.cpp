#include "SettingsParser.h"

SettingsParser::SettingsParser(int eepromAddress){
  this->eepromAddress = eepromAddress;
  loadSettingsFromEEPROM();
}

//серверная

void SettingsParser::raiseServer(const String& serverName){

  server = new WebServer(80);

  server->on("/", [this]() { handleNewClient(); });
  server->on("/submit", HTTP_POST, [this]() { handleForm(); });
  server->begin();

  if (!MDNS.begin(serverName.c_str())){
    Serial.println("Ошибка запуска mDNS!");
  }
  else{
    Serial.printf("mDNS запущен. Доступ по имени http://%s.local\n", serverName.c_str());
    MDNS.addService("http", "tcp", 80);
  }
}

void SettingsParser::tickServer(){
  server->handleClient();
}

void SettingsParser::handleNewClient() {
  // Устанавливаем заголовок с указанием кодировки UTF-8
  server->sendHeader("Content-Type", "text/html; charset=utf-8");

  // HTML-контент с мета-тегом для UTF-8
  String htmlCode = R"(
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset="UTF-8">
    </head>
    <body>
        <h2>Ввод своих настроек</h2>
        <form action="/submit" method="POST">
            Имя сети: <input type="text" name="ssid" value=")" + ssid + R"("><br>
            Пароль: <input type="text" name="password" value=")" + password + R"("><br>
            Токен бота: <input type="text" name="botToken" value=")" + botToken + R"("><br>
            id чата: <input type="text" name="chatId" value=")" + chatId + R"("><br>
            User ID: <input type="text" name="userId" value=")" + userId + R"("><br>
            MQTT Пароль: <input type="password" name="mqttPassword" value=")" + mqttPassword + R"("><br>
            <input type="submit" value="Принять">
        </form>
    </body>
    </html>
)";

  
  server->send(200, "text/html", htmlCode);
}

void SettingsParser::handleForm(){
  if (!server->hasArg("ssid") || !server->hasArg("password") || !server->hasArg("botToken") || !server->hasArg("chatId") || !server->hasArg("userId") || !server->hasArg("mqttPassword")){
    server->send(200, "text/html", "<h2>Ошибка сервера</h2><p>Обновите страницу</p>");
    return;
  }
  String parsedSsid     = server->arg("ssid");
  String parsedPassword = server->arg("password");
  String parsedBotToken = server->arg("botToken");
  String parsedChatId   = server->arg("chatId");
  String parsedUserId   = server->arg("userId");
  String parsedMqttPassword   = server->arg("mqttPassword");

  if(parsedSsid.length() > ssidSize - 2){
    server->sendHeader("Content-Type", "text/html; charset=utf-8");
    String response = "<html><head><meta charset='UTF-8'></head><body>";
    response += "<h2>Имя сети должно быть меньше чем " + String(ssidSize - 2) + "</h2>";
    response += "<form action='/'>"; // Переход на главную страницу
    response += "<button type='submit'>На главную</button>";
    response += "</form>";
    response += "</body></html>";
    server->send(200, "text/html", response);
    //server->send(200, "text/html", "<h2>Имя сети должно быть меньше чем " + String(ssidSize - 2) + "</h2><p>Обновите страницу</p>");
    return;
  }
  if(parsedPassword.length() > passwordSize - 2){
    server->sendHeader("Content-Type", "text/html; charset=utf-8");
    String response = "<html><head><meta charset='UTF-8'></head><body>";
    response += "<h2>Пароль должен быть меньше чем " + String(passwordSize - 2) + "</h2>";
    response += "<form action='/'>"; // Переход на главную страницу
    response += "<button type='submit'>На главную</button>";
    response += "</form>";
    response += "</body></html>";
    server->send(200, "text/html", response);
    //server->send(200, "text/html", "<h2>Пароль должен быть меньше чем " + String(passwordSize - 2) + "</h2><p>Обновите страницу</p>");
    return;
  }
  if(parsedBotToken.length() > botTokenSize - 2){
    server->sendHeader("Content-Type", "text/html; charset=utf-8");
    String response = "<html><head><meta charset='UTF-8'></head><body>";
    response += "<h2>Токен бота должен быть меньше чем " + String(botTokenSize - 2) + "</h2>";
    response += "<form action='/'>"; // Переход на главную страницу
    response += "<button type='submit'>На главную</button>";
    response += "</form>";
    response += "</body></html>";
    server->send(200, "text/html", response);
    //server->send(200, "text/html", "<h2>Токен бота должен быть меньше чем " + String(botTokenSize - 2) + "</h2><p>Обновите страницу</p>");
    return;
  }
  if(parsedChatId.length() > chatIdSize - 2){
    server->sendHeader("Content-Type", "text/html; charset=utf-8");
    String response = "<html><head><meta charset='UTF-8'></head><body>";
    response += "<h2>Чат id должен быть меньше чем " + String(chatIdSize - 2) + "</h2>";
    response += "<form action='/'>"; // Переход на главную страницу
    response += "<button type='submit'>На главную</button>";
    response += "</form>";
    response += "</body></html>";
    server->send(200, "text/html", response);
    return;
  }
  if (parsedUserId.length() > userIdSize - 2) {
    server->sendHeader("Content-Type", "text/html; charset=utf-8");
    String response = "<html><head><meta charset='UTF-8'></head><body>";
    response += "<h2>User ID должен быть меньше чем " + String(userIdSize - 2) + "</h2>";
    response += "<form action='/'>"; // Переход на главную страницу
    response += "<button type='submit'>На главную</button>";
    response += "</form>";
    response += "</body></html>";
    server->send(200, "text/html", response);
    return;
}

if (parsedMqttPassword.length() > mqttPasswordSize - 2) {
    server->sendHeader("Content-Type", "text/html; charset=utf-8");
    String response = "<html><head><meta charset='UTF-8'></head><body>";
    response += "<h2>MQTT Пароль должен быть меньше чем " + String(mqttPasswordSize - 2) + "</h2>";
    response += "<form action='/'>"; // Переход на главную страницу
    response += "<button type='submit'>На главную</button>";
    response += "</form>";
    response += "</body></html>";
    server->send(200, "text/html", response);
    return;
}

  ssid     = parsedSsid;
  password = parsedPassword;
  botToken = parsedBotToken;
  chatId   = parsedChatId;
  userId = parsedUserId;
  mqttPassword = parsedMqttPassword;

  writeSettingsToEEPROM();
  // Подтверждение отправки
  server->sendHeader("Content-Type", "text/html; charset=utf-8");
  String response = "<html><head><meta charset='UTF-8'></head><body>";
  response += "<h2>Данные успешно изменены</h2><p>Перезагрузка...</p>";
  response += "</body></html>";
  server->send(200, "text/html", response);
  ESP.restart();
}


//чтение из EEPROM

void SettingsParser::loadSettingsFromEEPROM(){
  ssid         = readStringFromEEPROM(eepromAddress);
  password     = readStringFromEEPROM(eepromAddress + ssidSize);
  botToken     = readStringFromEEPROM(eepromAddress + ssidSize + passwordSize);
  chatId       = readStringFromEEPROM(eepromAddress + ssidSize + passwordSize + botTokenSize);
  userId       = readStringFromEEPROM(eepromAddress + ssidSize + passwordSize + botTokenSize + chatIdSize);
  mqttPassword = readStringFromEEPROM(eepromAddress + ssidSize + passwordSize + botTokenSize + chatIdSize + userIdSize);
}

void SettingsParser::writeSettingsToEEPROM(){
  writeStringToEEPROM(eepromAddress,                                                                    ssid);
  writeStringToEEPROM(eepromAddress + ssidSize,                                                         password);
  writeStringToEEPROM(eepromAddress + ssidSize + passwordSize,                                          botToken);
  writeStringToEEPROM(eepromAddress + ssidSize + passwordSize + botTokenSize,                           chatId);
  writeStringToEEPROM(eepromAddress + ssidSize + passwordSize + botTokenSize + chatIdSize,              userId);
  writeStringToEEPROM(eepromAddress + ssidSize + passwordSize + botTokenSize + chatIdSize + userIdSize, mqttPassword);
}

void SettingsParser::writeStringToEEPROM(int eepromAddress,const String& str){
  int len = str.length();
  EEPROM.write(eepromAddress, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(eepromAddress + 1 + i, str[i]);
  }
  EEPROM.commit();
}

String SettingsParser::readStringFromEEPROM(int eepromAddress){
  int len = EEPROM.read(eepromAddress);
  char str[len + 1];
  for (int i = 0; i < len; i++) {
    str[i] = EEPROM.read(eepromAddress + 1 + i);
  }
  str[len] = '\0';
  return String(str);
}

int SettingsParser::getReservedSizeEEPROM(){
  return ssidSize + passwordSize + botTokenSize + chatIdSize + userIdSize + mqttPasswordSize;
}

//гэттеры

String SettingsParser::getSSID(){
  return ssid;
}

String SettingsParser::getPassword(){
  return password;
}

String SettingsParser::getBotToken(){
  return botToken;
}

String SettingsParser::getChatId(){
  return chatId;
}
String SettingsParser::getUserId(){
  return userId;
}
String SettingsParser::getMqttPassword(){
  return mqttPassword;
}


const int SettingsParser::ssidSize = 27;
const int SettingsParser::passwordSize = 20;
const int SettingsParser::botTokenSize = 50;
const int SettingsParser::chatIdSize = 20;
const int SettingsParser::userIdSize = 20;
const int SettingsParser::mqttPasswordSize = 20;

