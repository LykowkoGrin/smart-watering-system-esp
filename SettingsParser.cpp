#include "SettingsParser.h"

SettingsParser::SettingsParser(int eepromAddress,bool writeOnly){
  this->eepromAddress = eepromAddress;
  if(!writeOnly) loadSettingsFromEEPROM();
}

//серверная+

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
            <h3 style="color:black;">ручная настройка подключения(необязательно)</h3>

            IP: <input type="text" name="ip" value=")" + staticIP.toString() + R"("><br>
            SUBNET: <input type="text" name="subnet" value=")" + subnet.toString() + R"("><br>
            GATEWAY: <input type="text" name="gateway" value=")" + gateway.toString() + R"("><br>
            DNS0: <input type="text" name="dns0" value=")" + primaryDNS.toString() + R"("><br>
            DNS1: <input type="text" name="dns1" value=")" + secondaryDNS.toString() + R"("><br>
            <input type="submit" value="Принять">
        </form>
    </body>
    </html>
)";

  
  server->send(200, "text/html", htmlCode);
}

void SettingsParser::handleForm(){

  //Dont watch pls
  if (!server->hasArg("ssid") || !server->hasArg("password") || !server->hasArg("botToken") ||
      !server->hasArg("chatId") || !server->hasArg("userId") || !server->hasArg("mqttPassword") ||
      !server->hasArg("ip") || !server->hasArg("subnet") || !server->hasArg("gateway") ||
      !server->hasArg("dns0") || !server->hasArg("dns1")
   ){
    server->send(200, "text/html", "<h2>Ошибка сервера</h2><p>Обновите страницу</p>");
    return;
  }
  String parsedSsid     = server->arg("ssid");
  String parsedPassword = server->arg("password");
  String parsedBotToken = server->arg("botToken");
  String parsedChatId   = server->arg("chatId");
  String parsedUserId   = server->arg("userId");
  String parsedMqttPassword   = server->arg("mqttPassword");

  String parsedStaticIP = server->arg("ip");
  String parsedSubnet = server->arg("subnet");
  String parsedGateway = server->arg("gateway");
  String parsedPrimaryDNS = server->arg("dns0");
  String parsedSecondaryDNS = server->arg("dns1");

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

  IPAddress convertedStaticIP, convertedGateway, convertedSubnet, convertedPrimaryDNS,convertedSecondaryDNS;
  if(!convertStrToIP(parsedStaticIP,convertedStaticIP) || !convertStrToIP(parsedGateway,convertedGateway) || !convertStrToIP(parsedSubnet,convertedSubnet) ||
     !convertStrToIP(parsedPrimaryDNS,convertedPrimaryDNS) || !convertStrToIP(parsedSecondaryDNS,convertedSecondaryDNS))
      convertedStaticIP = convertedGateway = convertedSubnet = convertedPrimaryDNS = convertedSecondaryDNS = IPAddress(0,0,0,0);
     
  staticIP = convertedStaticIP;
  gateway = convertedGateway;
  subnet = convertedSubnet;
  primaryDNS = convertedPrimaryDNS;
  secondaryDNS = convertedSecondaryDNS;

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

void SettingsParser::updateIPConfig(IPAddress staticIP, IPAddress gateway, IPAddress subnet, IPAddress primaryDNS, IPAddress secondaryDNS){
  int startAdd = eepromAddress + ssidSize + passwordSize + botTokenSize + chatIdSize + userIdSize + mqttPasswordSize;

  writeIPToEEPROM(startAdd + IP4Size*0, staticIP);
  writeIPToEEPROM(startAdd + IP4Size*1, gateway);
  writeIPToEEPROM(startAdd + IP4Size*2, subnet);
  writeIPToEEPROM(startAdd + IP4Size*3, primaryDNS);
  writeIPToEEPROM(startAdd + IP4Size*4, secondaryDNS);

  this->staticIP = staticIP;
  this->gateway = gateway;
  this->subnet = subnet; 
  this->primaryDNS = primaryDNS;
  this->secondaryDNS = secondaryDNS;
  
}


//чтение из EEPROM

void SettingsParser::loadSettingsFromEEPROM(){
  ssid         = readStringFromEEPROM(eepromAddress);
  password     = readStringFromEEPROM(eepromAddress + ssidSize);
  botToken     = readStringFromEEPROM(eepromAddress + ssidSize + passwordSize);
  chatId       = readStringFromEEPROM(eepromAddress + ssidSize + passwordSize + botTokenSize);
  userId       = readStringFromEEPROM(eepromAddress + ssidSize + passwordSize + botTokenSize + chatIdSize);
  mqttPassword = readStringFromEEPROM(eepromAddress + ssidSize + passwordSize + botTokenSize + chatIdSize + userIdSize);

  int startAdd = eepromAddress + ssidSize + passwordSize + botTokenSize + chatIdSize + userIdSize + mqttPasswordSize;

  staticIP     = readIPFromEEPROM(startAdd);
  gateway      = readIPFromEEPROM(startAdd + IP4Size * 1);
  subnet       = readIPFromEEPROM(startAdd + IP4Size * 2);
  primaryDNS   = readIPFromEEPROM(startAdd + IP4Size * 3);
  secondaryDNS = readIPFromEEPROM(startAdd + IP4Size * 4);

}

void SettingsParser::writeSettingsToEEPROM(){
  writeStringToEEPROM(eepromAddress,                                                                    ssid);
  writeStringToEEPROM(eepromAddress + ssidSize,                                                         password);
  writeStringToEEPROM(eepromAddress + ssidSize + passwordSize,                                          botToken);
  writeStringToEEPROM(eepromAddress + ssidSize + passwordSize + botTokenSize,                           chatId);
  writeStringToEEPROM(eepromAddress + ssidSize + passwordSize + botTokenSize + chatIdSize,              userId);
  writeStringToEEPROM(eepromAddress + ssidSize + passwordSize + botTokenSize + chatIdSize + userIdSize, mqttPassword);

  int startAdd = eepromAddress + ssidSize + passwordSize + botTokenSize + chatIdSize + userIdSize + mqttPasswordSize;
  writeIPToEEPROM(startAdd + IP4Size*0, staticIP);
  writeIPToEEPROM(startAdd + IP4Size*1, gateway);
  writeIPToEEPROM(startAdd + IP4Size*2, subnet);
  writeIPToEEPROM(startAdd + IP4Size*3, primaryDNS);
  writeIPToEEPROM(startAdd + IP4Size*4, secondaryDNS);
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

void SettingsParser::writeIPToEEPROM(int eepromAddress,IPAddress ip){
  EEPROM.write(eepromAddress, ip[0]);
  EEPROM.write(eepromAddress + 1, ip[1]);
  EEPROM.write(eepromAddress + 2, ip[2]);
  EEPROM.write(eepromAddress + 3, ip[3]);

  EEPROM.commit();
}

IPAddress SettingsParser::readIPFromEEPROM(int eepromAddress){
  byte ip[4];
  ip[0] = EEPROM.read(eepromAddress);
  ip[1] = EEPROM.read(eepromAddress + 1);
  ip[2] = EEPROM.read(eepromAddress + 2);
  ip[3] = EEPROM.read(eepromAddress + 3);
  
  return IPAddress(ip[0], ip[1], ip[2], ip[3]);
}

int SettingsParser::getReservedSizeEEPROM(){
  return ssidSize + passwordSize + botTokenSize + chatIdSize + userIdSize + mqttPasswordSize + IP4Size * 5;
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

IPAddress SettingsParser::getStaticIP(){
  return staticIP;
}
IPAddress SettingsParser::getGateway(){
  return gateway;
}
IPAddress SettingsParser::getSubnet(){
  return subnet;
}
IPAddress SettingsParser::getPrimaryDNS(){
  return primaryDNS;
}
IPAddress SettingsParser::getSecondaryDNS(){
  return secondaryDNS;
}

/*
String SettingsParser::convertIPToStr(IPAddress ip){
  return ip.toString();
}
*/
bool SettingsParser::convertStrToIP(const String& ipStr, IPAddress& ip) {
    int parts[4] = {0};
    int partIndex = 0;
    
    size_t start = 0;
    size_t end = ipStr.indexOf('.');
    
    while (end != -1 && partIndex < 4) {
        parts[partIndex++] = ipStr.substring(start, end).toInt();
        start = end + 1;
        end = ipStr.indexOf('.', start);
    }
    
    if (partIndex < 3) return false;
    parts[partIndex] = ipStr.substring(start).toInt();
    
    for (int i = 0; i < 4; i++) {
        if (parts[i] < 0 || parts[i] > 255) return false;
    }
    
    ip = IPAddress(parts[0], parts[1], parts[2], parts[3]);
    return true;
}


const int SettingsParser::ssidSize = 27;
const int SettingsParser::passwordSize = 20;
const int SettingsParser::botTokenSize = 50;
const int SettingsParser::chatIdSize = 20;
const int SettingsParser::userIdSize = 20;
const int SettingsParser::mqttPasswordSize = 20;

const int SettingsParser::IP4Size = 4;



